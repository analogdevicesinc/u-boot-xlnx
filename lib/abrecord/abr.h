/* SPDX-License-Identifier: GPL-2.0-or-later OR BSD-3-Clause */
/*
 * A/B boot record: public API and on-flash format
 *
 * The record is stored twice. Copy 0 starts at the region base and copy 1
 * directly follows it; each copy is exactly one erase block. A copy is a
 * 128-byte header followed by an append-only boot journal. Multi-byte
 * fields are little-endian.
 *
 * Header:
 *   0x00 u32  magic, ABR_MAGIC ("ABRC")
 *   0x04 u16  format version, ABR_FORMAT_VERSION
 *   0x06 u16  journal capacity in entries
 *   0x08 u32  sequence number
 *   0x0c u8   active slot
 *   0x0d u8   trial slot, or ABR_SLOT_NONE
 *   0x0e u8   max attempts per candidate slot
 *   0x0f u8   reserved, 0
 *   0x10 16   slot A: BOOT.BIN length, BOOT.BIN CRC-32, FIT length,
 *             firmware version (u32 each)
 *   0x20 16   slot B, same layout
 *   0x30 76   reserved, 0
 *   0x7c u32  CRC-32 of bytes 0x00..0x7b
 *
 * Journal entry, ABR_ENTRY_SIZE bytes, one per recorded boot attempt:
 *   0     attempt mark: 0xf0 slot A, 0x0f slot B
 *   1     reserved, left erased
 *   2..3  confirm mark: 0x00 0x00 when confirmed
 * An entry whose bytes are all 0xff is free and ends the journal. Any
 * other attempt byte is an attempt of an unknown slot; any confirm mark
 * other than 0x00 0x00 is unconfirmed.
 *
 * Invariants:
 * - A copy is valid when magic, version, CRC and every field range check
 *   out. The current record is the valid copy with the higher sequence.
 * - A record is only written to the copy that is not current: erase, leave
 *   the journal erased, program the header last. A torn write leaves that
 *   copy invalid and the previous record current.
 * - Each record write uses sequence + 1. Every write erases a block, so
 *   flash endurance (~10^5 cycles per block) is exhausted long before the
 *   32-bit sequence; writers refuse to go past UINT32_MAX.
 * - Journal updates only clear bits of erased bytes and never erase.
 * - The active slot and the trial slot have a non-zero BOOT.BIN length;
 *   a zero length marks a slot that holds no committed image.
 *
 * Copyright (C) 2026 Analog Devices, Inc.
 */
#ifndef ABR_H
#define ABR_H

#include "abr_port.h"

#define ABR_NUM_SLOTS		2u
#define ABR_NUM_COPIES		2u
#define ABR_SLOT_A		0u
#define ABR_SLOT_B		1u
#define ABR_SLOT_NONE		0xffu

#define ABR_MAGIC		0x43524241u
#define ABR_FORMAT_VERSION	1u
#define ABR_HEADER_SIZE		128u
#define ABR_ENTRY_SIZE		4u

/* Journal capacity written by this implementation, bounded by geometry. */
#define ABR_JOURNAL_ENTRIES	4096u
#define ABR_JOURNAL_MIN_ENTRIES	128u
#define ABR_MIN_COPY_SIZE \
	(ABR_HEADER_SIZE + ABR_JOURNAL_MIN_ENTRIES * ABR_ENTRY_SIZE)

#define ABR_DEFAULT_MAX_ATTEMPTS 3u
#define ABR_MAX_ATTEMPTS_LIMIT	16u

/* Unconfirmed attempts, in units of max_attempts, that a fallback may reach. */
#define ABR_FALLBACK_TURNS	2u

