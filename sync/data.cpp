/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#include "data.h"
using sync::Data;

Data::~Data()
{
	for (size_t i = 0; i < tracks.size(); ++i) delete tracks[i];
}

size_t Data::createTrack(const std::basic_string<TCHAR> &name)
{
	assert(0 > getTrackIndex(name));
	
	// insert new track
	tracks.push_back(new sync::Track(name));
	
	assert(tracks.size() > 0);
	return tracks.size() - 1;
}

int Data::getTrackIndex(const std::basic_string<TCHAR> &name)
{
	// search for track
	for (size_t index = 0; index < tracks.size(); ++index)
	{
		if (name == tracks[index]->getName()) return int(index);
	}
	
	// not found
	return -1;
}
