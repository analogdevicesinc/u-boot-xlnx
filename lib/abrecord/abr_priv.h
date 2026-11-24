/* SPDX-License-Identifier: GPL-2.0-or-later OR BSD-3-Clause */
/*
 * A/B boot record: internal layout constants and helpers
 *
 * Copyright (C) 2026 Analog Devices, Inc.
 */
#ifndef ABR_PRIV_H
#define ABR_PRIV_H

#include "abr.h"

#define ABR_IO_CHUNK		64u

#define ABR_OFF_MAGIC		0x00u
#define ABR_OFF_VERSION		0x04u
#define ABR_OFF_JOURNAL_ENTRIES	0x06u
#define ABR_OFF_SEQUENCE	0x08u
#define ABR_OFF_ACTIVE		0x0cu
#define ABR_OFF_TRIAL		0x0du
#define ABR_OFF_MAX_ATTEMPTS	0x0eu
#define ABR_OFF_SLOT		0x10u
#define ABR_OFF_CRC		(ABR_HEADER_SIZE - 4u)

#define ABR_SLOT_INFO_SIZE	16u
#define ABR_SLOT_OFF_BOOT_LEN	0x0u
#define ABR_SLOT_OFF_BOOT_CRC	0x4u
#define ABR_SLOT_OFF_FIT_LEN	0x8u
#define ABR_SLOT_OFF_FW_VERSION	0xcu

#define ABR_ENTRY_OFF_ATTEMPT	0u
#define ABR_ENTRY_OFF_CONFIRM	2u
#define ABR_CONFIRM_MARK_LEN	2u
#define ABR_ATTEMPT_SLOT_A	0xf0u
#define ABR_ATTEMPT_SLOT_B	0x0fu
#define ABR_CONFIRMED		0x00u
#define ABR_ERASED		0xffu

#define ABR_JOURNAL_FIELD_MAX	0xffffu

/* Roll over once fewer than 1/ABR_ROLLOVER_DIVISOR of the entries are free. */
#define ABR_ROLLOVER_DIVISOR	4u

struct abr_current {
	bool copy_valid[ABR_NUM_COPIES];
	uint32_t copy_sequence[ABR_NUM_COPIES];
	bool valid;
	unsigned int copy;
	struct abr_record rec;
};

struct abr_journal {
	uint32_t used;
	uint32_t unconfirmed;
	unsigned int last_slot;
	bool last_confirmed;
};

static inline uint16_t abr_get_le16(const uint8_t *p)
{
	return (uint16_t)(p[0] | (unsigned int)p[1] << 8);
}

static inline uint32_t abr_get_le32(const uint8_t *p)
{
	return (uint32_t)p[0] | (uint32_t)p[1] << 8 |
	       (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static inline void abr_put_le16(uint8_t *p, uint16_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
}

static inline void abr_put_le32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

static inline uint32_t abr_copy_offset(const struct abr_dev *dev,
				       unsigned int copy)
{
	return dev->base + copy * dev->copy_size;
}

static inline uint32_t abr_entry_offset(const struct abr_dev *dev,
					unsigned int copy, uint32_t index)
{
	return abr_copy_offset(dev, copy) + ABR_HEADER_SIZE +
	       index * ABR_ENTRY_SIZE;
}

bool abr_dev_valid(const struct abr_dev *dev);
uint32_t abr_journal_capacity(uint32_t copy_size);
bool abr_record_valid(const struct abr_record *rec, uint32_t copy_size);
bool abr_header_decode(const uint8_t *hdr, uint32_t copy_size,
		       struct abr_record *rec);
void abr_header_encode(const struct abr_record *rec, uint8_t *hdr);
int abr_record_make(struct abr_record *rec, uint32_t copy_size,
		    unsigned int active,
		    const struct abr_slot_info slots[ABR_NUM_SLOTS],
		    unsigned int max_attempts);
uint32_t abr_writer_journal_entries(uint32_t copy_size);

/*
 * Read and validate both headers. Returns ABR_ERR_IO if a header could not
 * be read; @cur still describes the copies that could.
 */
int abr_load(const struct abr_dev *dev, struct abr_current *cur);

/*
 * Scan the journal of the current copy up to the first free entry. On
 * ABR_ERR_IO, @jnl covers the entries read before the failure.
 */
int abr_journal_scan(const struct abr_dev *dev, const struct abr_current *cur,
		     struct abr_journal *jnl);

#endif /* ABR_PRIV_H */
