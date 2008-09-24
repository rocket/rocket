/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#include "device.h"
#include "data.h"
#include "network.h"

#ifndef REMOTE_HOST
#define REMOTE_HOST "localhost"
#endif
#define REMOTE_PORT 1338

using namespace sync;

class ClientDevice : public Device
{
public:
	ClientDevice(const std::string &baseName, SOCKET serverSocket, Timer &timer) :
		Device(baseName),
		timer(timer),
		serverRow(-1),
		serverSocket(NetworkSocket(serverSocket))
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
	NetworkSocket serverSocket;
};

ClientDevice::~ClientDevice()
{
}

Track &ClientDevice::getTrack(const std::string &trackName)
{
	sync::Data::TrackContainer::iterator iter = syncData.tracks.find(trackName);
	if (iter != syncData.tracks.end()) return syncData.getTrack(iter->second);
	
	// send request data
	unsigned char cmd = GET_TRACK;
	serverSocket.send((char*)&cmd, 1, 0);
	size_t clientIndex = syncData.getTrackCount();
	serverSocket.send((char*)&clientIndex, sizeof(size_t), 0);
	size_t name_len = trackName.size();
	serverSocket.send((char*)&name_len, sizeof(size_t), 0);
	const char *name_str = trackName.c_str();
	serverSocket.send(name_str, int(name_len), 0);
	
	// insert new track
	return syncData.getTrack(trackName);
}

bool ClientDevice::update(float row)
{
	bool done = false;
	// look for new commands
	while (serverSocket.pollRead())
	{
		unsigned char cmd = 0;
		if (serverSocket.recv((char*)&cmd, 1, 0))
		{
			switch (cmd)
			{
				case SET_KEY:
					{
						int track = 0, row = 0;
						float value = 0.0f;
						unsigned char interp = 0;
						
						serverSocket.recv((char*)&track, sizeof(int), 0);
						serverSocket.recv((char*)&row,   sizeof(int), 0);
						serverSocket.recv((char*)&value, sizeof(float), 0);
						serverSocket.recv((char*)&interp, 1, 0);
						if (!serverSocket.connected()) return true;
						
						assert(interp < Track::KeyFrame::IT_COUNT);
						
						sync::Track &t = syncData.getTrack(track);
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
						int track = 0, row = 0;
						serverSocket.recv((char*)&track, sizeof(int), 0);
						serverSocket.recv((char*)&row,   sizeof(int), 0);
						if (!serverSocket.connected()) return true;
						
						sync::Track &t = syncData.getTrack(track);
						t.deleteKeyFrame(row);
					}
					break;
				
				case SET_ROW:
					{
						int row;
						serverSocket.recv((char*)&row,   sizeof(int), 0);
						if (!serverSocket.connected()) return true;
						timer.setRow(float(row));
					}
					break;
					
				case PAUSE:
					{
						char flag;
						serverSocket.recv((char*)&flag, 1, 0);
						if (!serverSocket.connected()) return true;
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
		if (serverRow != newServerRow && serverSocket.connected())
		{
			unsigned char cmd = SET_ROW;
			serverSocket.send((char*)&cmd, 1, 0);
			serverSocket.send((char*)&newServerRow, sizeof(int), 0);
			serverRow = newServerRow;
		}
	}
	
	return true;
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
	
	struct hostent *host = gethostbyname(REMOTE_HOST);
	if (NULL == host) return NULL;
	printf("IP address: %s\n", inet_ntoa(*(struct in_addr*)host->h_addr_list[0]));
	
	struct sockaddr_in sain;
	sain.sin_family = AF_INET;
	sain.sin_port = htons(REMOTE_PORT);
	sain.sin_addr.s_addr = ((struct in_addr *)(host->h_addr_list[0]))->s_addr;
	
	// connect to server
	SOCKET serverSocket = serverConnect(&sain);
	if (INVALID_SOCKET == serverSocket) return NULL;
	
	ClientDevice *device = new ClientDevice(baseName, serverSocket, timer);
	return device;
}
