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

enum RemoteCommand
{
	// server -> client
	SET_KEY,
	DELETE_KEY,
	
	// client -> server
	GET_TRACK,
	
	// client -> server, server -> client
	SET_ROW,
	
	// server -> client
	PAUSE,
	SAVE_TRACKS
};

#endif /* NETWORK_H */
