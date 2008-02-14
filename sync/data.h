#pragma once

#include <string>
#include <map>
#include <vector>

#include <exception>
#include <cmath>
#include <cassert>
#include <tchar.h>

#include "track.h"

namespace sync
{
	class Data
	{
	public:
		size_t getTrackIndex(const std::basic_string<TCHAR> &name);
		Track &getTrack(const std::basic_string<TCHAR> &name);
		Track &getTrack(size_t track);
		size_t getTrackCount() const;
		
	// private:
		typedef std::map<const std::basic_string<TCHAR>, size_t> TrackContainer;
	//	typedef std::map<const std::basic_string<TCHAR>, SyncTrack> TrackContainer;
		TrackContainer tracks;
		std::vector<Track*> actualTracks;
	};
}
