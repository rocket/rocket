#include "network.h"

bool initNetwork()
{
	WSADATA wsaData;
	if (0 != WSAStartup(MAKEWORD( 2, 0 ), &wsaData)) return false;
	if (LOBYTE( wsaData.wVersion ) != 2 || HIBYTE( wsaData.wVersion ) != 0) return false;
	return true;
}

void closeNetwork()
{
	WSACleanup();
}

SOCKET clientConnect(SOCKET serverSocket)
{
	SOCKET clientSocket = accept(serverSocket,  NULL, NULL);
	if (INVALID_SOCKET == clientSocket) return INVALID_SOCKET;
	
	const char *expectedHandshake = "hello, synctracker!";
	char recievedHandshake[19];
	
	recv(clientSocket, recievedHandshake, int(strlen(expectedHandshake)), 0);
	if (strncmp(expectedHandshake, recievedHandshake, strlen(expectedHandshake)) != 0)
	{
		closesocket(clientSocket);
		return INVALID_SOCKET;
	}
	
	const char *test = "hello, demo!";
	send(clientSocket, test, int(strlen(test)), 0);
	
	return clientSocket;
}

SOCKET serverConnect(struct sockaddr_in *addr)
{
	SOCKET serverSocket = socket( AF_INET, SOCK_STREAM, 0 );
	connect( serverSocket,(struct sockaddr *)addr, sizeof(struct sockaddr_in));
	
	const char * request_text = "hello, synctracker!";
	send(serverSocket, request_text, int(strlen(request_text)), 0);
	
	const char *expectedHandshake = "hello, demo!";
	char recievedHandshake[12];
	recv(serverSocket, recievedHandshake, int(sizeof(recievedHandshake)), 0);
	if (strncmp(expectedHandshake, recievedHandshake, strlen(expectedHandshake)) != 0)
	{
		closesocket(serverSocket);
		return INVALID_SOCKET;
 	}
 	
 	return serverSocket;
}

bool pollRead(SOCKET socket)
{
	struct timeval timeout = { 0, 0 };
	fd_set fds;
	
	FD_ZERO(&fds);
	FD_SET(socket, &fds);
	
	// look for new commands
	return select(0, &fds, NULL, NULL, &timeout) > 0;
}

