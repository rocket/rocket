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

static const char *clientGreeting = "hello, synctracker!";
static const char *serverGreeting = "hello, demo!";

SOCKET clientConnect(SOCKET serverSocket)
{
	SOCKET clientSocket = accept(serverSocket,  NULL, NULL);
	if (INVALID_SOCKET == clientSocket) return INVALID_SOCKET;
	
	const char *expectedGreeting = clientGreeting;
	char recievedGreeting[128];
	
	recv(clientSocket, recievedGreeting, int(strlen(expectedGreeting)), 0);
	if (strncmp(expectedGreeting, recievedGreeting, strlen(expectedGreeting)) != 0)
	{
		closesocket(clientSocket);
		return INVALID_SOCKET;
	}
	
	const char *greeting = serverGreeting;
	send(clientSocket, greeting, int(strlen(greeting)), 0);
	
	return clientSocket;
}

SOCKET serverConnect(struct sockaddr_in *addr)
{
	SOCKET serverSocket = socket( AF_INET, SOCK_STREAM, 0 );
	connect( serverSocket,(struct sockaddr *)addr, sizeof(struct sockaddr_in));
	
	const char *greeting = clientGreeting;
	send(serverSocket, greeting, int(strlen(greeting)), 0);
	
	const char *expectedGreeting = serverGreeting;
	char recievedGreeting[128];
	recv(serverSocket, recievedGreeting, int(sizeof(recievedGreeting)), 0);
	if (strncmp(expectedGreeting, recievedGreeting, strlen(expectedGreeting)) != 0)
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

