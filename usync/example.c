#include "usync.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
	if (usync_init() < 0)
		abort();

	while (1) {
		float row = 0.0f;
		usync_update(row);
		printf("%f\n", usync_get_val(foo));
	}
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
