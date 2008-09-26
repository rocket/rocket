/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#include "device.h"
#include "data.h"
#include "network.h"

using namespace sync;

class PlayerDevice : public Device
{
public:
	PlayerDevice(const std::string &baseName, Timer &timer) :
		Device(baseName),
		timer(timer)
	{
	}
	
	~PlayerDevice();
	
	Track &getTrack(const std::string &trackName);
	bool update(float row);
	
private:
	sync::Data syncData;
	Timer &timer;
};

PlayerDevice::~PlayerDevice() { }

static bool loadTrack(sync::Track &track, std::string fileName)
{
	FILE *fp = fopen(fileName.c_str(), "rb");
	if (NULL == fp) return false;
	
	size_t keyFrameCount;
	fread(&keyFrameCount, sizeof(size_t), 1, fp);
	
	for (size_t i = 0; i < keyFrameCount; ++i)
	{
		size_t row;
		float value;
		char interp;
		fread(&row, sizeof(size_t), 1, fp);
		fread(&value, sizeof(float), 1, fp);
		fread(&interp, sizeof(char), 1, fp);
		
		track.setKeyFrame(row,
			Track::KeyFrame(
				value, 
				Track::KeyFrame::InterpolationType(interp)
			)
		);
	}
	
	fclose(fp);
	fp = NULL;
	return true;
}


Track &PlayerDevice::getTrack(const std::string &trackName)
{
	int trackIndex = syncData.getTrackIndex(trackName);
	if (0 <= trackIndex) return syncData.getTrack(trackIndex);
	
	trackIndex = int(syncData.createTrack(trackName));
	sync::Track &track = syncData.getTrack(trackIndex);
	loadTrack(track, getTrackFileName(trackName));
	return track;
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
