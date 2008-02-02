#ifndef NETWORK_H
#define NETWORK_H

#include <winsock2.h>

bool initNetwork();
void closeNetwork();

SOCKET clientConnect(SOCKET serverSocket);
SOCKET serverConnect(struct sockaddr_in *addr);

bool pollRead(SOCKET socket);

#endif /* NETWORK_H */
