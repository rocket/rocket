/* Copyright (C) 2010 Contributors
 * For conditions of distribution and use, see copyright notice in COPYING
 */

#ifndef SYNC_H
#define SYNC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#ifdef __GNUC__
#define SYNC_DEPRECATED(msg) __attribute__ ((deprecated(msg)))
#elif defined(_MSC_VER)
#define SYNC_DEPRECATED(msg) __declspec(deprecated("is deprecated: " msg))
#else
#define SYNC_DEPRECATED(msg)
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
#define SYNC_DEFAULT_PORT 1338
int sync_tcp_connect(struct sync_device *, const char *, unsigned short);
int SYNC_DEPRECATED("use sync_tcp_connect instead") sync_connect(struct sync_device *, const char *, unsigned short);
int sync_update(struct sync_device *, int, struct sync_cb *, void *);
int sync_save_tracks(const struct sync_device *);
#endif /* defined(SYNC_PLAYER) */

struct sync_io_cb {
	void *(*open)(const char *filename, const char *mode);
	size_t (*read)(void *ptr, size_t size, size_t nitems, void *stream);
	int (*close)(void *stream);
};
void sync_set_io_cb(struct sync_device *d, struct sync_io_cb *cb);

const struct sync_track *sync_get_track(struct sync_device *, const char *);
double sync_get_val(const struct sync_track *, double);

#ifdef __cplusplus
}
#endif

#endif /* !defined(SYNC_H) */
