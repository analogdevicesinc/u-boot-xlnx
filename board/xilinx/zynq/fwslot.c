/*
 * A/B firmware slot selection for Zynq boards booting from QSPI flash
 *
 * Copyright (c) 2026 Analog Devices Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <command.h>
#include <environment.h>
#include <errno.h>
#include <image.h>
#include <libfdt.h>
#include <mapmem.h>
#include <spi.h>
#include <spi_flash.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>
#include <asm/arch/sys_proto.h>
#include <linux/bitops.h>
#include <linux/sizes.h>

#include <abr.h>
#include <abr_handoff.h>

#define FWSLOT_ENV_VAR		"fwslot"

/* MULTIBOOT_ADDR holds the boot image offset in units of 32 KiB */
#define MULTIBOOT_ADDR_MASK	GENMASK(12, 0)
#define MULTIBOOT_ADDR_UNIT	SZ_32K

struct fwslot {
	const char *name;
	unsigned int abr_slot;
	u32 boot_offset;
	u32 app_offset;
};

static const struct fwslot fwslots[] = {
	{
		.name = "a",
		.abr_slot = ABR_SLOT_A,
		.boot_offset = ZYNQ_FWSLOT_A_BOOT_OFFSET,
		.app_offset = ZYNQ_FWSLOT_A_APP_OFFSET,
	}, {
		.name = "b",
		.abr_slot = ABR_SLOT_B,
		.boot_offset = ZYNQ_FWSLOT_B_BOOT_OFFSET,
		.app_offset = ZYNQ_FWSLOT_B_APP_OFFSET,
	},
};

#define FWSLOT_DEFAULT		(&fwslots[0])

/* Slot the boot selector started, NULL when not started by it */
static const struct fwslot *fwslot_booted;
/* The selector recorded this boot's attempt in the boot record */
static bool fwslot_recorded;

static char * const fwslot_builtin_env_vars[] = {
	ZYNQ_FWSLOT_BUILTIN_ENV_VARS
};

static const struct fwslot *fwslot_find(const char *name)
{
	unsigned int i;

	if (!name)
		return NULL;

	for (i = 0; i < ARRAY_SIZE(fwslots); i++)
		if (!strcmp(name, fwslots[i].name))
			return &fwslots[i];

	return NULL;
}

static const struct fwslot *fwslot_find_by_multiboot(u32 multiboot)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(fwslots); i++)
		if (multiboot == fwslots[i].boot_offset / MULTIBOOT_ADDR_UNIT)
			return &fwslots[i];

	return NULL;
}

static const struct fwslot *fwslot_get(void)
{
	const struct fwslot *slot = fwslot_find(getenv(FWSLOT_ENV_VAR));

	return slot ? slot : FWSLOT_DEFAULT;
}

static const struct fwslot *fwslot_other(const struct fwslot *slot)
{
	return slot == &fwslots[0] ? &fwslots[1] : &fwslots[0];
}

/*
 * MULTIBOOT_ADDR and the selector's hand-off survive warm resets. Take and
 * clear both, so that the next reset starts again from the boot selector
 * and finds no hand-off that belongs to this boot.
 */
static u32 fwslot_take_multiboot(u32 *handoff)
{
	u32 multiboot = readl(&devcfg_base->multiboot_addr) &
			MULTIBOOT_ADDR_MASK;
	u32 status;

	writel(0, &devcfg_base->multiboot_addr);

	zynq_slcr_unlock();
	status = readl(&slcr_base->reboot_status);
	writel(status & ~ABR_HANDOFF_MASK, &slcr_base->reboot_status);
	zynq_slcr_lock();

	*handoff = (status & ABR_HANDOFF_MASK) >> ABR_HANDOFF_SHIFT;

	return multiboot;
}

void zynq_fwslot_init(void)
{
	const struct fwslot *slot;
	u32 multiboot, handoff;

	if (!set_default_vars(ARRAY_SIZE(fwslot_builtin_env_vars),
			      fwslot_builtin_env_vars))
		printf("fwslot: restoring the built-in boot environment failed\n");

	multiboot = fwslot_take_multiboot(&handoff);
	fwslot_booted = fwslot_find_by_multiboot(multiboot);
	fwslot_recorded = fwslot_booted && handoff == ABR_HANDOFF_RECORDED;
	slot = fwslot_booted;
	if (slot) {
		printf("FW slot: %s (MULTIBOOT_ADDR 0x%x, attempt %srecorded)\n",
		       slot->name, multiboot, fwslot_recorded ? "" : "not ");
	} else {
		slot = FWSLOT_DEFAULT;
		printf("FW slot: %s (default, MULTIBOOT_ADDR 0x%x names no slot)\n",
		       slot->name, multiboot);
	}

	setenv(FWSLOT_ENV_VAR, slot->name);
}

