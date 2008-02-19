/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#include "device.h"
#include "data.h"
#include "network.h"

using namespace sync;

class ClientDevice : public Device
{
public:
	ClientDevice(const std::string &baseName, SOCKET serverSocket, Timer &timer) :
		Device(baseName),
		timer(timer),
		serverSocket(serverSocket),
		serverRow(-1)
	{
	}
	
	~ClientDevice();
	
	Track &getTrack(const std::string &trackName);
	bool update(float row);
	
private:
	void saveTracks();

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
						char interp;
						
						recv(serverSocket, (char*)&track, sizeof(int), 0);
						recv(serverSocket, (char*)&row,   sizeof(int), 0);
						recv(serverSocket, (char*)&value, sizeof(float), 0);
						recv(serverSocket, (char*)&interp, 1, 0);
						
						assert(interp >= 0);
						assert(interp < Track::KeyFrame::IT_COUNT);
						
						sync::Track &t = *syncData.actualTracks[track];
						t.setKeyFrame(row,
							Track::KeyFrame(
								value,
								Track::KeyFrame::InterpolationType(interp)
							)
						);
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
				
				case SAVE_TRACKS:
					saveTracks();
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

static bool saveTrack(const sync::Track &track, std::string fileName)
{
	FILE *fp = fopen(fileName.c_str(), "wb");
	if (NULL == fp) return false;
	
	size_t keyFrameCount = track.keyFrames.size();
	fwrite(&keyFrameCount, sizeof(size_t), 1, fp);
	
	sync::Track::KeyFrameContainer::const_iterator it;
	for (it = track.keyFrames.begin(); it != track.keyFrames.end(); ++it)
	{
		size_t row = it->first;
		float value = it->second.value;
		char interpolationType = char(it->second.interpolationType);
		
		// write key
		fwrite(&row, sizeof(size_t), 1, fp);
		fwrite(&value, sizeof(float), 1, fp);
		fwrite(&interpolationType, sizeof(char), 1, fp);
	}
	
	fclose(fp);
	fp = NULL;
	return true;
}

void ClientDevice::saveTracks()
{
	sync::Data::TrackContainer::iterator iter;
	for (iter = syncData.tracks.begin(); iter != syncData.tracks.end(); ++iter)
	{
		size_t index = iter->second;
		const sync::Track &track = syncData.getTrack(index);
		saveTrack(track, getTrackFileName(iter->first));
	}
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
	
	ClientDevice *device = new ClientDevice(baseName, serverSocket, timer);
	return device;
}
