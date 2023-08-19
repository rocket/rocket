#ifndef SYNC_DEVICE_H
#define SYNC_DEVICE_H

#include "base.h"
#include "sync.h"

#ifndef SYNC_PLAYER

/* configure socket-stack */
#ifdef _WIN32
 #define WIN32_LEAN_AND_MEAN
 #define USE_GETADDRINFO
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <winsock2.h>
 #include <ws2tcpip.h>
 #include <windows.h>
 #include <limits.h>
#elif defined(USE_AMITCP)
 #include <sys/socket.h>
 #include <proto/exec.h>
 #include <proto/socket.h>
 #include <netdb.h>
 #define SOCKET int
 #define INVALID_SOCKET -1
 #define select(n,r,w,e,t) WaitSelect(n,r,w,e,t,0)
 #define closesocket(x) CloseSocket(x)
#elif defined(GEKKO)
 #include <gccore.h>
 #include <network.h>
 #define SOCKET s32
 #ifndef INVALID_SOCKET
  #define INVALID_SOCKET (-1)
 #endif
 #define closesocket(x) net_close(x)
 #define send(s,b,l,f) net_send(s,b,l,f)
 #define recv(s,b,l,f) net_recv(s,b,l,f)
 #ifdef USE_GETADDRINFO
  #error "getaddrinfo and libogc are incompatible with each other!"
 #endif
 #define gethostbyname(h) net_gethostbyname(h)
 #define socket(f,t,x) net_socket(f,t,x)
 #define connect(s,a,l) net_connect(s,a,l)
#else
 #include <sys/socket.h>
 #include <sys/time.h>
 #include <netinet/in.h>
 #ifdef USE_NODELAY
  #include <netinet/tcp.h>
 #endif
 #include <netdb.h>
 #include <unistd.h>
 #define SOCKET int
 #define INVALID_SOCKET -1
 #define closesocket(x) close(x)
#endif

#endif /* !defined(SYNC_PLAYER) */

struct sync_device {
	char *base;
	struct sync_track **tracks;
	size_t num_tracks;

#ifndef SYNC_PLAYER
	int row;
	struct sync_sockio_cb sockio_cb;
	void *sockio_ctxt;
#endif
	struct sync_io_cb io_cb;
};

#endif /* SYNC_DEVICE_H */
