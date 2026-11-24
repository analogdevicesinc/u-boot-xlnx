// SPDX-License-Identifier: GPL-2.0-or-later OR BSD-3-Clause
/*
 * A/B boot record: CRC-32, header decoding, copy selection and journal scan
 *
 * Copyright (C) 2026 Analog Devices, Inc.
 */
#include "abr_priv.h"

static const uint32_t abr_crc32_nibble[16] = {
	0x00000000u, 0x1db71064u, 0x3b6e20c8u, 0x26d930acu,
	0x76dc4190u, 0x6b6b51f4u, 0x4db26158u, 0x5005713cu,
	0xedb88320u, 0xf00f9344u, 0xd6d6a3e8u, 0xcb61b38cu,
	0x9b64c2b0u, 0x86d3d2d4u, 0xa00ae278u, 0xbdbdf21cu,
};

uint32_t abr_crc32(uint32_t crc, const void *buf, size_t len)
{
	const uint8_t *p = buf;

	crc = ~crc;
	while (len--) {
		crc ^= *p++;
		crc = (crc >> 4) ^ abr_crc32_nibble[crc & 0xfu];
		crc = (crc >> 4) ^ abr_crc32_nibble[crc & 0xfu];
	}
	return ~crc;
}

bool abr_dev_valid(const struct abr_dev *dev)
{
	if (!dev || !dev->ops || !dev->ops->read)
		return false;
	if (dev->copy_size < ABR_MIN_COPY_SIZE)
		return false;
	return dev->copy_size <= (UINT32_MAX - dev->base) / ABR_NUM_COPIES;
}

uint32_t abr_journal_capacity(uint32_t copy_size)
{
	uint32_t entries = (copy_size - ABR_HEADER_SIZE) / ABR_ENTRY_SIZE;

	return entries < ABR_JOURNAL_FIELD_MAX ? entries
					       : ABR_JOURNAL_FIELD_MAX;
}

bool abr_record_valid(const struct abr_record *rec, uint32_t copy_size)
{
	if (rec->active >= ABR_NUM_SLOTS)
		return false;
	if (rec->trial != ABR_SLOT_NONE &&
	    (rec->trial >= ABR_NUM_SLOTS || rec->trial == rec->active))
		return false;
	if (rec->max_attempts < 1 ||
	    rec->max_attempts > ABR_MAX_ATTEMPTS_LIMIT)
		return false;
	if (rec->journal_entries < ABR_JOURNAL_MIN_ENTRIES ||
	    rec->journal_entries > abr_journal_capacity(copy_size))
		return false;
	if (!rec->slot[rec->active].boot_len)
		return false;
	if (rec->trial != ABR_SLOT_NONE && !rec->slot[rec->trial].boot_len)
		return false;
	return true;
}

bool abr_header_decode(const uint8_t *hdr, uint32_t copy_size,
		       struct abr_record *rec)
{
	unsigned int slot;

	if (abr_get_le32(hdr + ABR_OFF_MAGIC) != ABR_MAGIC ||
	    abr_get_le16(hdr + ABR_OFF_VERSION) != ABR_FORMAT_VERSION ||
	    abr_get_le32(hdr + ABR_OFF_CRC) != abr_crc32(0, hdr, ABR_OFF_CRC))
		return false;

	rec->journal_entries = abr_get_le16(hdr + ABR_OFF_JOURNAL_ENTRIES);
	rec->sequence = abr_get_le32(hdr + ABR_OFF_SEQUENCE);
	rec->active = hdr[ABR_OFF_ACTIVE];
	rec->trial = hdr[ABR_OFF_TRIAL];
	rec->max_attempts = hdr[ABR_OFF_MAX_ATTEMPTS];
	for (slot = 0; slot < ABR_NUM_SLOTS; slot++) {
		const uint8_t *p = hdr + ABR_OFF_SLOT +
				   slot * ABR_SLOT_INFO_SIZE;
		struct abr_slot_info *info = &rec->slot[slot];

		info->boot_len = abr_get_le32(p + ABR_SLOT_OFF_BOOT_LEN);
		info->boot_crc = abr_get_le32(p + ABR_SLOT_OFF_BOOT_CRC);
		info->fit_len = abr_get_le32(p + ABR_SLOT_OFF_FIT_LEN);
		info->fw_version = abr_get_le32(p + ABR_SLOT_OFF_FW_VERSION);
	}
	return abr_record_valid(rec, copy_size);
}

