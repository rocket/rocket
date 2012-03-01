/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in COPYING
 */

#include "device.h"
#include "sync.h"
#include <stdio.h>
#include <math.h>

static const char *sync_track_path(const char *base, const char *name)
{
	static char temp[FILENAME_MAX];
	strncpy(temp, base, sizeof(temp) - 1);
	temp[sizeof(temp) - 1] = '\0';
	strncat(temp, "_", sizeof(temp) - 1);
	strncat(temp, name, sizeof(temp) - 1);
	strncat(temp, ".track", sizeof(temp) - 1);
	return temp;
}

#ifndef SYNC_PLAYER

#ifdef USE_AMITCP
static struct Library *socket_base = NULL;
#endif

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
#elif defined(USE_AMITCP)
	if (!socket_base) {
		socket_base = OpenLibrary("bsdsocket.library", 4);
		if (!socket_base)
			return INVALID_SOCKET;
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

#if defined(USE_AMITCP) && !defined(SYNC_PLAYER)
	if (socket_base) {
		CloseLibrary(socket_base);
		socket_base = NULL;
	}
#endif
}

#ifdef SYNC_PLAYER

static int get_track_data(struct sync_device *d, struct sync_track *t)
{
	int i;
	FILE *fp = fopen(sync_track_path(d->base, t->name), "rb");
	if (!fp)
		return -1;

	fread(&t->num_keys, sizeof(size_t), 1, fp);
	t->keys = malloc(sizeof(struct track_key) * t->num_keys);
	if (!t->keys)
		return -1;

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

static int save_track(const struct sync_track *t, const char *path)
{
	int i;
	FILE *fp = fopen(path, "wb");
	if (!fp)
		return -1;

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

void sync_save_tracks(const struct sync_device *d)
{
	int i;
	for (i = 0; i < (int)d->data.num_tracks; ++i) {
		const struct sync_track *t = d->data.tracks[i];
		save_track(t, sync_track_path(d->base, t->name));
	}
}

static int get_track_data(struct sync_device *d, struct sync_track *t)
{
	unsigned char cmd = GET_TRACK;
	uint32_t name_len;

	assert(strlen(t->name) <= UINT32_MAX);
	name_len = htonl((uint32_t)strlen(t->name));

	/* send request data */
	if (xsend(d->sock, (char *)&cmd, 1, 0) ||
	    xsend(d->sock, (char *)&name_len, sizeof(name_len), 0) ||
	    xsend(d->sock, t->name, (int)strlen(t->name), 0))
	{
		closesocket(d->sock);
		d->sock = INVALID_SOCKET;
		return -1;
	}

	return 0;
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
		return -1;

	track = ntohl(track);
	v.i = ntohl(v.i);

	key.row = ntohl(row);
	key.value = v.f;

	assert(type < KEY_TYPE_COUNT);
	assert(track < data->num_tracks);
	key.type = (enum key_type)type;
	return sync_set_key(data->tracks[track], &key);
}

static int handle_del_key_cmd(SOCKET sock, struct sync_data *data)
{
	uint32_t track, row;

	if (xrecv(sock, (char *)&track, sizeof(track), 0) ||
	    xrecv(sock, (char *)&row, sizeof(row), 0))
		return -1;

	track = ntohl(track);
	row = ntohl(row);

	assert(track < data->num_tracks);
	return sync_del_key(data->tracks[track], row);
}

int sync_connect(struct sync_device *d, const char *host, unsigned short port)
{
	int i;
	if (d->sock != INVALID_SOCKET)
		closesocket(d->sock);

	d->sock = server_connect(host, port);
	if (d->sock == INVALID_SOCKET)
		return -1;

	for (i = 0; i < (int)d->data.num_tracks; ++i) {
		free(d->data.tracks[i]->keys);
		d->data.tracks[i]->keys = NULL;
		d->data.tracks[i]->num_keys = 0;
	}

	for (i = 0; i < (int)d->data.num_tracks; ++i) {
		if (get_track_data(d, d->data.tracks[i])) {
			closesocket(d->sock);
			d->sock = INVALID_SOCKET;
			return -1;
		}
	}
	return 0;
}

int sync_update(struct sync_device *d, int row, struct sync_cb *cb,
    void *cb_param)
{
	if (d->sock == INVALID_SOCKET)
		return -1;

	/* look for new commands */
	while (socket_poll(d->sock)) {
		unsigned char cmd = 0, flag;
		uint32_t row;
		if (xrecv(d->sock, (char *)&cmd, 1, 0))
			goto sockerr;

		switch (cmd) {
		case SET_KEY:
			if (handle_set_key_cmd(d->sock, &d->data))
				goto sockerr;
			break;
		case DELETE_KEY:
			if (handle_del_key_cmd(d->sock, &d->data))
				goto sockerr;
			break;
		case SET_ROW:
			if (xrecv(d->sock, (char *)&row, sizeof(row), 0))
				goto sockerr;
			if (cb && cb->set_row)
				cb->set_row(cb_param, ntohl(row));
			break;
		case PAUSE:
			if (xrecv(d->sock, (char *)&flag, 1, 0))
				goto sockerr;
			if (cb && cb->pause)
				cb->pause(cb_param, flag);
			break;
		case SAVE_TRACKS:
			sync_save_tracks(d);
			break;
		default:
			fprintf(stderr, "unknown cmd: %02x\n", cmd);
			goto sockerr;
		}
	}

	if (cb && cb->is_playing && cb->is_playing(cb_param)) {
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
	return -1;
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

	get_track_data(d, t);
	return t;
}
