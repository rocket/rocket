/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in COPYING
 */

#include "device.h"
#include "sync.h"
#include <stdio.h>
#include <assert.h>
#include <math.h>

static const char *sync_track_path(const char *base, const char *name)
{
	static char temp[FILENAME_MAX];
	strncpy(temp, base, sizeof(temp));
	strncat(temp, "_", sizeof(temp));
	strncat(temp, name, sizeof(temp));
	strncat(temp, ".track", sizeof(temp));
	return temp;
}

#ifndef SYNC_PLAYER

static SOCKET server_connect(const char *host, unsigned short nport)
{
	struct hostent *he;
	struct sockaddr_in sa;
	char greet[128], **ap;
	SOCKET sock = INVALID_SOCKET;

#ifdef WIN32
	static int need_init = 1;
	if (need_init) {
		WSADATA wsa;
		if (WSAStartup(MAKEWORD(2, 0), &wsa))
			return INVALID_SOCKET;
		need_init = 0;
	}
#endif

	he = gethostbyname(host);
	if (!he)
		return INVALID_SOCKET;

	for (ap = he->h_addr_list; *ap; ++ap) {
		sa.sin_family = he->h_addrtype;
		sa.sin_port = htons(nport);
		memcpy(&sa.sin_addr, *ap, he->h_length);

		sock = socket(he->h_addrtype, SOCK_STREAM, 0);
		if (sock == INVALID_SOCKET)
			continue;

		if (connect(sock, (struct sockaddr *)&sa, sizeof(sa)) >= 0)
			break;

		closesocket(sock);
		sock = INVALID_SOCKET;
	}

	if (sock == INVALID_SOCKET)
		return INVALID_SOCKET;

	if (xsend(sock, CLIENT_GREET, strlen(CLIENT_GREET), 0) ||
	    xrecv(sock, greet, strlen(SERVER_GREET), 0))
		return INVALID_SOCKET;

	if (!strncmp(SERVER_GREET, greet, strlen(SERVER_GREET)))
		return sock;

	closesocket(sock);
	return INVALID_SOCKET;
}
#endif

struct sync_device *sync_create_device(const char *base)
{
	struct sync_device *d = malloc(sizeof(*d));
	if (!d)
		return NULL;

	d->base = strdup(base);
	if (!d->base) {
		free(d);
		return NULL;
	}

	d->data.tracks = NULL;
	d->data.num_tracks = 0;

#ifndef SYNC_PLAYER
	d->cb = d->cb_param = NULL;
	d->row = -1;
	d->sock = INVALID_SOCKET;
#endif

	return d;
}

void sync_destroy_device(struct sync_device *d)
{
	free(d->base);
	sync_data_deinit(&d->data);
	free(d);
}

#ifdef SYNC_PLAYER

static int load_track_data(struct sync_track *t, const char *path)
{
	int i;
	FILE *fp = fopen(path, "rb");
	if (!fp)
		return 1;

	fread(&t->num_keys, sizeof(size_t), 1, fp);
	t->keys = malloc(sizeof(struct track_key) * t->num_keys);

	for (i = 0; i < (int)t->num_keys; ++i) {
		struct track_key *key = t->keys + i;
		char type;
		fread(&key->row, sizeof(size_t), 1, fp);
		fread(&key->value, sizeof(float), 1, fp);
		fread(&type, sizeof(char), 1, fp);
		key->type = (enum key_type)type;
	}

	fclose(fp);
	return 0;
}

#else

void sync_set_callbacks(struct sync_device *d, struct sync_cb *cb, void *cb_param)
{
	d->cb = cb;
	d->cb_param = cb_param;
}

static int save_track(const struct sync_track *t, const char *path)
{
	int i;
	FILE *fp = fopen(path, "wb");
	if (!fp)
		return 1;

	fwrite(&t->num_keys, sizeof(size_t), 1, fp);
	for (i = 0; i < (int)t->num_keys; ++i) {
		char type = (char)t->keys[i].type;
		fwrite(&t->keys[i].row, sizeof(int), 1, fp);
		fwrite(&t->keys[i].value, sizeof(float), 1, fp);
		fwrite(&type, sizeof(char), 1, fp);
	}

	fclose(fp);
	return 0;
}

static void save_tracks(const char *base, struct sync_data *data)
{
	int i;
	for (i = 0; i < (int)data->num_tracks; ++i) {
		const struct sync_track *t = data->tracks[i];
		save_track(t, sync_track_path(base, t->name));
	}
}

