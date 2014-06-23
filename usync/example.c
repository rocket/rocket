#include "usync.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
	if (usync_init() < 0)
		abort();

#ifndef SYNC_PLAYER
	/* HACK: prefetch tracks - not needed when actually editing */
	usync_get_val(foo);
	sleep(1);
	usync_update(0.0f);
#endif

	float row = 0.0f;
	while (row <= 8.0f) {
		usync_update(row);
		printf("%f: %f\n", row, usync_get_val(foo));
		row += 1.0f / 4; /* step one 4th of a row */
	}
	usync_export();
}

#ifndef SYNC_PLAYER
static void pause(void *d, int flag)
{
	/* TODO*/
}

static void set_row(void *d, int row)
{
	/* TODO*/
}

static int is_playing(void *d)
{
	/* TODO */
}

struct sync_cb usync_cb = {
	pause,
	set_row,
	is_playing
};

void *usync_data = NULL;
#endif
