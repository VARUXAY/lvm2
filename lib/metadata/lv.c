/*
 * Copyright (C) 2010 Red Hat, Inc. All rights reserved.
 *
 * This file is part of LVM2.
 *
 * This copyrighted material is made available to anyone wishing to use,
 * modify, copy, or redistribute it subject to the terms and conditions
 * of the GNU Lesser General Public License v.2.1.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "lib.h"
#include "metadata.h"
#include "activate.h"

char *lv_uuid_dup(const struct logical_volume *lv)
{
	return id_format_and_copy(lv->vg->vgmem, &lv->lvid.id[1]);
}

char *lv_tags_dup(const struct logical_volume *lv)
{
	return tags_format_and_copy(lv->vg->vgmem, &lv->tags);
}

uint64_t lv_size(const struct logical_volume *lv)
{
	return lv->size;
}

static int _lv_mimage_in_sync(const struct logical_volume *lv)
{
	float percent;
	percent_range_t percent_range;
	struct lv_segment *mirror_seg = find_mirror_seg(first_seg(lv));

	if (!(lv->status & MIRROR_IMAGE) || !mirror_seg)
		return_0;

	if (!lv_mirror_percent(lv->vg->cmd, mirror_seg->lv, 0, &percent,
			       &percent_range, NULL))
		return_0;

	return (percent_range == PERCENT_100) ? 1 : 0;
}

char *lv_attr_dup(struct dm_pool *mem, const struct logical_volume *lv)
{
	float snap_percent;
	percent_range_t percent_range;
	struct lvinfo info;
	char *repstr;

	if (!(repstr = dm_pool_zalloc(mem, 7))) {
		log_error("dm_pool_alloc failed");
		return 0;
	}

	/* Blank if this is a "free space" LV. */
	if (!*lv->name)
		goto out;

	if (lv->status & PVMOVE)
		repstr[0] = 'p';
	else if (lv->status & CONVERTING)
		repstr[0] = 'c';
	else if (lv->status & VIRTUAL)
		repstr[0] = 'v';
	/* Origin takes precedence over Mirror */
	else if (lv_is_origin(lv)) {
		repstr[0] = (lv_is_merging_origin(lv)) ? 'O' : 'o';
	}
	else if (lv->status & MIRRORED) {
		repstr[0] = (lv->status & MIRROR_NOTSYNCED) ? 'M' : 'm';
	}else if (lv->status & MIRROR_IMAGE)
		repstr[0] = (_lv_mimage_in_sync(lv)) ? 'i' : 'I';
	else if (lv->status & MIRROR_LOG)
		repstr[0] = 'l';
	else if (lv_is_cow(lv)) {
		repstr[0] = (lv_is_merging_cow(lv)) ? 'S' : 's';
	} else
		repstr[0] = '-';

	if (lv->status & PVMOVE)
		repstr[1] = '-';
	else if (lv->status & LVM_WRITE)
		repstr[1] = 'w';
	else if (lv->status & LVM_READ)
		repstr[1] = 'r';
	else
		repstr[1] = '-';

	repstr[2] = alloc_policy_char(lv->alloc);

	if (lv->status & LOCKED)
		repstr[2] = toupper(repstr[2]);

	repstr[3] = (lv->status & FIXED_MINOR) ? 'm' : '-';

	if (lv_info(lv->vg->cmd, lv, 0, &info, 1, 0) && info.exists) {
		if (info.suspended)
			repstr[4] = 's';	/* Suspended */
		else if (info.live_table)
			repstr[4] = 'a';	/* Active */
		else if (info.inactive_table)
			repstr[4] = 'i';	/* Inactive with table */
		else
			repstr[4] = 'd';	/* Inactive without table */

		/* Snapshot dropped? */
		if (info.live_table && lv_is_cow(lv) &&
		    (!lv_snapshot_percent(lv, &snap_percent, &percent_range) ||
		     percent_range == PERCENT_INVALID)) {
			repstr[0] = toupper(repstr[0]);
			if (info.suspended)
				repstr[4] = 'S'; /* Susp Inv snapshot */
			else
				repstr[4] = 'I'; /* Invalid snapshot */
		}

		repstr[5] = (info.open_count) ? 'o' : '-';
	} else {
		repstr[4] = '-';
		repstr[5] = '-';
	}
out:
	return repstr;
}