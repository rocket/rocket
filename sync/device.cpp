/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#include "device.h"

using namespace sync;

std::string Device::getTrackFileName(std::string trackName)
{
	std::string fileName = baseName.c_str();
	fileName += "_";
	fileName += trackName;
	fileName += ".track";
	return fileName;
}
