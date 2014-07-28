#ifndef SYNC_DEVICE_H
#define SYNC_DEVICE_H

#include "data.h"
#include "sync.h"

struct sync_device {
	char *base;
	struct sync_data data;

#ifndef SYNC_PLAYER
	int row;
	SOCKET sock;
#else
	struct sync_io_cb io_cb;
#endif
};

#endif /* SYNC_DEVICE_H */