enum abr_error {
	ABR_OK = 0,
	ABR_ERR_INVAL = -1,		/* bad argument or geometry */
	ABR_ERR_IO = -2,		/* flash operation failed */
	ABR_ERR_NO_RECORD = -3,		/* no valid copy */
	ABR_ERR_EXISTS = -4,		/* a valid record already exists */
	ABR_ERR_SLOT_RUNNING = -5,	/* target slot is the running slot */
	ABR_ERR_NOT_ACTIVE = -6,	/* running slot is not the active slot */
	ABR_ERR_UNCOMMITTED = -7,	/* slot holds no committed image */
	ABR_ERR_NO_ERASE = -8,		/* operation needs erase/program ops */
	ABR_ERR_VERIFY = -9,		/* read-back after erase/program differs */
	ABR_ERR_SEQUENCE = -10,		/* sequence number exhausted */
};

/*
 * Flash access. Offsets are absolute flash offsets. Each callback returns
 * 0 on success and non-zero on failure. program() only clears bits. erase()
 * is called with an offset and length of exactly one copy and may be NULL
 * where erasing is not allowed; program() may be NULL for read-only use.
 */
struct abr_flash_ops {
	int (*read)(void *ctx, uint32_t offset, void *buf, uint32_t len);
	int (*program)(void *ctx, uint32_t offset, const void *buf,
		       uint32_t len);
	int (*erase)(void *ctx, uint32_t offset, uint32_t len);
};

struct abr_dev {
	const struct abr_flash_ops *ops;
	void *ctx;
	uint32_t base;		/* flash offset of copy 0 */
	uint32_t copy_size;	/* bytes per copy, one erase block */
};

struct abr_slot_info {
	uint32_t boot_len;	/* 0: no committed image */
	uint32_t boot_crc;
	uint32_t fit_len;
	uint32_t fw_version;
};

struct abr_record {
	uint32_t sequence;
	uint32_t journal_entries;
	unsigned int active;
	unsigned int trial;
	unsigned int max_attempts;
	struct abr_slot_info slot[ABR_NUM_SLOTS];
};

/*
 * Return true when the BOOT.BIN of @slot is intact. @info is the slot's
 * metadata from the record, or NULL when there is no valid record.
 */
typedef bool (*abr_intact_fn)(void *ctx, unsigned int slot,
			      const struct abr_slot_info *info);

#define ABR_SEL_NO_RECORD	0x01u	/* no valid record, default choice */
#define ABR_SEL_UNRECORDED	0x02u	/* attempt not written to journal */
#define ABR_SEL_JOURNAL_FULL	0x04u	/* journal has no free entry */
#define ABR_SEL_FALLBACK	0x08u	/* choice not intact, other slot used */
#define ABR_SEL_NOT_INTACT	0x10u	/* no slot intact, choice kept */
#define ABR_SEL_KEPT_ACTIVE	0x20u	/* attempt not recordable, active used */

struct abr_selection {
	unsigned int slot;
	unsigned int flags;
	uint32_t unconfirmed;	/* unconfirmed attempts before this boot */
};

enum abr_confirm_outcome {
	ABR_CONFIRM_NORMAL,		/* active slot confirmed */
	ABR_CONFIRM_TRIAL_ACCEPTED,	/* trial slot is now active */
	ABR_CONFIRM_ROLLED_BACK,	/* failed trial cleared */
	ABR_CONFIRM_FELL_BACK,		/* other slot replaced failed active */
};

struct abr_status {
	bool copy_valid[ABR_NUM_COPIES];
	uint32_t copy_sequence[ABR_NUM_COPIES];
	bool valid;			/* fields below are set only if true */
	unsigned int copy;		/* current copy */
	struct abr_record rec;
	uint32_t journal_used;
	uint32_t unconfirmed;
	unsigned int last_slot;		/* slot of latest entry, or NONE */
	bool last_confirmed;
	unsigned int next_slot;		/* next choice before intact checks */
};

/* CRC-32 (IEEE 802.3, zlib convention); pass 0 as @crc to start. */
uint32_t abr_crc32(uint32_t crc, const void *buf, size_t len);

