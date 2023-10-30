#include "device.h"
#include "track.h"
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>

#ifdef WIN32
 #include <direct.h>
 #define S_ISDIR(m) (((m)& S_IFMT) == S_IFDIR)
 #define mkdir(pathname, mode) _mkdir(pathname)
#endif

void sync_tcp_device_dtor(void); /* not worth adding a tcp.h for */

static int find_track(struct sync_device *d, const char *name)
{
	int i;
	for (i = 0; i < (int)d->num_tracks; ++i)
		if (!strcmp(name, d->tracks[i]->name))
			return i;
	return -1; /* not found */
}

static int valid_path_char(int ch)
{
	switch (ch) {
	case '.':
	case '_':
	case '/':
		return 1;

	default:
		return isalnum(ch);
	}
}

static const char *path_encode(const char *path)
{
	static char temp[FILENAME_MAX];
	int i, pos = 0;
	int path_len = (int)strlen(path);
	for (i = 0; i < path_len; ++i) {
		int ch = path[i];
		if (valid_path_char(ch)) {
			if (pos >= sizeof(temp) - 1)
				break;

			temp[pos++] = (char)ch;
		} else {
			if (pos >= sizeof(temp) - 3)
				break;

			temp[pos++] = '-';
			temp[pos++] = "0123456789ABCDEF"[(ch >> 4) & 0xF];
			temp[pos++] = "0123456789ABCDEF"[ch & 0xF];
		}
	}

	temp[pos] = '\0';
	return temp;
}

static const char *sync_track_path(const char *base, const char *name)
{
	static char temp[FILENAME_MAX];
	strncpy(temp, base, sizeof(temp) - 1);
	temp[sizeof(temp) - 1] = '\0';
	strncat(temp, "_", sizeof(temp) - strlen(temp) - 1);
	strncat(temp, path_encode(name), sizeof(temp) - strlen(temp) - 1);
	strncat(temp, ".track", sizeof(temp) - strlen(temp) - 1);
	return temp;
}

#ifndef SYNC_PLAYER

#define CLIENT_GREET "hello, synctracker!"
#define SERVER_GREET "hello, demo!"

enum {
	SET_KEY = 0,
	DELETE_KEY = 1,
	GET_TRACK = 2,
	SET_ROW = 3,
	PAUSE = 4,
	SAVE_TRACKS = 5
};

static inline int sockio_poll(struct sync_device *d, int *res_readable, int *res_writeable)
{
	assert(res_readable || res_writeable);
	return d->sockio_cb.poll(d->sockio_ctxt, res_readable, res_writeable);
}

static inline int sockio_send(struct sync_device *d, const void *buf, int len)
{
	assert(len > 0);
	return d->sockio_cb.send(d->sockio_ctxt, buf, len) != len;
}

static inline int sockio_recv(struct sync_device *d, void *buf, int len)
{
	assert(len > 0);
	return d->sockio_cb.recv(d->sockio_ctxt, buf, len) != len;
}

static inline void sockio_close(struct sync_device *d)
{
	d->sockio_cb.close(d->sockio_ctxt);
	d->sockio_ctxt = NULL;
}

#else

void sync_set_io_cb(struct sync_device *d, struct sync_io_cb *cb)
{
	d->io_cb.open = cb->open;
	d->io_cb.read = cb->read;
	d->io_cb.close = cb->close;
}

#endif

#ifdef NEED_STRDUP
static inline char *rocket_strdup(const char *str)
{
	char *ret = malloc(strlen(str) + 1);
	if (ret)
		strcpy(ret, str);
	return ret;
}
#define strdup rocket_strdup
#endif

struct sync_device *sync_create_device(const char *base)
{
	struct sync_device *d = malloc(sizeof(*d));
	if (!d)
		return NULL;

	if (!base || base[0] == '/')
		return NULL;

	d->base = strdup(path_encode(base));
	if (!d->base) {
		free(d);
		return NULL;
	}

	d->tracks = NULL;
	d->num_tracks = 0;

#ifndef SYNC_PLAYER
	d->row = -1;
	d->sockio_ctxt = NULL;
#endif

	d->io_cb.open = (void *(*)(const char *, const char *))fopen;
	d->io_cb.read = (size_t (*)(void *, size_t, size_t, void *))fread;
	d->io_cb.close = (int (*)(void *))fclose;

	return d;
}