int abr_load(const struct abr_dev *dev, struct abr_current *cur)
{
	uint8_t hdr[ABR_HEADER_SIZE];
	struct abr_record rec;
	unsigned int copy;
	int ret = ABR_OK;

	cur->valid = false;
	cur->copy = 0;
	for (copy = 0; copy < ABR_NUM_COPIES; copy++) {
		cur->copy_valid[copy] = false;
		cur->copy_sequence[copy] = 0;
		if (dev->ops->read(dev->ctx, abr_copy_offset(dev, copy), hdr,
				   sizeof(hdr))) {
			ret = ABR_ERR_IO;
			continue;
		}
		if (!abr_header_decode(hdr, dev->copy_size, &rec))
			continue;

		cur->copy_valid[copy] = true;
		cur->copy_sequence[copy] = rec.sequence;
		if (!cur->valid || rec.sequence > cur->rec.sequence) {
			cur->valid = true;
			cur->copy = copy;
			cur->rec = rec;
		}
	}
	return ret;
}

static unsigned int abr_entry_slot(uint8_t attempt)
{
	if (attempt == ABR_ATTEMPT_SLOT_A)
		return ABR_SLOT_A;
	if (attempt == ABR_ATTEMPT_SLOT_B)
		return ABR_SLOT_B;
	return ABR_SLOT_NONE;
}

static bool abr_entry_free(const uint8_t *entry)
{
	unsigned int i;

	for (i = 0; i < ABR_ENTRY_SIZE; i++)
		if (entry[i] != ABR_ERASED)
			return false;
	return true;
}

static bool abr_entry_confirmed(const uint8_t *entry)
{
	unsigned int i;

	for (i = 0; i < ABR_CONFIRM_MARK_LEN; i++)
		if (entry[ABR_ENTRY_OFF_CONFIRM + i] != ABR_CONFIRMED)
			return false;
	return true;
}

int abr_journal_scan(const struct abr_dev *dev, const struct abr_current *cur,
		     struct abr_journal *jnl)
{
	uint8_t buf[ABR_IO_CHUNK];
	uint32_t total = cur->rec.journal_entries;
	uint32_t index = 0;

	jnl->used = 0;
	jnl->unconfirmed = 0;
	jnl->last_slot = ABR_SLOT_NONE;
	jnl->last_confirmed = false;

	while (index < total) {
		uint32_t count = total - index;
		uint32_t i;

		if (count > ABR_IO_CHUNK / ABR_ENTRY_SIZE)
			count = ABR_IO_CHUNK / ABR_ENTRY_SIZE;
		if (dev->ops->read(dev->ctx,
				   abr_entry_offset(dev, cur->copy, index),
				   buf, count * ABR_ENTRY_SIZE))
			return ABR_ERR_IO;

		for (i = 0; i < count; i++) {
			const uint8_t *entry = buf + i * ABR_ENTRY_SIZE;

			if (abr_entry_free(entry))
				return ABR_OK;

			jnl->used = index + i + 1;
			jnl->last_slot =
				abr_entry_slot(entry[ABR_ENTRY_OFF_ATTEMPT]);
			jnl->last_confirmed = abr_entry_confirmed(entry);
			if (jnl->last_confirmed)
				jnl->unconfirmed = 0;
			else
				jnl->unconfirmed++;
		}
		index += count;
	}
	return ABR_OK;
}

unsigned int abr_candidate(const struct abr_record *rec, uint32_t unconfirmed)
{
	unsigned int first = rec->active;
	unsigned int second = rec->active ^ 1u;
	unsigned int slot;

	if (rec->trial != ABR_SLOT_NONE) {
		first = rec->trial;
		second = rec->active;
	}
	slot = (unconfirmed / rec->max_attempts) % 2u ? second : first;
	if (!rec->slot[slot].boot_len)
		slot = rec->active;
	return slot;
}
