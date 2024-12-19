// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2024 Analog Devices Inc.
 *
 * Eliza Balas <eliza.balas@analog.com>
 *
 */

/*
 * Configuration support for SelectMap based on ADI axi_selmap
 * FPGA IP Core for programming external fpga.
 */

#define LOG_CATEGORY UCLASS_FPGA

#include <asm-generic/gpio.h>
#include <asm/io.h>
#include <common.h>
#include <dm.h>
#include <dm/device.h>
#include <dm/read.h>
#include <dm/uclass.h>
#include <linux/delay.h>
#include <xilinx.h>

#define ADI_SELMAP_MAGIC                        0x73656C6D /* selm */

/* Device registers */
#define ADI_SELMAP_MAGIC_REG                    0x0 /* Core Magic reg */
#define ADI_SELMAP_SCRATCH_REG                  0x4 /* Scratch reg */
#define ADI_SELMAP_RESET_REG                    0x8 /* Reset reg */
#define ADI_SELMAP_PROGRAM_B_REG                0xC /* Program_b reg */
#define ADI_SELMAP_DEVICE_READY_REG             0x10 /* Device_ready reg */
#define ADI_SELMAP_CSI_B_REG                    0x14 /* Csi_b reg */
#define ADI_SELMAP_DATA_REG                     0x18 /* Data reg */
#define ADI_SELMAP_BYTE_COUNTER_REG             0x1C /* Byte_counter reg */
#define ADI_SELMAP_DONE_REG                     0x20 /* Done reg */

enum adi_selmap_dev_ids {
	ID_ADI_8_SELMAP,
	ID_ADI_16_SELMAP,
	ID_ADI_32_SELMAP,
};

struct adi_selmap_pdata {
	phys_addr_t reg_base;
	struct gpio_desc prog_b;
	struct gpio_desc init_b;
	struct gpio_desc csi_b;
	struct gpio_desc rdwr_b;
	struct gpio_desc done;
	struct gpio_desc mode[3];
};

struct adi_selmap_priv {
	struct udevice *dev;
	struct adi_selmap_pdata *pdata;
	enum adi_selmap_dev_ids id;
};

static struct adi_selmap_priv *adi_selmap_priv;

static void adi_selmap_set_priv(struct adi_selmap_priv *priv) {
	adi_selmap_priv = priv;
}

static struct adi_selmap_priv *adi_selmap_get_priv(void) {
	return adi_selmap_priv;
}

static int adi_selmap_probe(struct udevice *dev)
{
	struct adi_selmap_priv *priv = dev_get_priv(dev);
	u32 magic_value;

	priv->id = dev_get_driver_data(dev);
	priv->dev = dev;

	// Read the MAGIC value from the specified address
	magic_value = readl(priv->pdata->reg_base + ADI_SELMAP_MAGIC_REG);

	if (magic_value != ADI_SELMAP_MAGIC) {
		printf("%s: %s: Failed to probe adi-selmap. Invalid magic\n",
		       __FILE__,__func__);
		return -EINVAL;
	}

	adi_selmap_set_priv(priv);

	printf("%s: %s: Probed adi-selmap successfuly; SelectMap mode: %d-bit\n",
	       __FILE__,__func__,
	       (priv->id == ID_ADI_8_SELMAP) ? 8 : (priv->id == ID_ADI_16_SELMAP) ? 16 : 32);

	return 0;
}

