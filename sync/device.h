/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in COPYING
 */

#ifndef SYNC_DEVICE_H
#define SYNC_DEVICE_H

#include "track.h"
#include "data.h"

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

#endif /* SYNC_DEVICE_H */
