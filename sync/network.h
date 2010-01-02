/* Copyright (C) 2007-2010 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#ifndef SYNC_NETWORK_H
#define SYNC_NETWORK_H

#ifdef WIN32
 #include <winsock2.h>
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
#else
 #include <sys/socket.h>
 #include <netinet/in.h>
 #include <netdb.h>
 #define SOCKET int
 #define INVALID_SOCKET -1
#endif

#include "base.h"

static const char *client_greet = "hello, synctracker!";
static const char *server_greet = "hello, demo!";

enum {
	SET_KEY = 0,
	DELETE_KEY = 1,
	GET_TRACK = 2,
	SET_ROW = 3,
	PAUSE = 4,
	SAVE_TRACKS = 5
};

static inline int init_network()
{
#ifdef WIN32
	WSADATA wsa;
	if (0 != WSAStartup(MAKEWORD(2, 0), &wsa))
		return 0;
	if (LOBYTE(wsa.wVersion) != 2 || HIBYTE(wsa.wVersion) != 0)
		return 0;
#endif

	return 1;
}

static inline void close_network()
{
#ifdef WIN32
	WSACleanup();
#endif
}

static inline int socket_poll(SOCKET socket)
{
	struct timeval to = { 0, 0 };
	fd_set fds;

	FD_ZERO(&fds);
	FD_SET(socket, &fds);

	return select(0, &fds, NULL, NULL, &to) > 0;
}

#endif /* SYNC_NETWORK_H */
