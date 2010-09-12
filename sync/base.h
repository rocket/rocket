/* Copyright (C) 2007-2010 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in COPYING
 */

#ifndef SYNC_BASE_H
#define SYNC_BASE_H

#ifdef _MSC_VER
 #define inline __inline
 #define strdup _strdup
 #define snprintf _snprintf
 typedef unsigned int uint32_t;
#elif defined(__GNUC__)
 #define inline __inline
 #include <stdint.h>
#endif

#ifdef _WIN32
 #define WIN32_LEAN_AND_MEAN
 #define NOMINMAX
 #include <winsock2.h>
 #include <windows.h>
#else
 #include <sys/socket.h>
 #include <sys/time.h>
 #include <netinet/in.h>
 #include <netdb.h>
 #include <unistd.h>
 #define SOCKET int
 #define INVALID_SOCKET -1
 #define closesocket(x) close(x)
#endif

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

static inline int socket_poll(SOCKET socket)
{
	struct timeval to = { 0, 0 };
	fd_set fds;

	FD_ZERO(&fds);

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4127)
#endif
	FD_SET(socket, &fds);
#ifdef _MSC_VER
#pragma warning(pop)
#endif

	return select(socket + 1, &fds, NULL, NULL, &to) > 0;
}

static inline int xsend(SOCKET s, const void *buf, size_t len, int flags)
{
	return send(s, (const char *)buf, len, flags) != (int)len;
}

static inline int xrecv(SOCKET s, void *buf, size_t len, int flags)
{
	return recv(s, (char *)buf, len, flags) != (int)len;
}

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

#endif /* SYNC_BASE_H */