static int adi_selmap_of_to_plat(struct udevice *dev)
{
	struct adi_selmap_pdata *pdata = dev_get_plat(dev);
	struct adi_selmap_priv *priv = dev_get_priv(dev);
	int ret;

	pdata->reg_base = (phys_addr_t)dev_read_addr(dev);
	if (!pdata->reg_base) {
		printf("%s: %s: Failed to get base reg address\n",
		       __FILE__,__func__);
		return -EINVAL;
	}

	if (!CONFIG_IS_ENABLED(DM_GPIO)) {
		printf("%s: %s: DM_GPIO is not enabled. Enable it\n",
		       __FILE__,__func__);
		return -EINVAL;
	}

	/* search "prog-gpios" in device node */
	ret = gpio_request_by_name(dev, "prog-gpios", 0,
				   &pdata->prog_b,
				   GPIOD_IS_OUT| GPIOD_ACTIVE_LOW);
	if (ret) {
		printf("%s: %s: Failed to get PROGRAM_B gpio\n",
		       __FILE__,__func__);
		return ret;
	}

	/* search "init-gpios" in device node */
	ret = gpio_request_by_name(dev, "init-gpios", 0,
				   &pdata->init_b,
				   GPIOD_IS_IN);
	if (ret) {
		printf("%s: %s: Failed to get INIT_B gpio\n",
		       __FILE__,__func__);
		return ret;
	}

	/* search "done-gpios" in device node */
	ret = gpio_request_by_name(dev, "done-gpios", 0,
				   &pdata->done,
				   GPIOD_IS_IN);
	if (ret) {
		printf("%s: %s: Failed to get DONE gpio\n",
		       __FILE__,__func__);
		return ret;
	}

	/* search "csi-gpios" in device node */
	ret = gpio_request_by_name(dev, "csi-gpios", 0,
				   &pdata->csi_b,
				   GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);
	if (ret) {
		printf("%s: %s: Failed to get CSI_B gpio\n",
		       __FILE__,__func__);
		return ret;
	}

	/* search "rdwr-gpios" in device node */
	ret = gpio_request_by_name(dev, "rdwr-gpios", 0,
				   &pdata->rdwr_b,
				   GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);
	if (ret) {
		printf("%s: %s: Failed to get RDWR_B gpio\n",
		       __FILE__,__func__);
		return ret;
	}

	/* search mode[2] gpio in device node */
	ret = gpio_request_by_name(dev, "mode-gpios", 2,
				   &pdata->mode[2],
				   GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);
	if (ret) {
		printf("%s: %s: Failed to get MODE PIN [2] gpio\n",
		       __FILE__,__func__);
		return ret;
	}

	/* search mode[1] gpio in device node */
	ret = gpio_request_by_name(dev, "mode-gpios", 1,
				   &pdata->mode[1],
				   GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);
	if (ret) {
		printf("%s: %s: Failed to get MODE PIN [1] gpio\n",
		       __FILE__,__func__);
		return ret;
	}

	/* search mode[0] gpio in device node */
	ret = gpio_request_by_name(dev, "mode-gpios", 0,
				   &pdata->mode[0],
				   GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);
	if (ret) {
		printf("%s: %s: Failed to get MODE PIN [0] gpio\n",
		       __FILE__,__func__);
		return ret;
	}

	priv->pdata = pdata;

	return 0;
}

static const struct udevice_id adi_selmap_ids[] = {
	{ .compatible = "adi,fpga-8-selectmap", .data = (ulong)ID_ADI_8_SELMAP, }, // ADI 8bit version
	{ .compatible = "adi,fpga-16-selectmap", .data = (ulong)ID_ADI_16_SELMAP, }, // ADI 16bit version
	{ .compatible = "adi,fpga-32-selectmap", .data = (ulong)ID_ADI_32_SELMAP, }, // ADI 32bit version
	{ }
};

U_BOOT_DRIVER(adi_selmap) = {
	.name = "adi-selmap",
	.id = UCLASS_MISC,
	.of_match = adi_selmap_ids,
	.of_to_plat = adi_selmap_of_to_plat,
	.probe = adi_selmap_probe,
	.priv_auto = sizeof(struct adi_selmap_priv),
	.plat_auto = sizeof(struct adi_selmap_pdata),
	.flags = DM_FLAG_PROBE_AFTER_BIND,
};

static int wait_for_init_b(struct adi_selmap_priv * priv, int init_b_val)
{
	unsigned long timeout = get_timer(0) + 1000; /* 1 sec from now*/

	while (time_before(get_timer(0), timeout)) {
		int ret = dm_gpio_get_value(&priv->pdata->init_b);

		if (ret == init_b_val)
			return 0;

		if (ret < 0) {
			printf("%s: %s: Error reading INIT_B (%d)\n",
			       __FILE__,__func__, ret);
			return ret;
		}

		udelay(100);
	}

	printf("%s: %s: Timeout waiting for INIT_B to %s\n",
	       __FILE__,__func__,
	       init_b_val ? "assert" : "deassert");

	return -ETIMEDOUT;
}