void sync_destroy_device(struct sync_device *d)
{
	int i;

#ifndef SYNC_PLAYER
	if (d->sockio_ctxt)
		sockio_close(d);

	sync_tcp_device_dtor();
#endif

	for (i = 0; i < (int)d->num_tracks; ++i) {
		free(d->tracks[i]->name);
		free(d->tracks[i]->keys);
		free(d->tracks[i]);
	}
	free(d->tracks);
	free(d->base);
	free(d);
}

static int read_track_data(struct sync_device *d, struct sync_track *t)
{
	int i;
	void *fp = d->io_cb.open(sync_track_path(d->base, t->name), "rb");
	if (!fp)
		return -1;

	d->io_cb.read(&t->num_keys, sizeof(int), 1, fp);
	t->keys = malloc(sizeof(struct track_key) * t->num_keys);
	if (!t->keys)
		return -1;

	for (i = 0; i < (int)t->num_keys; ++i) {
		struct track_key *key = t->keys + i;
		char type;
		d->io_cb.read(&key->row, sizeof(int), 1, fp);
		d->io_cb.read(&key->value, sizeof(float), 1, fp);
		d->io_cb.read(&type, sizeof(char), 1, fp);
		key->type = (enum key_type)type;
	}

	d->io_cb.close(fp);
	return 0;
}

static int create_leading_dirs(const char *path)
{
	char *pos, buf[FILENAME_MAX];

	strncpy(buf, path, sizeof(buf));
	buf[sizeof(buf) - 1] = '\0';
	pos = buf;

	while (1) {
		struct stat st;

		pos = strchr(pos, '/');
		if (!pos)
			break;
		*pos = '\0';

		/* does path exist, but isn't a dir? */
		if (!stat(buf, &st)) {
			if (!S_ISDIR(st.st_mode))
				return -1;
		} else {
			if (mkdir(buf, 0777))
				return -1;
		}

		*pos++ = '/';
	}

	return 0;
}

static int save_track(const struct sync_track *t, const char *path)
{
	int i;
	FILE *fp;

	if (create_leading_dirs(path))
		return -1;

	fp = fopen(path, "wb");
	if (!fp)
		return -1;

	fwrite(&t->num_keys, sizeof(int), 1, fp);
	for (i = 0; i < (int)t->num_keys; ++i) {
		char type = (char)t->keys[i].type;
		fwrite(&t->keys[i].row, sizeof(int), 1, fp);
		fwrite(&t->keys[i].value, sizeof(float), 1, fp);
		fwrite(&type, sizeof(char), 1, fp);
	}

	fclose(fp);
	return 0;
}

int sync_save_tracks(const struct sync_device *d)
{
	int i;
	for (i = 0; i < (int)d->num_tracks; ++i) {
		const struct sync_track *t = d->tracks[i];
		if (save_track(t, sync_track_path(d->base, t->name)))
			return -1;
	}
	return 0;
}

#ifndef SYNC_PLAYER

static int fetch_track_data(struct sync_device *d, struct sync_track *t)
{
	unsigned char cmd = GET_TRACK;
	uint32_t name_len;

	assert(strlen(t->name) <= UINT32_MAX);
	name_len = htonl((uint32_t)strlen(t->name));

	/* send request data */
	if (sockio_send(d, (char *)&cmd, 1) ||
	    sockio_send(d, (char *)&name_len, sizeof(name_len)) ||
	    sockio_send(d, t->name, (int)strlen(t->name)))
	{
		sockio_close(d);
		return -1;
	}

	return 0;
}

static int handle_set_key_cmd(struct sync_device *d)
{
	uint32_t track, row;
	union {
		float f;
		uint32_t i;
	} v;
	struct track_key key;
	unsigned char type;

	if (sockio_recv(d, (char *)&track, sizeof(track)) ||
	    sockio_recv(d, (char *)&row, sizeof(row)) ||
	    sockio_recv(d, (char *)&v.i, sizeof(v.i)) ||
	    sockio_recv(d, (char *)&type, 1))
		return -1;

	track = ntohl(track);
	v.i = ntohl(v.i);

	key.row = ntohl(row);
	key.value = v.f;

	if (type >= KEY_TYPE_COUNT || track >= d->num_tracks)
		return -1;

	key.type = (enum key_type)type;
	return sync_set_key(d->tracks[track], &key);
}

static int handle_del_key_cmd(struct sync_device *d)
{
	uint32_t track, row;

	if (sockio_recv(d, (char *)&track, sizeof(track)) ||
	    sockio_recv(d, (char *)&row, sizeof(row)))
		return -1;

	track = ntohl(track);
	row = ntohl(row);

	if (track >= d->num_tracks)
		return -1;

	return sync_del_key(d->tracks[track], row);
}

