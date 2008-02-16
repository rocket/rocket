#include "device.h"
#include "data.h"
#include "network.h"

using namespace sync;

class ClientDevice : public Device
{
public:
	ClientDevice(const std::string &baseName, SOCKET serverSocket, Timer &timer) :
		baseName(baseName),
		timer(timer),
		serverSocket(serverSocket),
		serverRow(-1)
	{
	}
	
	~ClientDevice();
	
	Track &getTrack(const std::string &trackName);
	bool update(float row);
	
private:
	const std::string &baseName;
	sync::Data syncData;
	Timer &timer;
	
	int    serverRow;
	SOCKET serverSocket;
};

ClientDevice::~ClientDevice()
{
}

Track &ClientDevice::getTrack(const std::string &trackName)
{
	sync::Data::TrackContainer::iterator iter = syncData.tracks.find(trackName);
	if (iter != syncData.tracks.end()) return *syncData.actualTracks[iter->second];
		
	unsigned char cmd = GET_TRACK;
	send(serverSocket, (char*)&cmd, 1, 0);
	
	size_t clientIndex = syncData.actualTracks.size();
	send(serverSocket, (char*)&clientIndex, sizeof(size_t), 0);
	
	// send request data
	size_t name_len = trackName.size();
	send(serverSocket, (char*)&name_len, sizeof(size_t), 0);
	
	const char *name_str = trackName.c_str();
	send(serverSocket, name_str, int(name_len), 0);
	
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
						
						sync::Track &t = syncData.getTrack(track);
						t.setKeyFrame(row, Track::KeyFrame(value));
					}
					break;
				
				case DELETE_KEY:
					{
						int track, row;
						recv(serverSocket, (char*)&track, sizeof(int), 0);
						recv(serverSocket, (char*)&row,   sizeof(int), 0);
						
						sync::Track &t = syncData.getTrack(track);
						t.deleteKeyFrame(row);
					}
					break;
				
				case SET_ROW:
					{
						int row;
						recv(serverSocket, (char*)&row,   sizeof(int), 0);
						timer.setRow(float(row));
					}
					break;
					
				case PAUSE:
					{
						char flag;
						recv(serverSocket, (char*)&flag, 1, 0);
						if (flag == 0) timer.play();
						else           timer.pause();
					}
					break;
				
				default:
					assert(false);
					fprintf(stderr, "unknown cmd: %02x\n", cmd);
			}
		}
	}
	
	if (timer.isPlaying())
	{
		int newServerRow = int(floor(row));
		if (serverRow != newServerRow)
		{
			unsigned char cmd = SET_ROW;
			send(serverSocket, (char*)&cmd, 1, 0);
			send(serverSocket, (char*)&newServerRow, sizeof(int), 0);
			serverRow = newServerRow;
		}
	}
	
	return !done;
}

Device *sync::createDevice(const std::string &baseName, Timer &timer)
{
	if (false == initNetwork()) return NULL;
	
	struct hostent *myhost = gethostbyname("localhost");
	struct sockaddr_in sain;
	sain.sin_family = AF_INET;
	sain.sin_port = htons(1338);
	sain.sin_addr.s_addr= *( (unsigned long *)(myhost->h_addr_list[0]) );
	
	// connect to server
	SOCKET serverSocket = serverConnect(&sain);
	if (INVALID_SOCKET == serverSocket) return NULL;
	
	Device *device = new ClientDevice(baseName, serverSocket, timer);
	return device;
}