/*
 * Choose the slot to boot and, if @record is true and the journal has a
 * free entry, append an attempt for that slot. Never erases. @intact may be
 * NULL to treat every committed slot as intact. @sel always holds a
 * decision when @dev and @sel are valid, even if an error is returned.
 *
 * Only the active slot boots without a recorded attempt: when the attempt
 * cannot be recorded (journal full or unreadable, no program operation,
 * program failed) and the choice is another slot, the active slot is used
 * instead if it is intact (ABR_SEL_KEPT_ACTIVE). A caller that passes
 * @record false gets the choice the selector makes when recording works.
 */
int abr_select(const struct abr_dev *dev, abr_intact_fn intact,
	       void *intact_ctx, bool record, struct abr_selection *sel);

/*
 * Slot the selector picks for @unconfirmed attempts when it can record the
 * attempt, before intact checks.
 */
unsigned int abr_candidate(const struct abr_record *rec, uint32_t unconfirmed);

/*
 * Read-only view of both copies, the current record and its journal.
 * Returns ABR_ERR_NO_RECORD when neither copy is valid.
 */
int abr_read_status(const struct abr_dev *dev, struct abr_status *st);

/*
 * Return true when a reset lets the selector start the other slot after
 * the boot of @running failed, given the status @st read in that boot and
 * whether the selector recorded its attempt. Only a recorded boot may
 * reset, and only while at most ABR_FALLBACK_TURNS * max_attempts attempts
 * are unconfirmed, this boot included, and the other slot becomes the
 * choice within the next max_attempts attempts. Every reset therefore adds
 * an attempt and the resets since the last confirmed boot are bounded.
 */
bool abr_fallback_allowed(const struct abr_status *st, unsigned int running,
			  bool recorded);

/*
 * Write the first record (sequence 1, no trial) to copy 0. @slots[@active]
 * must have a non-zero BOOT.BIN length; the other slot may be all zero.
 * Refuses with ABR_ERR_EXISTS if a valid record exists.
 */
int abr_init(const struct abr_dev *dev, unsigned int active,
	     const struct abr_slot_info slots[ABR_NUM_SLOTS],
	     unsigned int max_attempts);

/*
 * Replace any record with a new one holding @active, @slots and
 * @max_attempts, no trial and an empty journal. Without a valid record this
 * is abr_init(); otherwise the record goes to the other copy with the next
 * sequence number, so a torn write leaves the old record current.
 */
int abr_reset(const struct abr_dev *dev, unsigned int active,
	      const struct abr_slot_info slots[ABR_NUM_SLOTS],
	      unsigned int max_attempts);

/*
 * Build the image of copy 0 as abr_init() would write it: header followed
 * by erased journal, @len bytes in total (ABR_HEADER_SIZE..@copy_size).
 * Copy 1 of a factory image must be left erased.
 */
int abr_format_image(uint32_t copy_size, unsigned int active,
		     const struct abr_slot_info slots[ABR_NUM_SLOTS],
		     unsigned int max_attempts, uint8_t *buf, uint32_t len);

/*
 * Put @slot, whose images have just been written and verified, on trial
 * with metadata @info. @running_slot must be the active slot and must
 * differ from @slot. The journal starts empty.
 */
int abr_commit_update(const struct abr_dev *dev, unsigned int slot,
		      const struct abr_slot_info *info,
		      unsigned int running_slot);

/*
 * Mark @slot empty before it is overwritten: write a record without its
 * metadata and without a trial of it, so that neither the trial nor the
 * fallback rule can pick it. @running_slot must be the active slot and must
 * differ from @slot. Writes nothing if the slot is already empty and not on
 * trial.
 */
int abr_clear_slot(const struct abr_dev *dev, unsigned int slot,
		   unsigned int running_slot);

/*
 * Confirm a healthy boot of @slot. Marks this boot's journal entry, or
 * writes a new record when @slot becomes active or a failed trial is
 * cleared; rolls the journal over when it runs low. @outcome is set on
 * ABR_OK.
 */
int abr_confirm(const struct abr_dev *dev, unsigned int slot,
		enum abr_confirm_outcome *outcome);

const char *abr_strerror(int err);

#endif /* ABR_H */
