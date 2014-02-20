/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in COPYING
 */

#include "data.h"

void sync_data_deinit(struct sync_data *d)
{
	int i;
	for (i = 0; i < (int)d->num_tracks; ++i) {
		free(d->tracks[i]->name);
		free(d->tracks[i]->keys);
		free(d->tracks[i]);
	}
	free(d->tracks);
}

int sync_create_track(struct sync_data *d, const char *name)
{
	struct sync_track *t;
	assert(sync_find_track(d, name) < 0);

	t = malloc(sizeof(*t));
	t->name = strdup(name);
	t->keys = NULL;
	t->num_keys = 0;

	d->num_tracks++;
	d->tracks = realloc(d->tracks, sizeof(d->tracks[0]) * d->num_tracks);
	d->tracks[d->num_tracks - 1] = t;

	return (int)d->num_tracks - 1;
}