static int server_greet(struct sync_device *d)
{
	char greet[128];
	int i;

	if (sockio_send(d, CLIENT_GREET, (int)strlen(CLIENT_GREET)) ||
	    sockio_recv(d, greet, (int)strlen(SERVER_GREET)) ||
	    strncmp(SERVER_GREET, greet, strlen(SERVER_GREET))) {
		sockio_close(d);
		return -1;
	}

	for (i = 0; i < (int)d->num_tracks; ++i) {
		free(d->tracks[i]->keys);
		d->tracks[i]->keys = NULL;
		d->tracks[i]->num_keys = 0;
	}

	for (i = 0; i < (int)d->num_tracks; ++i) {
		if (fetch_track_data(d, d->tracks[i])) {
			sockio_close(d);
			return -1;
		}
	}

	return 0;
}

/* Setting a new cb+ctxt establishes a new abstract connection represented by ctxt.
 * How the connection communicates is implemented by the .recv()/.send() members.
 * How the connection ends and cleans itself up is implemented by .close().
 * ctxt must already be allocated, connected, and ready to be used with the provided methods.
 * The device takes ownership of ctxt, to be cleaned up by the .close() method to disconnect.
 *
 * Returns 0 on success, -1 on error, which may occur since this performs the
 * initial Rocket handshake.  Even in error ctxt will be closed.
 *
 * This is the low-level communications api, only use this if the sync_tcp_connect() style
 * helper is insufficient for your needs.  They are mutually exclusive.
 */
int sync_set_sockio_cb(struct sync_device *d, struct sync_sockio_cb *cb, void *ctxt)
{
	assert(ctxt);
	assert(cb->send);
	assert(cb->recv);
	assert(cb->close);

	if (d->sockio_ctxt)
		sockio_close(d);

	d->sockio_cb = *cb;
	d->sockio_ctxt = ctxt;

	return server_greet(d);
}

int sync_update(struct sync_device *d, int row, struct sync_cb *cb,
    void *cb_param)
{
	int readable;

	if (!d->sockio_ctxt)
		return -1;

	/* look for new commands */
	while (sockio_poll(d, &readable, NULL) > 0) {
		unsigned char cmd = 0, flag;
		uint32_t new_row;

		if (sockio_recv(d, (char *)&cmd, 1))
			goto sockerr;

		switch (cmd) {
		case SET_KEY:
			if (handle_set_key_cmd(d))
				goto sockerr;
			break;
		case DELETE_KEY:
			if (handle_del_key_cmd(d))
				goto sockerr;
			break;
		case SET_ROW:
			if (sockio_recv(d, (char *)&new_row, sizeof(new_row)))
				goto sockerr;
			if (cb && cb->set_row)
				cb->set_row(cb_param, ntohl(new_row));
			break;
		case PAUSE:
			if (sockio_recv(d, (char *)&flag, 1))
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
		if (d->row != row && d->sockio_ctxt) {
			unsigned char cmd = SET_ROW;
			uint32_t nrow = htonl(row);
			if (sockio_send(d, (char*)&cmd, 1) ||
			    sockio_send(d, (char*)&nrow, sizeof(nrow)))
				goto sockerr;
			d->row = row;
		}
	}
	return 0;

sockerr:
	sockio_close(d);
	return -1;
}

#endif /* !defined(SYNC_PLAYER) */

static int create_track(struct sync_device *d, const char *name)
{
	void *tmp;
	struct sync_track *t;
	assert(find_track(d, name) < 0);

	t = malloc(sizeof(*t));
	if (!t)
		return -1;

	t->name = strdup(name);
	t->keys = NULL;
	t->num_keys = 0;

	tmp = realloc(d->tracks, sizeof(d->tracks[0]) * (d->num_tracks + 1));
	if (!tmp) {
		free(t);
		return -1;
	}

	d->tracks = tmp;
	d->tracks[d->num_tracks++] = t;

	return (int)d->num_tracks - 1;
}

const struct sync_track *sync_get_track(struct sync_device *d,
    const char *name)
{
	struct sync_track *t;
	int idx = find_track(d, name);
	if (idx >= 0)
		return d->tracks[idx];

	idx = create_track(d, name);
	if (idx < 0)
		return NULL;

	t = d->tracks[idx];

#ifndef SYNC_PLAYER
	if (d->sockio_ctxt)
		fetch_track_data(d, t);
	else
#endif
		read_track_data(d, t);

	return t;
}
