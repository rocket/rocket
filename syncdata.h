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
		KeyFrameContainer::const_iterator upper = keyFrames.upper_bound(currRow);
		KeyFrameContainer::const_iterator lower = upper;
		lower--;
		
		// bounds check
		if (lower == keyFrames.end()) return upper->second.value;
		if (upper == keyFrames.end()) return lower->second.value;
		
		float delta = upper->second.value - lower->second.value;
		
		// lerp, bitch
		float d = (time - lower->first) / (upper->first - lower->first);
		return lower->second.value + delta * d;
	};
	
	bool isKeyFrame(int row) const
	{
		return keyFrames.find(row) != keyFrames.end();
	}
	
	const KeyFrame *getKeyFrame(int row) const
	{
		KeyFrameContainer::const_iterator iter = keyFrames.find(row);
		if (iter == keyFrames.end()) return NULL;
		return &iter->second;
	}
	
	void deleteKeyFrame(int row)
	{
		keyFrames.erase(row);
	}

	void setKeyFrame(int row, const KeyFrame &keyFrame)
	{
		keyFrames[row] = keyFrame;
	}
	
	void setKeyFrame(int row, const float value)
	{
		keyFrames[row] = KeyFrame(value);
	}

	
private:
	typedef std::map<int, struct KeyFrame> KeyFrameContainer;
	KeyFrameContainer keyFrames;
};

class SyncData
{
public:
	SyncTrack &getTrack(const std::basic_string<TCHAR> &name)
	{
		TrackContainer::iterator iter = tracks.find(name);
		if (iter != tracks.end()) return iter->second;
		return tracks[name] = SyncTrack();
	}
	
	SyncTrack &getTrack(size_t track)
	{
		assert(track >= 0);
		assert(track < tracks.size());
		
		SyncData::TrackContainer::iterator trackIter = tracks.begin();
		for (size_t currTrack = 0; currTrack < track; ++currTrack, ++trackIter);
		return trackIter->second;
	}
	
	size_t getTrackCount() { return tracks.size(); }
	
// private:
	typedef std::map<const std::basic_string<TCHAR>, SyncTrack> TrackContainer;
	TrackContainer tracks;
};
