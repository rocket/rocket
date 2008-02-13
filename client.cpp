#include <stdio.h>
#include "network.h"
#include "syncdataclient.h"

int main(int argc, char *argv[])
{
	if (false == initNetwork())
	{
		fputs("Failed to init WinSock", stderr);
		exit(1);
	}
	
    struct hostent * myhost = gethostbyname("localhost");
    printf("Found IP Address!\n");

    struct sockaddr_in sain;
    sain.sin_family = AF_INET;
    sain.sin_port = htons(1338);
    sain.sin_addr.s_addr= *( (unsigned long *)(myhost->h_addr_list[0]) );
	SOCKET serverSocket = serverConnect(&sain);
	
	if (INVALID_SOCKET == serverSocket)
	{
		puts("connection failed.");
		exit(-1);
	}
	
	SyncDataClient syncData(serverSocket);
	SyncTrack &track = syncData.getTrack("test");
	SyncTrack &track2 = syncData.getTrack("test2");
	
	puts("recieving...");
	bool done = false;
	while (!done)
	{
//		putchar('.');
		done = syncData.poll();
	}
	closesocket(serverSocket);
	
	closeNetwork();


	return 0;
}
