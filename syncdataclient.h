#include "network.h"
#include "syncdata.h"

class SyncDataClient : public SyncData
{
public:
	SyncDataClient(SOCKET serverSocket) : serverSocket(serverSocket) {}
	
	SyncTrack &getTrack(const std::basic_string<TCHAR> &name);	
	bool poll();	
private:
	std::map<int, SyncTrack*> serverRemap;
	SOCKET serverSocket;
};
