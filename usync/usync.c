#include "usync.h"
#include <math.h>

#ifdef SYNC_PLAYER

#include "sync-data.h"

static int sync_rows[SYNC_TRACK_COUNT];
float sync_values[SYNC_TRACK_COUNT];

void usync_update(float t)
{
	int i, row = (int)floor(t);
	for (i = 0; i < SYNC_TRACK_COUNT; ++i) {
		/* TODO */
	}
}

#else /* !defined(SYNC_PLAYER) */

struct sync_device *usync_dev;
float usync_time = 0;

void usync_update(float t)
{
	usync_time = t;
	sync_update(usync_dev, (int)floor(t), &usync_cb, usync_data);
}

int usync_init(void)
{
	usync_dev = sync_create_device("sync");
	return sync_connect(usync_dev, "localhost", SYNC_DEFAULT_PORT);
}

#endif
