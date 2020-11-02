/*
 * Configuration for Xilinx ZynqMP zcu102
 *
 * (C) Copyright 2015 Xilinx, Inc.
 * Michal Simek <michal.simek@xilinx.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __CONFIG_ADI_ZYNQMP_ADR9009_H
#define __CONFIG_ADI_ZYNQMP_ADR9009_H

#define CONFIG_ZYNQ_SDHCI1
#define CONFIG_SYS_I2C_MAX_HOPS		1
#define CONFIG_SYS_NUM_I2C_BUSES	18
#define CONFIG_SYS_I2C_BUSES	{ \
				{0, {I2C_NULL_HOP} }, \
				{0, {{I2C_MUX_PCA9544, 0x75, 0} } }, \
				{0, {{I2C_MUX_PCA9544, 0x75, 1} } }, \
				{0, {{I2C_MUX_PCA9544, 0x75, 2} } }, \
				{1, {I2C_NULL_HOP} }, \
				{1, {{I2C_MUX_PCA9548, 0x74, 0} } }, \
				{1, {{I2C_MUX_PCA9548, 0x74, 1} } }, \
				{1, {{I2C_MUX_PCA9548, 0x74, 2} } }, \
				{1, {{I2C_MUX_PCA9548, 0x74, 3} } }, \
				{1, {{I2C_MUX_PCA9548, 0x74, 4} } }, \
				{1, {{I2C_MUX_PCA9548, 0x75, 0} } }, \
				{1, {{I2C_MUX_PCA9548, 0x75, 1} } }, \
				{1, {{I2C_MUX_PCA9548, 0x75, 2} } }, \
				{1, {{I2C_MUX_PCA9548, 0x75, 3} } }, \
				{1, {{I2C_MUX_PCA9548, 0x75, 4} } }, \
				{1, {{I2C_MUX_PCA9548, 0x75, 5} } }, \
				{1, {{I2C_MUX_PCA9548, 0x75, 6} } }, \
				{1, {{I2C_MUX_PCA9548, 0x75, 7} } }, \
				}

#define CONFIG_SYS_I2C_ZYNQ
#define CONFIG_PCA953X

#define CONFIG_SYS_I2C_EEPROM_ADDR_LEN	1
#define CONFIG_ZYNQ_EEPROM_BUS		5
#define CONFIG_ZYNQ_GEM_EEPROM_ADDR	0x54
#define CONFIG_ZYNQ_GEM_I2C_MAC_OFFSET	0x20

/* Initial environment variables */
#define CONFIG_EXTRA_ENV_SETTINGS \
	"kernel_addr=0x80000\0" \
	"initrd_addr=0xa00000\0" \
	"initrd_size=0x2000000\0" \
	"fdt_addr=4000000\0" \
	"fdt_high=0x10000000\0" \
	"loadbootenv_addr=0x100000\0" \
	"sdbootdev=0\0"\
	"kernel_offset=0x280000\0" \
	"fdt_offset=0x200000\0" \
	"kernel_size=0x1e00000\0" \
	"fdt_size=0x80000\0" \
	"extraenv_offset=0x2000000\0" \
	"extraenv_load_address=0x2FE0000\0" \
	"bootenv=uEnv.txt\0" \
	"loadbootenv=load mmc $sdbootdev:$partid ${loadbootenv_addr} ${bootenv}\0" \
	"importbootenv=echo Importing environment from SD ...; " \
		"env import -t ${loadbootenv_addr} $filesize\0" \
	"sd_uEnvtxt_existence_test=test -e mmc $sdbootdev:$partid /uEnv.txt\0" \
	"qspi_extraenv=sf probe 0 0 0 && sf read ${extraenv_load_address} $extraenv_offset 0x20000 && " \
		"env import -c ${extraenv_load_address} 0x20000 || true \0" \
	"sata_root=if test $scsidevs -gt 0; then setenv bootargs $bootargs root=/dev/sda rw rootfstype=ext4; fi\0" \
	"sataboot=load scsi 0 80000 boot/Image && load scsi 0 $fdt_addr boot/system.dtb && booti 80000 - $fdt_addr\0" \
	"veloce=fdt addr f000000 && fdt resize" \
		"fdt set /amba/misc_clk clock-frequency <48000> && "\
		"fdt set /timer clock-frequency <240000> && " \
		"fdt set /amba/i2c_clk clock-frequency <240000> && " \
		"booti 80000 - f000000\0" \
	"netboot=tftpboot 10000000 image.ub && bootm\0" \
	"qspiboot=sf probe 0 0 0 && sf read $fdt_addr $fdt_offset $fdt_size && " \
		  "sf read $kernel_addr $kernel_offset $kernel_size && " \
		  "booti $kernel_addr - $fdt_addr\0" \
	"uenvboot=" \
		"if run sd_uEnvtxt_existence_test; then " \
			"run loadbootenv; " \
			"echo Loaded environment from ${bootenv}; " \
			"run importbootenv; " \
		"fi; " \
		"if test -n $uenvcmd; then " \
			"echo Running uenvcmd ...; " \
			"run uenvcmd; " \
		"fi\0" \
	"sdboot=mmc dev $sdbootdev && mmcinfo && run uenvboot || run qspi_extraenv || run sdroot$sdbootdev; " \
		"load mmc $sdbootdev:$partid $fdt_addr system.dtb && " \
		"load mmc $sdbootdev:$partid $kernel_addr Image && " \
		"booti $kernel_addr - $fdt_addr\0" \
	"emmcboot=run sdboot\0" \
	"nandboot=nand info && nand read $fdt_addr $fdt_offset $fdt_size && " \
		  "nand read $kernel_addr $kernel_offset $kernel_size && " \
		  "booti $kernel_addr - $fdt_addr\0" \
	"xen_prepare_dt=fdt addr $fdt_addr && fdt resize 128 && " \
		"fdt set /chosen \\\\#address-cells <1> && " \
		"fdt set /chosen \\\\#size-cells <1> && " \
		"fdt mknod /chosen dom0 && " \
		"fdt set /chosen/dom0 compatible \"xen,linux-zimage\" \"xen,multiboot-module\" && " \
		"fdt set /chosen/dom0 reg <0x80000 0x$filesize> && " \
		"fdt set /chosen xen,xen-bootargs \"console=dtuart dtuart=serial0 dom0_mem=768M bootscrub=0 maxcpus=1 timer_slop=0\" && " \
		"fdt set /chosen xen,dom0-bootargs \"console=hvc0 earlycon=xen earlyprintk=xen maxcpus=1 clk_ignore_unused\"\0" \
	"xen_prepare_dt_qemu=run xen_prepare_dt && " \
		"fdt set /cpus/cpu@1 device_type \"none\" && " \
		"fdt set /cpus/cpu@2 device_type \"none\" && " \
		"fdt set /cpus/cpu@3 device_type \"none\" && " \
		"fdt rm /cpus/cpu@1 compatible && " \
		"fdt rm /cpus/cpu@2 compatible && " \
		"fdt rm /cpus/cpu@3 compatible\0" \
	"xen=tftpb $fdt_addr system.dtb &&  tftpb 0x80000 Image &&" \
		"run xen_prepare_dt && " \
		"tftpb 6000000 xen.ub && tftpb 0x1000000 image.ub && " \
		"bootm 6000000 0x1000000 $fdt_addr\0" \
	"xen_qemu=tftpb $fdt_addr system.dtb && tftpb 0x80000 Image && " \
		"run xen_prepare_dt_qemu && " \
		"tftpb 6000000 xen.ub && tftpb 0x1000000 image.ub && " \
		"bootm 6000000 0x1000000 $fdt_addr\0" \
	"jtagboot=tftpboot 80000 Image && tftpboot $fdt_addr system.dtb && " \
		 "tftpboot 6000000 rootfs.cpio.ub && booti 80000 6000000 $fdt_addr\0" \
	"nosmp=setenv bootargs $bootargs maxcpus=1\0" \
	"nfsroot=setenv bootargs $bootargs root=/dev/nfs nfsroot=$serverip:/mnt/sata,tcp ip=$ipaddr:$serverip:$serverip:255.255.255.0:zynqmp:eth0:off rw\0" \
	"sdroot0=setenv bootargs $bootargs root=/dev/mmcblk0p2 rw rootwait\0" \
	"sdroot1=setenv bootargs $bootargs root=/dev/mmcblk1p2 rw rootwait\0" \
	"android=setenv bootargs $bootargs init=/init androidboot.selinux=disabled androidboot.hardware=$board\0" \
	"android_debug=run android && setenv bootargs $bootargs video=DP-1:1024x768@60 drm.debug=0xf\0" \
	"usb_dfu_spl=booti $kernel_addr - $fdt_addr\0" \
	"usbhostboot=usb start && load usb 0 $fdt_addr system.dtb && " \
		     "load usb 0 $kernel_addr Image && " \
		     "booti $kernel_addr - $fdt_addr\0" \
	PARTS_DEFAULT \
	DFU_ALT_INFO

#include <configs/xilinx_zynqmp.h>

#endif /* __CONFIG_ADI_ZYNQMP_ADR9009_H */
