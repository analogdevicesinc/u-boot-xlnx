/*
 * Configuration settings for the Sharkbyte board
 * Based on zynq_zc70x.h - See zynq-common.h for Zynq common configs
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __CONFIG_ZYNQ_SHARKBYTE_H
#define __CONFIG_ZYNQ_SHARKBYTE_H

#include <linux/stringify.h>

#define CONFIG_ZYNQ_I2C0
#define CONFIG_ZYNQ_EEPROM
#define CONFIG_DFU_SF

/* QSPI flash layout */
#define SHARKBYTE_GOLDEN_OFFSET			0x0
#define SHARKBYTE_GOLDEN_SIZE			0x40000
#define ZYNQ_FWSLOT_BOOTREC_OFFSET		0x40000
#define ZYNQ_FWSLOT_BOOTREC_SIZE		0x20000
#define SHARKBYTE_EXTRA_ENV_OFFSET		0xFF000
#define SHARKBYTE_EXTRA_ENV_SIZE		0x1000
#define SHARKBYTE_ENV_OFFSET			0x100000
#define SHARKBYTE_ENV_SIZE			0x20000
#define ZYNQ_FWSLOT_A_BOOT_OFFSET		0x200000
#define ZYNQ_FWSLOT_B_BOOT_OFFSET		0x300000
#define ZYNQ_FWSLOT_BOOT_SIZE			0x100000
#define ZYNQ_FWSLOT_A_APP_OFFSET		0x400000
#define ZYNQ_FWSLOT_B_APP_OFFSET		0x2000000
#define ZYNQ_FWSLOT_APP_SIZE			0x1C00000

/*
 * The variables that make up the boot flow are always taken from the
 * default environment of the running U-Boot, never from a stored one.
 */
#define ZYNQ_FWSLOT_BUILTIN_ENV_VARS \
	"bootcmd", \
	"preboot", \
	"fit_load_address", \
	"extraenv_load_address", \
	"qspiboot_extraenv", \
	"read_sf", \
	"boot_fit", \
	"qspiboot", \
	"qspiboot_verbose", \
	"qspiboot_failed", \
	"ramboot_verbose", \
	"jtagboot", \
	"dfu_sf", \
	"dfu_sf_info", \
	"dfu_ram", \
	"dfu_ram_info", \
	"thor_ram"

#define SHARKBYTE_DFU_ALT(name, offset, size) \
	name " raw " __stringify(offset) " " __stringify(size)

#define SHARKBYTE_DFU_ALT_INFO_SF \
	SHARKBYTE_DFU_ALT("golden.dfu", SHARKBYTE_GOLDEN_OFFSET, \
			  SHARKBYTE_GOLDEN_SIZE) "\\\\;" \
	SHARKBYTE_DFU_ALT("bootrec.dfu", ZYNQ_FWSLOT_BOOTREC_OFFSET, \
			  ZYNQ_FWSLOT_BOOTREC_SIZE) "\\\\;" \
	SHARKBYTE_DFU_ALT("uboot-extra-env.dfu", SHARKBYTE_EXTRA_ENV_OFFSET, \
			  SHARKBYTE_EXTRA_ENV_SIZE) "\\\\;" \
	SHARKBYTE_DFU_ALT("uboot-env.dfu", SHARKBYTE_ENV_OFFSET, \
			  SHARKBYTE_ENV_SIZE) "\\\\;" \
	SHARKBYTE_DFU_ALT("boot-a.dfu", ZYNQ_FWSLOT_A_BOOT_OFFSET, \
			  ZYNQ_FWSLOT_BOOT_SIZE) "\\\\;" \
	SHARKBYTE_DFU_ALT("boot-b.dfu", ZYNQ_FWSLOT_B_BOOT_OFFSET, \
			  ZYNQ_FWSLOT_BOOT_SIZE) "\\\\;" \
	SHARKBYTE_DFU_ALT("app-a.dfu", ZYNQ_FWSLOT_A_APP_OFFSET, \
			  ZYNQ_FWSLOT_APP_SIZE) "\\\\;" \
	SHARKBYTE_DFU_ALT("app-b.dfu", ZYNQ_FWSLOT_B_APP_OFFSET, \
			  ZYNQ_FWSLOT_APP_SIZE)

#define SHARKBYTE_BOOTARGS(log_args) \
	"console=ttyPS0,115200 maxcpus=${maxcpus} rootfstype=ramfs " \
	"root=/dev/ram0 rw " log_args " clk_ignore_unused panic=5 " \
	"uboot=\"${uboot-version}\""

#define CONFIG_EXTRA_ENV_SETTINGS \
	"ethaddr=00:0a:35:00:01:22\0" \
	"ipaddr=192.168.2.1\0" \
	"ipaddr_host=192.168.2.10\0" \
	"netmask=255.255.255.0\0" \
	"fit_load_address=0x2080000\0" \
	"extraenv_load_address=0x207E000\0" \
	"fdt_high=0x20000000\0" \
	"initrd_high=0x20000000\0" \
	"maxcpus=1\0" \
	"qspiboot_extraenv=" \
		"sf read ${extraenv_load_address} 0xFF000 0x1000 && " \
		"env import -c ${extraenv_load_address} 0x1000 || true \0" \
	"read_sf=sf probe 0:0 50000000 0 && run qspiboot_extraenv &&" \
		"fwslot load ${fit_load_address}\0" \
	"boot_fit=if test -n \"${fit_config}\"; then " \
			"bootm ${fit_load_address}#${fit_config}; " \
		"else " \
			"bootm ${fit_load_address}; " \
		"fi\0" \
	"ramboot_verbose=echo Copying Linux from DFU to RAM... && " \
		"run dfu_ram; " \
		"envversion;setenv bootargs " \
		SHARKBYTE_BOOTARGS("earlyprintk") " && run boot_fit\0" \
	"qspiboot_verbose=echo Copying Linux from QSPI flash to RAM... && " \
		"run read_sf && " \
		"envversion && setenv bootargs " \
		SHARKBYTE_BOOTARGS("earlyprintk") " fwslot=${fwslot} && " \
		"run boot_fit || run qspiboot_failed\0" \
	"qspiboot=echo Booting silently && set stdout nulldev; " \
		"run read_sf && " \
		"envversion && setenv bootargs " \
		SHARKBYTE_BOOTARGS("quiet loglevel=4") " fwslot=${fwslot} && " \
		"run boot_fit || set stdout serial@e0001000;" \
		"run qspiboot_failed\0" \
	"qspiboot_failed=if fwslot fallback; then " \
			"echo Resetting to the boot selector ...; reset; " \
		"fi; " \
		"echo BOOT failed entering DFU mode ... && run dfu_sf\0" \
	"jtagboot=env default -a;sf probe && sf protect unlock 0 100000 && " \
		"run dfu_sf; \0" \
	DFU_ALT_INFO \
	"dfu_sf_info=" \
		"set dfu_alt_info " SHARKBYTE_DFU_ALT_INFO_SF "\0" \
	"dfu_sf=gpio set 15;set stdout serial@e0001000;" \
		"echo Entering DFU SF mode ... && run dfu_sf_info && " \
		"dfu 0 sf 0:0:40000000:0;gpio clear 15;\0"

#include <configs/zynq-common.h>

/* Sharkbyte has no reset button; MIO14 is used by eMMC (SD1 DAT2).
 * Disable misc_init_r to prevent false "button pressed" env reset. */
#undef CONFIG_MISC_INIT_R

#endif /* __CONFIG_ZYNQ_SHARKBYTE_H */
