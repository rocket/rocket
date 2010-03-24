/* Copyright (C) 2010 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in COPYING
 */

#include <stdlib.h>
#include <assert.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.141926
#endif

#include "sync.h"
#include "track.h"
#include "base.h"

static float key_linear(const struct track_key k[2], double row)
{
	double t = (row - k[0].row) / (k[1].row - k[0].row);
	return (float)(k[0].value + (k[1].value - k[0].value) * t);
}

static float key_smooth(const struct track_key k[2], double row)
{
	double t = (row - k[0].row) / (k[1].row - k[0].row);
	t = t * t * (3 - 2 * t);
	return (float)(k[0].value + (k[1].value - k[0].value) * t);
}

static float key_ramp(const struct track_key k[2], double row)
{
	double t = (row - k[0].row) / (k[1].row - k[0].row);
	t = pow(t, 2.0);
	return (float)(k[0].value + (k[1].value - k[0].value) * t);
}

float sync_get_val(const struct sync_track *t, double row)
{
	int idx, irow;
	if (!t->num_keys)
		return 0.0f;

	irow = (int)floor(row);
	idx = key_idx_floor(t, irow);
	if (idx < 0)
		return t->keys[0].value;
	if (idx > (int)t->num_keys - 2)
		return t->keys[t->num_keys - 1].value;

	switch (t->keys[idx].type) {
	case KEY_STEP:
		return t->keys[idx].value;
	case KEY_LINEAR:
		return key_linear(t->keys + idx, row);
	case KEY_SMOOTH:
		return key_smooth(t->keys + idx, row);
	case KEY_RAMP:
		return key_ramp(t->keys + idx, row);
	default:
		assert(0);
		return 0.0f;
	}
}

int sync_find_key(const struct sync_track *t, int row)
{
	int lo = 0, hi = t->num_keys;
	while (lo < hi) {
		int mi = (lo + hi) / 2;
		assert(mi != hi);

		if (t->keys[mi].row < row)
			lo = mi + 1;
		else if (t->keys[mi].row > row)
			hi = mi;
		else
			return mi;
	}
	return -lo - 1;
}

#ifndef SYNC_PLAYER
void sync_set_key(struct sync_track *t, const struct track_key *k)
{
	int idx = sync_find_key(t, k->row);
	if (idx < 0) {
		idx = -idx - 1;
		t->num_keys++;
		t->keys = realloc(t->keys, sizeof(struct track_key) *
		    t->num_keys);
		assert(t->keys);
		memmove(t->keys + idx + 1, t->keys + idx,
		    sizeof(struct track_key) * (t->num_keys - idx - 1));
	}
	t->keys[idx] = *k;
}

void sync_del_key(struct sync_track *t, int pos)
{
	int idx = sync_find_key(t, pos);
	assert(idx >= 0);
	memmove(t->keys + idx, t->keys + idx + 1,
	    sizeof(struct track_key) * (t->num_keys - idx - 1));
	assert(t->keys);
	t->num_keys--;
	t->keys = realloc(t->keys, t->num_keys * sizeof(struct track_key));
}
#endif
