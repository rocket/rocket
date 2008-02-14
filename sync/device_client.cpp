#include "device.h"
#include "../network.h"
#include "../syncdataclient.h"

using namespace sync;

class ClientDevice : public Device
{
public:
	ClientDevice(SOCKET serverSocket, Timer &timer) : syncData(serverSocket), timer(timer) {}
	~ClientDevice();

	Track &getTrack(const std::string &trackName);
	bool update(float row);
	
private:
	SyncDataClient syncData;
	Timer &timer;
};

ClientDevice::~ClientDevice()
{
	
}

Track &ClientDevice::getTrack(const std::string &trackName)
{
	return syncData.getTrack(trackName);
}

bool ClientDevice::update(float row)
{
	return !syncData.poll();
}

Device *sync::createDevice(const std::string &baseName, Timer &timer)
{
	if (false == initNetwork())
	{
		printf("noes 1!\n");
		return NULL;
	}
	
	struct hostent *myhost = gethostbyname("localhost");
	struct sockaddr_in sain;
	sain.sin_family = AF_INET;
	sain.sin_port = htons(1338);
	sain.sin_addr.s_addr= *( (unsigned long *)(myhost->h_addr_list[0]) );
	
	// connect to server
	SOCKET serverSocket = serverConnect(&sain);
	if (INVALID_SOCKET == serverSocket)
	{
		printf("noes 2!\n");
		return NULL;
	}
	
	Device *device = new ClientDevice(serverSocket, timer);

	return device;
}