static int request_track_data(SOCKET sock, const char *name, uint32_t idx)
{
	unsigned char cmd = GET_TRACK;
	uint32_t name_len = htonl(strlen(name));
	idx = htonl(idx);

	/* send request data */
	return xsend(sock, (char *)&cmd, 1, 0) ||
	       xsend(sock, (char *)&idx, sizeof(idx), 0) ||
	       xsend(sock, (char *)&name_len, sizeof(name_len), 0) ||
	       xsend(sock, name, (int)strlen(name), 0);
}

static int handle_set_key_cmd(SOCKET sock, struct sync_data *data)
{
	uint32_t track, row;
	union {
		float f;
		uint32_t i;
	} v;
	struct track_key key;
	unsigned char type;

	if (xrecv(sock, (char *)&track, sizeof(track), 0) ||
	    xrecv(sock, (char *)&row, sizeof(row), 0) ||
	    xrecv(sock, (char *)&v.i, sizeof(v.i), 0) ||
	    xrecv(sock, (char *)&type, 1, 0))
		return 0;

	track = ntohl(track);
	v.i = ntohl(v.i);

	key.row = ntohl(row);
	key.value = v.f;

	assert(type < KEY_TYPE_COUNT);
	assert(track < data->num_tracks);
	key.type = (enum key_type)type;
	sync_set_key(data->tracks[track], &key);
	return 1;
}

static int handle_del_key_cmd(SOCKET sock, struct sync_data *data)
{
	uint32_t track, row;

	if (xrecv(sock, (char *)&track, sizeof(track), 0) ||
	    xrecv(sock, (char *)&row, sizeof(row), 0))
		return 0;

	track = ntohl(track);
	row = ntohl(row);

	assert(track < data->num_tracks);
	sync_del_key(data->tracks[track], row);
	return 1;
}

static int purge_and_rerequest(struct sync_device *d)
{
	int i;
	for (i = 0; i < (int)d->data.num_tracks; ++i) {
		free(d->data.tracks[i]->keys);
		d->data.tracks[i]->keys = NULL;
		d->data.tracks[i]->num_keys = 0;

		if (request_track_data(d->sock, d->data.tracks[i]->name, i))
			return 1;
	}
	return 0;
}

int sync_connect(struct sync_device *d, const char *host, unsigned short port)
{
	if (d->sock != INVALID_SOCKET)
		closesocket(d->sock);

	d->sock = server_connect(host, port);
	if (d->sock == INVALID_SOCKET)
		return 1;

	return purge_and_rerequest(d);
}

int sync_update(struct sync_device *d, int row)
{
	if (d->sock == INVALID_SOCKET)
		return 1;

	/* look for new commands */
	while (socket_poll(d->sock)) {
		unsigned char cmd = 0, flag;
		uint32_t row;
		if (xrecv(d->sock, (char *)&cmd, 1, 0))
			goto sockerr;

		switch (cmd) {
		case SET_KEY:
			if (!handle_set_key_cmd(d->sock, &d->data))
				goto sockerr;
			break;
		case DELETE_KEY:
			if (!handle_del_key_cmd(d->sock, &d->data))
				goto sockerr;
			break;
		case SET_ROW:
			if (xrecv(d->sock, (char *)&row, sizeof(row), 0))
				goto sockerr;
			if (d->cb && d->cb->set_row)
				d->cb->set_row(d->cb_param, ntohl(row));
			break;
		case PAUSE:
			if (xrecv(d->sock, (char *)&flag, 1, 0))
				goto sockerr;
			if (d->cb && d->cb->pause)
				d->cb->pause(d->cb_param, flag);
			break;
		case SAVE_TRACKS:
			save_tracks(d->base, &d->data);
			break;
		default:
			fprintf(stderr, "unknown cmd: %02x\n", cmd);
			goto sockerr;
		}
	}

	if (d->cb && d->cb->is_playing && d->cb->is_playing(d->cb_param)) {
		if (d->row != row && d->sock != INVALID_SOCKET) {
			unsigned char cmd = SET_ROW;
			uint32_t nrow = htonl(row);
			if (xsend(d->sock, (char*)&cmd, 1, 0) ||
			    xsend(d->sock, (char*)&nrow, sizeof(nrow), 0))
				goto sockerr;
			d->row = row;
		}
	}
	return 0;

sockerr:
	closesocket(d->sock);
	d->sock = INVALID_SOCKET;
	return 1;
}

#endif

const struct sync_track *sync_get_track(struct sync_device *d,
    const char *name)
{
	struct sync_track *t;
	int idx = sync_find_track(&d->data, name);
	if (idx >= 0)
		return d->data.tracks[idx];

	idx = sync_create_track(&d->data, name);
	t = d->data.tracks[idx];

#if SYNC_PLAYER
	load_track_data(t, sync_track_path(d->base, name));
#else
	request_track_data(d->sock, name, idx);
#endif

	return t;
}
