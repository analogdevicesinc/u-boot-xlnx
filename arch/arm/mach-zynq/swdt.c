/*
 * Zynq system watchdog armed for the boot of the operating system
 *
 * Copyright (c) 2026 Analog Devices Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <bootm.h>
#include <asm/io.h>
#include <asm/arch/clk.h>
#include <asm/arch/hardware.h>
#include <asm/arch/sys_proto.h>
#include <linux/bitops.h>
#include <linux/kernel.h>

#define SWDT_ZMR_WDEN		BIT(0)
#define SWDT_ZMR_RSTEN		BIT(1)
#define SWDT_ZMR_RSTLEN_16	(3 << 4)
#define SWDT_ZMR_ZKEY		(0xabc << 12)

#define SWDT_CCR_CLKSEL_4096	3
#define SWDT_CCR_CRV_SHIFT	2
#define SWDT_CCR_CRV_MAX	0xfff
#define SWDT_CCR_CKEY		(0x248 << 14)

#define SWDT_RESTART_KEY	0x1999

#define SWDT_PRESCALE		4096
/* The counter restarts from the CRV field followed by 12 one bits */
#define SWDT_CRV_STEP		4096

#define SLCR_WDT_CLK_SEL_CPU_1X	0

void arch_preboot_os(void)
{
	ulong ticks_per_sec = zynq_clk_get_rate(cpu_1x_clk) / SWDT_PRESCALE;
	ulong crv;

	crv = DIV_ROUND_UP(CONFIG_ZYNQ_BOOT_WATCHDOG_TIMEOUT * ticks_per_sec,
			   SWDT_CRV_STEP) - 1;
	crv = min_t(ulong, crv, SWDT_CCR_CRV_MAX);

	zynq_slcr_unlock();
	writel(SLCR_WDT_CLK_SEL_CPU_1X, &slcr_base->wdt_clk_sel);
	zynq_slcr_lock();

	writel(SWDT_ZMR_ZKEY, &swdt_base->zmr);
	writel(SWDT_CCR_CKEY | crv << SWDT_CCR_CRV_SHIFT | SWDT_CCR_CLKSEL_4096,
	       &swdt_base->ccr);
	writel(SWDT_ZMR_ZKEY | SWDT_ZMR_RSTLEN_16 | SWDT_ZMR_RSTEN |
	       SWDT_ZMR_WDEN, &swdt_base->zmr);
	writel(SWDT_RESTART_KEY, &swdt_base->restart);

	printf("Watchdog: %lu s\n",
	       (crv + 1) * SWDT_CRV_STEP / ticks_per_sec);
}
