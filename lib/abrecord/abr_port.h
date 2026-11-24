/* SPDX-License-Identifier: GPL-2.0-or-later OR BSD-3-Clause */
/*
 * A/B boot record: type definitions
 *
 * Fixed-width integer, size_t and bool definitions for the A/B boot
 * record library. An environment whose own headers clash with the
 * compiler's <stdint.h> (for example U-Boot) defines ABR_PORT_INCLUDE
 * as a header that provides uint8_t, uint16_t, uint32_t, UINT32_MAX,
 * size_t, bool, true, false and NULL.
 *
 * Copyright (C) 2026 Analog Devices, Inc.
 */
#ifndef ABR_PORT_H
#define ABR_PORT_H

#ifdef ABR_PORT_INCLUDE
#include ABR_PORT_INCLUDE
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#endif

#endif /* ABR_PORT_H */
