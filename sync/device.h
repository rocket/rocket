#ifndef SYNC_H
#define SYNC_H

#include <string>
#include "track.h"

namespace sync
{
	class Timer
	{
	public:
		virtual ~Timer() {};
		
		virtual void  pause() = 0;
		virtual void  play() = 0;
		virtual float getRow() = 0;
		virtual void  setRow(float pos) = 0;
		virtual bool  isPlaying() = 0;
	};
	
	class Device
	{
	public:
		virtual ~Device() {}
		
		virtual Track &getTrack(const std::string &trackName) = 0;
		virtual bool update(float row) = 0;
	};
	
	Device *createDevice(const std::string &baseName, Timer &timer);
}

#endif /* SYNC_H */
