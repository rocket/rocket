#ifndef SYNC_PLAYER

#include "device.h"
#include "sync.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct sync_tcp {
	SOCKET sock;
};

#ifdef USE_AMITCP
static struct Library *socket_base = NULL;
#endif

static SOCKET server_connect(const char *host, unsigned short nport)
{
	SOCKET sock = INVALID_SOCKET;
#ifdef USE_GETADDRINFO
	struct addrinfo *addr, *curr;
	char port[6];
#else
	struct hostent *he;
	char **ap;
#endif

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

#ifdef USE_GETADDRINFO

	snprintf(port, sizeof(port), "%u", nport);
	if (getaddrinfo(host, port, 0, &addr) != 0)
		return INVALID_SOCKET;

	for (curr = addr; curr; curr = curr->ai_next) {
		int family = curr->ai_family;
		struct sockaddr *sa = curr->ai_addr;
		int sa_len = (int)curr->ai_addrlen;

#else

	he = gethostbyname(host);
	if (!he)
		return INVALID_SOCKET;

	for (ap = he->h_addr_list; *ap; ++ap) {
		int family = he->h_addrtype;
		struct sockaddr_in sin;
		struct sockaddr *sa = (struct sockaddr *)&sin;
		int sa_len = sizeof(*sa);

		sin.sin_family = he->h_addrtype;
		sin.sin_port = htons(nport);
		memcpy(&sin.sin_addr, *ap, he->h_length);
		memset(&sin.sin_zero, 0, sizeof(sin.sin_zero));

#endif
		sock = socket(family, SOCK_STREAM, 0);
		if (sock == INVALID_SOCKET)
			continue;

		if (connect(sock, sa, sa_len) >= 0) {
#ifdef USE_NODELAY
			int yes = 1;

			/* Try disabling Nagle since we're latency-sensitive, UDP would
			 * really be more appropriate but that's a much bigger change.
			 */
			(void) setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void *)&yes, sizeof(yes));
#endif
			break;
		}

		closesocket(sock);
		sock = INVALID_SOCKET;
	}

#ifdef USE_GETADDRINFO
	freeaddrinfo(addr);
#endif

	return sock;
}

static int sync_tcp_poll(void *ctxt, int *res_readable, int *res_writeable)
{
	struct sync_tcp *tcp = ctxt;
#ifdef GEKKO
	// libogc doesn't impmelent select()...
	struct pollsd sds[1];
	int events = (res_readable ? POLLIN : 0) | (res_writeable ? POLLOUT : 0)
	sds[0].socket  = tcp->sock;
	sds[0].events  = events;
	sds[0].revents = 0;
	if (net_poll(sds, 1, 0) < 0) return 0;
	if (res_readable)
		*res_readable = (sds[0].revents & POLLIN) && !(sds[0].revents & (POLLERR|POLLHUP|POLLNVAL));
	if (res_writeable)
		*res_writeable = (sds[0].revents & POLLOUT) && !(sds[0].revents & (POLLERR|POLLHUP|POLLNVAL));
	return 1;
#else
	struct timeval to = { 0, 0 };
	fd_set rfds, wfds;
	int r;

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4127)
#endif
	if (res_readable)
		FD_SET(tcp->sock, &rfds);
	if (res_writeable)
		FD_SET(tcp->sock, &wfds);
#ifdef _MSC_VER
#pragma warning(pop)
#endif

	r = select((int)tcp->sock + 1, &rfds, &wfds, NULL, &to);
	if (r > 0) {
		if (res_readable)
			*res_readable = FD_ISSET(tcp->sock, &rfds);
		if (res_writeable)
			*res_writeable = FD_ISSET(tcp->sock, &wfds);
	}
	return r;

#endif
}

static int sync_tcp_send(void *ctxt, const void *buf, int len)
{
	struct sync_tcp *tcp = ctxt;

#ifdef WIN32
	assert(len <= INT_MAX);
	return send(tcp->sock, (const char *)buf, (int)len, 0);
#else
	return send(tcp->sock, (const char *)buf, len, 0);
#endif
}

static int sync_tcp_recv(void *ctxt, void *buf, int len)
{
	struct sync_tcp *tcp = ctxt;

#ifdef WIN32
	assert(len <= INT_MAX);
	return recv(tcp->sock, (char *)buf, (int)len, 0);
#else
	return recv(tcp->sock, (char *)buf, len, 0);
#endif
}

static void sync_tcp_close(void *ctxt)
{
	struct sync_tcp *tcp = ctxt;

	closesocket(tcp->sock);
	free(tcp);
}

static struct sync_sockio_cb sync_tcp_sockio = {
	.poll = sync_tcp_poll,
	.send = sync_tcp_send,
	.recv = sync_tcp_recv,
	.close = sync_tcp_close,
};

int sync_tcp_connect(struct sync_device *d, const char *host, unsigned short port)
{
	struct sync_tcp *tcp;

	tcp = malloc(sizeof(*tcp));
	if (!tcp)
		return -1;

	tcp->sock = server_connect(host, port);
	if (tcp->sock == INVALID_SOCKET) {
		free(tcp);
		return -1;
	}

	return sync_set_sockio_cb(d, &sync_tcp_sockio, tcp);
}

int sync_connect(struct sync_device *d, const char *host, unsigned short port)
{
	return sync_tcp_connect(d, host, port);
}

void sync_tcp_device_dtor(void)
{
	/* XXX: this is janky */
#if defined(USE_AMITCP)
	if (socket_base) {
		CloseLibrary(socket_base);
		socket_base = NULL;
	}
#endif
}

#endif /* !defined(SYNC_PLAYER) */
