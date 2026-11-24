/* SPDX-License-Identifier: GPL-2.0-or-later OR BSD-3-Clause */
/*
 * A/B boot record: boot selector to U-Boot hand-off
 *
 * The selector tells the slot's U-Boot whether it recorded the boot's
 * attempt in bits 27:24 of the Zynq-7000 SLCR REBOOT_STATUS register,
 * which survive the soft reset. The selector clears the field at entry
 * and writes it before its soft reset into the slot; U-Boot reads it
 * together with MULTIBOOT_ADDR and clears it.
 *
 * Copyright (C) 2026 Analog Devices, Inc.
 */
#ifndef ABR_HANDOFF_H
#define ABR_HANDOFF_H

#define ABR_HANDOFF_MASK	0x0f000000u
#define ABR_HANDOFF_SHIFT	24

#define ABR_HANDOFF_NONE	0x0u
#define ABR_HANDOFF_UNRECORDED	0x5u
#define ABR_HANDOFF_RECORDED	0xau

#endif /* ABR_HANDOFF_H */
