#include <stdio.h>
#include "network.h"

/*
struct hostent *getHost()
{
	
}*/

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
	
	puts("recieving...");
	bool done = false;
	while (!done)
	{
		// look for new commands
		while (pollRead(serverSocket))
		{
			unsigned char cmd = 0;
			int ret = recv(serverSocket, (char*)&cmd, 1, 0);
			if (0 == ret)
			{
				done = true;
				break;
			}
			else
			{
				switch (cmd)
				{
					case 1:
						printf("yes, master!\n");
						{
							unsigned char cmd = 0x1;
							send(serverSocket, (char*)&cmd, 1, 0);
						}
					break;
					
					default:
						printf("unknown cmd: %02x\n", cmd);
				}
			}
		}
		
		putchar('.');
	}
	closesocket(serverSocket);
	
	closeNetwork();


	return 0;
}
