/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#ifndef SYNC_DEVICE_H
#define SYNC_DEVICE_H

#include "track.h"
#include "data.h"

#ifdef __cplusplus
extern "C" {
#endif

struct sync_device {
	char *base;
	struct sync_data data;

#ifndef SYNC_PLAYER
	struct sync_cb *cb;
	void *cb_param;
	int row;
	SOCKET sock;
#endif
};
const char *sync_track_path(const char *base, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* SYNC_DEVICE_H */