static int adi_selmap_write_init(struct adi_selmap_priv * priv, bitstream_type bstype)
{
	int ret;

	if (bstype == BIT_PARTIAL) {
		printf("%s: %s: SelectMap partial reconfiguration not supported\n",
		       __FILE__,__func__);
		return -EINVAL;
	}

	if (!CONFIG_IS_ENABLED(DM_GPIO))
		return -EINVAL;

	if (dm_gpio_is_valid(&priv->pdata->mode[2]) &&
	    dm_gpio_is_valid(&priv->pdata->mode[1]) &&
	    dm_gpio_is_valid(&priv->pdata->mode[0] )) {

		printf("%s: %s: Setting FPGA Configuration mode to SelectMap (mode[2:0] = 110)\n",
		       __FILE__, __func__);
		dm_gpio_set_value(&priv->pdata->mode[2], 1);
		dm_gpio_set_value(&priv->pdata->mode[1], 1);
		dm_gpio_set_value(&priv->pdata->mode[0], 0);
	}

	dm_gpio_set_value(&priv->pdata->prog_b, 1);

	ret = wait_for_init_b(priv, 1); /* min is 500 ns */
	if (ret) {
		dm_gpio_set_value(&priv->pdata->prog_b, 0);
		goto end;
	}

	dm_gpio_set_value(&priv->pdata->prog_b, 0);

	ret = wait_for_init_b(priv, 0);
	if (ret)
		goto end;

	if (dm_gpio_get_value(&priv->pdata->done)) {
		printf("%s: %s: Unexpected DONE pin state...\n",
		       __FILE__, __func__);
		ret = -EIO;
		goto end;
	}

	/* program latency */
	udelay(7500);
	return 0;

end:
	printf("%s: %s: Setting FPGA Configuration mode back to JTAG (mode[2:0] = 000)\n",
	       __FILE__, __func__);
	dm_gpio_set_value(&priv->pdata->mode[2], 0);
	dm_gpio_set_value(&priv->pdata->mode[1], 0);
	dm_gpio_set_value(&priv->pdata->mode[0], 0);

	return ret;
}

static int adi_selmap_write(struct adi_selmap_priv * priv,
				  const char *buf, size_t count)
{
	u16 *buf16 = (u16 *)buf;
	u32 *buf32 = (u32 *)buf;
	size_t i;

	switch (priv->id) {
	case ID_ADI_8_SELMAP:
		for (i = 0; i < count; ++i)
			writeb(buf[i],
			       priv->pdata->reg_base + ADI_SELMAP_DATA_REG);
		break;
	case ID_ADI_16_SELMAP:
		for (i = 0; i < (count/2)+1; ++i)
			writew(cpu_to_be16(buf16[i]),
			       priv->pdata->reg_base + ADI_SELMAP_DATA_REG);
		break;
	case ID_ADI_32_SELMAP:
		for (i = 0; i < (count/4)+1; ++i)
			writel(cpu_to_be32(buf32[i]),
			       priv->pdata->reg_base + ADI_SELMAP_DATA_REG);
		break;
	default:
		printf("%s: %s: Unsupported id, %d",
		       __FILE__, __func__, priv->id);
	}

	return 0;
}

