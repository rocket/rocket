#pragma once

#include <string>
#include <map>
#include <vector>

#include <exception>
#include <cmath>
#include <cassert>
#include <tchar.h>

#include "sync/track.h"

class SyncData
{
public:
	size_t getTrackIndex(const std::basic_string<TCHAR> &name)
	{
		TrackContainer::iterator iter = tracks.find(name);
		if (iter != tracks.end()) return int(iter->second);
		
		size_t index = actualTracks.size();
		tracks[name] = index;
		actualTracks.push_back(new sync::Track);
		return index;
	}
	
	sync::Track &getTrack(const std::basic_string<TCHAR> &name)
	{
		size_t index = getTrackIndex(name);
		assert(index >= 0);
		assert(index < int(actualTracks.size()));
		assert(NULL != actualTracks[index]);
		return *actualTracks[index];
	}
	
	sync::Track &getTrack(size_t track)
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
	std::vector<sync::Track*> actualTracks;
};

