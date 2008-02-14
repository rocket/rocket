#include "device.h"
#include "../network.h"
#include "../syncdata.h"

using namespace sync;

class ClientDevice : public Device
{
public:
	ClientDevice(const std::string &baseName, SOCKET serverSocket, Timer &timer) :
		baseName(baseName),
		timer(timer),
		serverSocket(serverSocket)
	{
	}
	
	~ClientDevice();
	
	Track &getTrack(const std::string &trackName);
	bool update(float row);
	
private:
	const std::string &baseName;
	SyncData syncData;
	Timer &timer;
	
	SOCKET serverSocket;
};

ClientDevice::~ClientDevice()
{
	
}

Track &ClientDevice::getTrack(const std::string &trackName)
{
	SyncData::TrackContainer::iterator iter = syncData.tracks.find(trackName);
	if (iter != syncData.tracks.end()) return *syncData.actualTracks[iter->second];
		
	unsigned char cmd = GET_TRACK;
	send(serverSocket, (char*)&cmd, 1, 0);

	size_t clientIndex = syncData.actualTracks.size();
	send(serverSocket, (char*)&clientIndex, sizeof(size_t), 0);
	
	// send request data
	size_t name_len = trackName.size();
	printf("len: %d\n", name_len);
	send(serverSocket, (char*)&name_len, sizeof(size_t), 0);
	
	const char *name_str = trackName.c_str();
	send(serverSocket, name_str, name_len, 0);
	
	sync::Track *track = new sync::Track();

	syncData.actualTracks.push_back(track);
	syncData.tracks[trackName] = clientIndex;
	return *track;
}

bool ClientDevice::update(float row)
{
	bool done = false;
	// look for new commands
	while (pollRead(serverSocket))
	{
		unsigned char cmd = 0;
		int ret = recv(serverSocket, (char*)&cmd, 1, 0);
		if (0 >= ret)
		{
			done = true;
			break;
		}
		else
		{
			switch (cmd)
			{
				case SET_KEY:
					{
						int track, row;
						float value;
						recv(serverSocket, (char*)&track, sizeof(int), 0);
						recv(serverSocket, (char*)&row,   sizeof(int), 0);
						recv(serverSocket, (char*)&value, sizeof(float), 0);
						printf("set: %d,%d = %f\n", track, row, value);
					}
					break;
				
				case DELETE_KEY:
					{
						int track, row;
						recv(serverSocket, (char*)&track, sizeof(int), 0);
						recv(serverSocket, (char*)&row,   sizeof(int), 0);
						printf("delete: %d,%d = %f\n", track, row);
					}
					break;
				
				default:
					printf("unknown cmd: %02x\n", cmd);
			}
		}
	}
	return !done;
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
