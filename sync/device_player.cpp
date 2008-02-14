#include "device.h"
#include "../network.h"
#include "../syncdata.h"

using namespace sync;

class PlayerDevice : public Device
{
public:
	PlayerDevice(const std::string &baseName, Timer &timer) :
		baseName(baseName),
		timer(timer)
	{
	}
	
	~PlayerDevice();
	
	Track &getTrack(const std::string &trackName);
	bool update(float row);
	
private:
	const std::string &baseName;
	SyncData syncData;
	Timer &timer;
};

PlayerDevice::~PlayerDevice() { }

Track &PlayerDevice::getTrack(const std::string &trackName)
{
	SyncData::TrackContainer::iterator iter = syncData.tracks.find(trackName);
	if (iter != syncData.tracks.end()) return *syncData.actualTracks[iter->second];
		
	sync::Track *track = new sync::Track();
	
	// TODO: load data from file
	track->setKeyFrame(0,  10.0f);
	track->setKeyFrame(10,  0.0f);
	
	size_t index = syncData.actualTracks.size();
	syncData.actualTracks.push_back(track);
	syncData.tracks[trackName] = index;
	return *track;
}

bool PlayerDevice::update(float row)
{
	return true;
}

Device *sync::createDevice(const std::string &baseName, Timer &timer)
{
	Device *device = new PlayerDevice(baseName, timer);
	return device;
}
