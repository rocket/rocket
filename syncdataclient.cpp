#include "syncdataclient.h"
#include "network.h"

SyncTrack &SyncDataClient::getTrack(const std::basic_string<TCHAR> &name)
{
	TrackContainer::iterator iter = tracks.find(name);
	if (iter != tracks.end()) return iter->second;
		
	unsigned char cmd = GET_TRACK;
	send(serverSocket, (char*)&cmd, 1, 0);
	
	// send request data
	int name_len = name.size();
	printf("len: %d\n", name_len);
	send(serverSocket, (char*)&name_len, sizeof(int), 0);
	
	const char *name_str = name.c_str();
	send(serverSocket, name_str, name_len, 0);
	
	SyncTrack track = SyncTrack();
	/* todo: fill in based on the response */
	return tracks[name] = track;
}

bool SyncDataClient::poll()
{
	bool done = false;
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
