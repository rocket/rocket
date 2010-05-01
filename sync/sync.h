/* Copyright (C) 2010 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in COPYING
 */

#ifndef SYNC_H
#define SYNC_H

#ifdef __cplusplus
extern "C" {
#endif

struct sync_device;
struct sync_track;

struct sync_device *sync_create_device(const char *);
void sync_destroy_device(struct sync_device *);

#ifndef SYNC_PLAYER
struct sync_cb {
	void (*pause)(void *, int);
	void (*set_row)(void *, int);
	int (*is_playing)(void *);
};
void sync_set_callbacks(struct sync_device *, struct sync_cb *, void *);
#endif /* !defined(SYNC_PLAYER) */

void sync_update(struct sync_device *, int);
const struct sync_track *sync_get_track(struct sync_device *, const char *);
float sync_get_val(const struct sync_track *, double);

#ifdef __cplusplus
}
#endif

#endif /* !defined(SYNC_H) */
