#pragma once

#include <string>
#include <map>
#include <vector>

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
	}
	
	bool isKeyFrame(size_t row) const
	{
		return keyFrames.find(row) != keyFrames.end();
	}
	
	const KeyFrame *getKeyFrame(size_t row) const
	{
		KeyFrameContainer::const_iterator iter = keyFrames.find(row);
		if (iter == keyFrames.end()) return NULL;
		return &iter->second;
	}
	
	void deleteKeyFrame(size_t row)
	{
		keyFrames.erase(row);
	}
	
	void setKeyFrame(size_t row, const KeyFrame &keyFrame)
	{
		keyFrames[row] = keyFrame;
	}
	
	void setKeyFrame(size_t row, const float value)
	{
		setKeyFrame(row, KeyFrame(value));
	}
	
	size_t getFrameCount() const
	{
		if (keyFrames.empty()) return 0;
		KeyFrameContainer::const_iterator iter = keyFrames.end();
		iter--;
		return iter->first;
	}
	
private:
	
	typedef std::map<size_t, struct KeyFrame> KeyFrameContainer;
	KeyFrameContainer keyFrames;
};

class SyncData
{
public:
	SyncTrack &getTrack(const std::basic_string<TCHAR> &name)
	{
		TrackContainer::iterator iter = tracks.find(name);
		if (iter != tracks.end()) return *actualTracks[iter->second];
		
		tracks[name] = actualTracks.size();
		actualTracks.push_back(new SyncTrack());
		return *actualTracks.back();
	}
	
	SyncTrack &getTrack(size_t track)
	{
		assert(track >= 0);
		assert(track < tracks.size());
		
		SyncData::TrackContainer::iterator trackIter = tracks.begin();
		for (size_t currTrack = 0; currTrack < track; ++currTrack, ++trackIter);
		return *actualTracks[trackIter->second];
	}
	
	size_t getTrackCount() const { return tracks.size(); }
	
// private:
	typedef std::map<const std::basic_string<TCHAR>, size_t> TrackContainer;
//	typedef std::map<const std::basic_string<TCHAR>, SyncTrack> TrackContainer;
	TrackContainer tracks;
	std::vector<SyncTrack*> actualTracks;
};

