#ifndef NETWORK_H
#define NETWORK_H

#include <winsock2.h>
#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>

bool initNetwork();
void closeNetwork();

SOCKET clientConnect(SOCKET serverSocket);
SOCKET serverConnect(struct sockaddr_in *addr);

bool pollRead(SOCKET socket);

enum RemoteCommand {
	// server -> client
	SET_KEY = 0,
	DELETE_KEY = 1,
	
	// client -> server
	GET_TRACK = 2,
	
	// client -> server, server -> client
	SET_ROW = 3,
	
	// server -> client
	PAUSE = 4,
};

#endif /* NETWORK_H */
