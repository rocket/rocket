#include "usync.h"
#include <math.h>

#ifdef SYNC_PLAYER

static int usync_rows[SYNC_TRACK_COUNT];
float usync_values[SYNC_TRACK_COUNT];

void usync_update(float t)
{
	int i, row = (int)floor(t);
	for (i = 0; i < SYNC_TRACK_COUNT; ++i) {
		int pos;
		float mag, x, a, b, c, d;

		/* empty tracks should not be neccesary! */
		if (!sync_data_count[i]) {
			usync_values[i] = 0.0f;
			continue;
		}

		/* step forward until we're at the right key-frame */
		while (usync_rows[i] < (sync_data_count[i] - 1) &&
		       row >= sync_data_rows[usync_rows[i] + 1]) {
			usync_rows[i]++;
		}

		pos = usync_rows[i] + sync_data_offset[i];

		/* we need a segment to interpolate over */
		if (usync_rows[i] == sync_data_count[i] - 1) {
			usync_values[i] =  sync_data_values[pos];
			continue;
		}

		/* prepare coefficients for interpolation */
		a = sync_data_values[pos];
		mag = sync_data_values[pos + 1] - sync_data_values[pos];
		switch (sync_data_type[pos]) {
		case 0:
			b = c = d = 0.0f;
			break;

		case 1:
			b = mag;
			c = d = 0.0f;
			break;

		case 2:
			b =  0.0f;
			c =  3 * mag;
			d = -2 * mag;
			break;

		case 3:
			b = d = 0.0f;
			c = mag;
			break;
		}

		/* evaluate function */
		x = (t - sync_data_rows[pos]) / (sync_data_rows[pos + 1] - sync_data_rows[pos]);
		usync_values[i] = a + (b + (c + d * x) * x) * x;
	}
}

#else /* !defined(SYNC_PLAYER) */

#include <stdio.h>
#include "../lib/sync.h"
#include "../lib/device.h"

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

void usync_export(void)
{
	int i, j;
	int offset = 0;
	FILE *fp = fopen("sync-data.h", "w");

	if (!fp)
		return;

	/* header-guard */
	fputs("#ifndef SYNC_DATA_H\n#define SYNC_DATA_H\n\n", fp);

	fputs("enum sync_tracks {\n", fp);
	for (i = 0; i < usync_dev->data.num_tracks; ++i) {
		struct sync_track *t = usync_dev->data.tracks[i];
		fprintf(fp, "\tSYNC_TRACK_%s = %d,\n", t->name, i);
	}
	fprintf(fp, "\tSYNC_TRACK_COUNT = %d\n", usync_dev->data.num_tracks);
	fputs("};\n\n", fp);

	fputs("static const int sync_data_offset[SYNC_TRACK_COUNT] = {\n", fp);
	for (i = 0; i < usync_dev->data.num_tracks; ++i) {
		struct sync_track *t = usync_dev->data.tracks[i];
		fprintf(fp, "\t%d, /* track: %s */\n", offset, t->name);
		offset += t->num_keys;
	}
	fputs("};\n\n", fp);

	fputs("static const int sync_data_count[SYNC_TRACK_COUNT] = {\n", fp);
	for (i = 0; i < usync_dev->data.num_tracks; ++i) {
		struct sync_track *t = usync_dev->data.tracks[i];
		fprintf(fp, "\t%d, /* track: %s */\n", t->num_keys, t->name);
	}
	fputs("};\n\n", fp);

	fputs("static const int sync_data_rows[] = {\n", fp);
	for (i = 0; i < usync_dev->data.num_tracks; ++i) {
		struct sync_track *t = usync_dev->data.tracks[i];
		fprintf(fp, "\t/* track: %s */\n", t->name);
		for (j = 0; j < t->num_keys; ++j)
			fprintf(fp, "\t%d,\n", t->keys[j].row);
	}
	fputs("};\n\n", fp);

	fputs("static const float sync_data_values[] = {\n", fp);
	for (i = 0; i < usync_dev->data.num_tracks; ++i) {
		struct sync_track *t = usync_dev->data.tracks[i];
		fprintf(fp, "\t/* track: %s */\n", t->name);
		for (j = 0; j < t->num_keys; ++j)
			fprintf(fp, "\t%d,\n", t->keys[j].value);
	}
	fputs("};\n\n", fp);

	fputs("static const unsigned char sync_data_type[] = {\n", fp);
	for (i = 0; i < usync_dev->data.num_tracks; ++i) {
		struct sync_track *t = usync_dev->data.tracks[i];
		fprintf(fp, "\t/* track: %s */\n", t->name);
		for (j = 0; j < t->num_keys; ++j)
			fprintf(fp, "\t%d,\n", t->keys[j].type);
	}
	fputs("};\n\n", fp);

	fputs("#endif /* !defined(SYNC_DATA_H) */\n", fp);
	fclose(fp);
}

#endif
