#ifndef NETWORK_H
#define NETWORK_H

#include <winsock2.h>

bool initNetwork();
void closeNetwork();

SOCKET clientConnect(SOCKET serverSocket);
SOCKET serverConnect(struct sockaddr_in *addr);

bool pollRead(SOCKET socket);

enum RemoteCommand {
	SET_KEY = 0,
	DELETE_KEY = 1,
	GET_TRACK = 2,
	SET_ROW = 3,
};

#endif /* NETWORK_H */