static int fwslot_probe_flash(struct udevice **flash)
{
	int ret;

	ret = spi_flash_probe_bus_cs(CONFIG_SF_DEFAULT_BUS,
				     CONFIG_SF_DEFAULT_CS,
				     CONFIG_SF_DEFAULT_SPEED,
				     CONFIG_SF_DEFAULT_MODE, flash);
	if (ret)
		printf("fwslot: SPI flash probe failed (%d)\n", ret);

	return ret;
}

/* Return the size of the FIT image in a slot, or a negative error code */
static int fwslot_fit_size(struct udevice *flash, const struct fwslot *slot)
{
	struct fdt_header header;
	u32 size;
	int ret;

	ret = spi_flash_read_dm(flash, slot->app_offset, sizeof(header),
				&header);
	if (ret) {
		printf("fwslot: reading slot %s failed (%d)\n", slot->name,
		       ret);
		return ret;
	}

	size = fdt_totalsize(&header);
	if (fdt_check_header(&header) || size < sizeof(header) ||
	    size > ZYNQ_FWSLOT_APP_SIZE) {
		printf("fwslot: no valid FIT image in slot %s\n", slot->name);
		return -ENOEXEC;
	}

	return size;
}

static int fwslot_load_fit(const struct fwslot *slot, ulong addr)
{
	struct udevice *flash;
	bool valid;
	void *buf;
	int size;
	int ret;

	ret = fwslot_probe_flash(&flash);
	if (ret)
		return ret;

	size = fwslot_fit_size(flash, slot);
	if (size < 0)
		return size;

	buf = map_sysmem(addr, size);
	ret = spi_flash_read_dm(flash, slot->app_offset, size, buf);
	valid = !ret && fit_check_format(buf);
	unmap_sysmem(buf);

	if (ret) {
		printf("fwslot: reading slot %s failed (%d)\n", slot->name,
		       ret);
		return ret;
	}

	if (!valid) {
		printf("fwslot: no valid FIT image in slot %s\n", slot->name);
		return -ENOEXEC;
	}

	setenv_hex("filesize", size);

	return 0;
}

static int fwslot_abr_read(void *ctx, uint32_t offset, void *buf,
			   uint32_t len)
{
	return spi_flash_read_dm(ctx, offset, len, buf);
}

static const struct abr_flash_ops fwslot_abr_ops = {
	.read = fwslot_abr_read,
};

/*
 * Reset into the boot selector only when the boot record library says the
 * selector will start the other slot and that slot holds a FIT image.
 */
static int fwslot_fallback(void)
{
	const struct fwslot *other;
	struct abr_status st;
	struct abr_dev dev;
	struct udevice *flash;
	int ret;

	if (!fwslot_recorded) {
		printf("fwslot: this boot's attempt was not recorded\n");
		return -ESRCH;
	}
	other = fwslot_other(fwslot_booted);

	ret = fwslot_probe_flash(&flash);
	if (ret)
		return ret;

	dev.ops = &fwslot_abr_ops;
	dev.ctx = flash;
	dev.base = ZYNQ_FWSLOT_BOOTREC_OFFSET;
	dev.copy_size = ZYNQ_FWSLOT_BOOTREC_SIZE / ABR_NUM_COPIES;

	ret = abr_read_status(&dev, &st);
	if (ret != ABR_OK) {
		printf("fwslot: boot record: %s\n", abr_strerror(ret));
		return -EIO;
	}

	if (!abr_fallback_allowed(&st, fwslot_booted->abr_slot,
				  fwslot_recorded)) {
		printf("fwslot: the boot selector will not start slot %s\n",
		       other->name);
		return -ENOENT;
	}

	ret = fwslot_fit_size(flash, other);

	return ret < 0 ? ret : 0;
}

static int do_fwslot(cmd_tbl_t *cmdtp, int flag, int argc,
		     char * const argv[])
{
	ulong addr;
	int ret;

	if (argc == 3 && !strcmp(argv[1], "load")) {
		addr = simple_strtoul(argv[2], NULL, 16);
		ret = fwslot_load_fit(fwslot_get(), addr);
	} else if (argc == 2 && !strcmp(argv[1], "fallback")) {
		ret = fwslot_fallback();
	} else {
		return CMD_RET_USAGE;
	}

	return ret ? CMD_RET_FAILURE : CMD_RET_SUCCESS;
}

U_BOOT_CMD(
	fwslot, 3, 0, do_fwslot,
	"A/B firmware slot handling",
	"load <addr> - load the FIT image of slot $fwslot from SPI flash to <addr>\n"
	"fwslot fallback - succeed if a reset makes the boot selector start\n"
	"    the other slot"
);