static int adi_selmap_write_complete(struct adi_selmap_priv * priv)
{
	unsigned long timeout = get_timer(0) + 200; /* 200 ms */
	const char padding[1] = { 0xff };
	bool expired = false;
	int done;
	int ret;

	if (!CONFIG_IS_ENABLED(DM_GPIO)) {
		ret = -EINVAL;
		goto end;
	}

	/*
	 * This loop is carefully written such that if the driver is
	 * scheduled out for more than 'timeout', we still check for DONE
	 * before giving up and we apply 8 extra CCLK cycles in all cases.
	 */
	while (!expired) {
		expired = time_after(get_timer(0), timeout);

		done = dm_gpio_get_value(&priv->pdata->done);
		if (done < 0) {
			ret = done;
			goto end;
		}

		ret = adi_selmap_write(priv, padding, sizeof(padding));
		if (ret)
			goto end;

		if (done) {
			ret = 0;
			goto end;
		}
	}

	ret = dm_gpio_get_value(&priv->pdata->init_b);
	if (ret < 0) {
		printf("%s: %s: Error reading INIT_B (%d)\n",
		       __FILE__, __func__, ret);
		goto end;
	}

	printf("%s: %s: %s",
	       ret ? "CRC error or invalid device\n" :
	       "Missing sync word or incomplete bitstream\n",
	       __FILE__, __func__);
	ret = -ETIMEDOUT;

end:
	printf("%s: %s: Setting FPGA Configuration mode back to JTAG (mode[2:0] = 000)\n",
	       __FILE__, __func__);
	dm_gpio_set_value(&priv->pdata->mode[2], 0);
	dm_gpio_set_value(&priv->pdata->mode[1], 0);
	dm_gpio_set_value(&priv->pdata->mode[0], 0);

	return ret;
}

static int adi_selmap_load(xilinx_desc *desc, const void *buf, size_t bsize,
			bitstream_type bstype, int flags)
{
	struct adi_selmap_priv * priv= adi_selmap_get_priv();
	int ret_val = FPGA_FAIL;
	unsigned long start, stop;

	if (desc->iface == slave_selectmap) {
		start = get_timer(0)/1000;

		printf("%s: %s: Launching SelectMap Load...\n", __FILE__, __func__);

		ret_val = adi_selmap_write_init(priv, bstype);
		if (ret_val)
			return FPGA_FAIL;

		printf("%s: %s: Programming...\n", __FILE__, __func__);

		ret_val = adi_selmap_write(priv, buf, bsize);
		if (ret_val)
			return FPGA_FAIL;

		ret_val = adi_selmap_write_complete(priv);
		stop = get_timer(0)/1000;
		if (!ret_val)
			printf("%s: %s: Finished SelectMap Load. Time elapsed: %lu seconds\n",
			       __FILE__, __func__, stop-start);
	}

	return ret_val;
}

static int adi_selmap_dump(xilinx_desc *desc, const void *buf, size_t bsize)
{
	printf("%s: %s: SelectMap Dumping is unsupported\n", __FILE__, __func__);

	return FPGA_FAIL;
}

static int adi_selmap_info(xilinx_desc *desc)
{
	struct adi_selmap_priv * priv= adi_selmap_get_priv();

	if (!CONFIG_IS_ENABLED(DM_GPIO))
		return -EINVAL;

	printf("External FPGA - Select Map:\n\n");
	printf("\tSlave SelectMAP bus width: %d-bit\n\n",
	(priv->id == ID_ADI_8_SELMAP) ? 8 : (priv->id == ID_ADI_16_SELMAP) ? 16 : 32);

	printf("\tGPIO pins status:\n");
	printf("\t\t\t boot mode[2:0] = %d%d%d\n",
		dm_gpio_get_value(&priv->pdata->mode[2]),
		dm_gpio_get_value(&priv->pdata->mode[1]),
		dm_gpio_get_value(&priv->pdata->mode[0]));
	printf("\t\t\t prog_b = %d\n", dm_gpio_get_value(&priv->pdata->prog_b));
	printf("\t\t\t init_b = %d\n", dm_gpio_get_value(&priv->pdata->init_b));
	printf("\t\t\t done = %d\n", dm_gpio_get_value(&priv->pdata->done));
	printf("\t\t\t csi_b = %d\n", dm_gpio_get_value(&priv->pdata->csi_b));
	printf("\t\t\t rdwr_b = %d\n", dm_gpio_get_value(&priv->pdata->rdwr_b));

	return FPGA_SUCCESS;
}

struct xilinx_fpga_op adi_selmap_fpga_op = {
	.load = adi_selmap_load,
	.dump = adi_selmap_dump,
	.info = adi_selmap_info,
};
