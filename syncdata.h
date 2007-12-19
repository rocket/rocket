#pragma once

#include <string>
#include <map>

#include <exception>
#include <cmath>

class SyncTrack
{
public:
	struct KeyFrame
	{
		KeyFrame() : lerp(false) {}
		KeyFrame(float value) : value(value), lerp(false) {}
		float value;
		bool lerp;
	};
	
	float getValue(float time)
	{
		if (keyFrames.size() == 0) return 0.0f;
		
		int currRow = int(floor(time));
		
		// find bounding keyframes
		keyFrameContainer::const_iterator upper = keyFrames.upper_bound(currRow);
		keyFrameContainer::const_iterator lower = upper;
		lower--;
		
		// bounds check
		if (lower == keyFrames.end()) return upper->second.value;
		if (upper == keyFrames.end()) return lower->second.value;
		
		// lerp, bitch
		float d = (time - lower->first) / (upper->first - lower->first);
		return lower->second.value + (upper->second.value - lower->second.value) * d;
	};
	
	bool isKeyFrame(int row)
	{
		return keyFrames.find(row) != keyFrames.end();
	}
	
	KeyFrame &getKeyFrame(int row)
	{
		return keyFrames[row];
	}
	
	void setKeyFrame(int row, const KeyFrame &keyFrame)
	{
		keyFrames[row] = keyFrame;
	}
	
private:
	typedef std::map<int, struct KeyFrame> keyFrameContainer;
	keyFrameContainer keyFrames;
};

class SyncData
{
public:
	SyncTrack &getTrack(std::string name)
	{
		TrackContainer::iterator iter = tracks.find(name);
		if (iter != tracks.end()) return iter->second;
		return tracks[name] = SyncTrack();
	}
	
	size_t getTrackCount() { return tracks.size(); }
	
// private:
	typedef std::map<const std::string, SyncTrack> TrackContainer;
	TrackContainer tracks;
};
