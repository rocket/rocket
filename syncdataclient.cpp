#include "syncdataclient.h"
#include "network.h"

sync::Track &SyncDataClient::getTrack(const std::basic_string<TCHAR> &name)
{
	TrackContainer::iterator iter = tracks.find(name);
	if (iter != tracks.end()) return *actualTracks[iter->second];
		
	unsigned char cmd = GET_TRACK;
	send(serverSocket, (char*)&cmd, 1, 0);

	size_t clientIndex = actualTracks.size();
	send(serverSocket, (char*)&clientIndex, sizeof(size_t), 0);
	
	// send request data
	size_t name_len = name.size();
	printf("len: %d\n", name_len);
	send(serverSocket, (char*)&name_len, sizeof(size_t), 0);
	
	const char *name_str = name.c_str();
	send(serverSocket, name_str, name_len, 0);
	
	sync::Track *track = new sync::Track();
	/* todo: fill in based on the response */

	actualTracks.push_back(track);
	tracks[name] = clientIndex;
	return *track;
}

bool SyncDataClient::poll()
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
	return done;
}
