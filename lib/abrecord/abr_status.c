// SPDX-License-Identifier: GPL-2.0-or-later OR BSD-3-Clause
/*
 * A/B boot record: read-only status and error strings
 *
 * Copyright (C) 2026 Analog Devices, Inc.
 */
#include "abr_priv.h"

int abr_read_status(const struct abr_dev *dev, struct abr_status *st)
{
	struct abr_current cur;
	struct abr_journal jnl;
	unsigned int copy;
	int ret;
	int err;

	if (!st)
		return ABR_ERR_INVAL;
	st->valid = false;
	for (copy = 0; copy < ABR_NUM_COPIES; copy++) {
		st->copy_valid[copy] = false;
		st->copy_sequence[copy] = 0;
	}
	if (!abr_dev_valid(dev))
		return ABR_ERR_INVAL;

	ret = abr_load(dev, &cur);
	for (copy = 0; copy < ABR_NUM_COPIES; copy++) {
		st->copy_valid[copy] = cur.copy_valid[copy];
		st->copy_sequence[copy] = cur.copy_sequence[copy];
	}
	if (!cur.valid)
		return ret ? ret : ABR_ERR_NO_RECORD;

	err = abr_journal_scan(dev, &cur, &jnl);
	if (err)
		return err;

	st->valid = true;
	st->copy = cur.copy;
	st->rec = cur.rec;
	st->journal_used = jnl.used;
	st->unconfirmed = jnl.unconfirmed;
	st->last_slot = jnl.last_slot;
	st->last_confirmed = jnl.last_confirmed;
	st->next_slot = jnl.used < cur.rec.journal_entries ?
			abr_candidate(&cur.rec, jnl.unconfirmed) :
			cur.rec.active;
	return ret;
}

bool abr_fallback_allowed(const struct abr_status *st, unsigned int running,
			  bool recorded)
{
	unsigned int other = running ^ 1u;
	uint32_t n;

	if (!st || !st->valid || running >= ABR_NUM_SLOTS || !recorded)
		return false;
	if (!st->journal_used || st->last_confirmed ||
	    st->last_slot != running)
		return false;
	if (st->unconfirmed > ABR_FALLBACK_TURNS * st->rec.max_attempts)
		return false;
	if (st->journal_used >= st->rec.journal_entries)
		return st->next_slot == other;
	for (n = st->unconfirmed;
	     n < st->unconfirmed + st->rec.max_attempts; n++)
		if (abr_candidate(&st->rec, n) == other)
			return true;
	return false;
}

const char *abr_strerror(int err)
{
	switch (err) {
	case ABR_OK:
		return "success";
	case ABR_ERR_INVAL:
		return "invalid argument";
	case ABR_ERR_IO:
		return "flash access failed";
	case ABR_ERR_NO_RECORD:
		return "no valid boot record";
	case ABR_ERR_EXISTS:
		return "boot record already exists";
	case ABR_ERR_SLOT_RUNNING:
		return "slot is running";
	case ABR_ERR_NOT_ACTIVE:
		return "running slot is not the active slot";
	case ABR_ERR_UNCOMMITTED:
		return "slot has no committed image";
	case ABR_ERR_NO_ERASE:
		return "flash erase or program not available";
	case ABR_ERR_VERIFY:
		return "flash verify failed";
	case ABR_ERR_SEQUENCE:
		return "sequence number exhausted";
	default:
		return "unknown error";
	}
}
