/* Copyright (C) 2007-2010 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in COPYING
 */

#ifndef SYNC_DATA_H
#define SYNC_DATA_H

#include "track.h"

struct sync_data {
	struct sync_track **tracks;
	size_t num_tracks;
};

static inline int sync_find_track(const struct sync_data *data,
    const char *name)
{
	int i;
	for (i = 0; i < (int)data->num_tracks; ++i)
		if (!strcmp(name, data->tracks[i]->name))
			return i;
	return -1; /* not found */
}

void sync_data_deinit(struct sync_data *);
int sync_create_track(struct sync_data *, const char *);

#endif /* SYNC_DATA_H */
