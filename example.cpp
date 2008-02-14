#include <stdio.h>
#include "sync/device.h"

#if 0

class BassTimer : public sync::Timer
{
public:
	BassTimer(HSTREAM stream, float bpm) : stream(stream)
	{
		rowRate = bpm / 60;
	}
	
	// BASS hooks
	void  pause()           { BASS_ChannelPause(stream); }
	void  play()            { BASS_ChannelPlay(stream, false); }
	float getTime()         { return BASS_ChannelBytes2Seconds(stream, BASS_ChannelGetPosition(stream)); }
	float getRow()          { return getTime() * rowRate; }
	void  setRow(float pos) { BASS_ChannelSetPosition(stream, BASS_ChannelSeconds2Bytes(stream, row / rowRate)); }
	bool  isPlaying()       { return (BASS_ChannelIsActive(stream) == BASS_ACTIVE_PLAYING); }
private:
	HSTREAM stream;
	float rowRate;
};

#else

#include <cmath>

class NullTimer : public sync::Timer
{
public:
	NullTimer() : paused(true), row(0) {}
	void  pause()           { paused = true; }
	void  play()            { paused = false; }
	
	float getRow()
	{ 
		if (!paused) return row++;
		else return row;
	}
	
	void  setRow(float row) { row = int(floor(row)); }
	bool  isPlaying()       { return !paused; }
	
private:
	bool paused;
	int row;
};

#endif

#include <windows.h>
#include <memory>
int main(int argc, char *argv[])
{
//	BassTimer timer(stream, 120.0f);
	NullTimer timer;
	std::auto_ptr<sync::Device> syncDevice = std::auto_ptr<sync::Device>(sync::createDevice("sync", timer));
	if (NULL == syncDevice.get())
	{
		printf("wft?!");
		return -1;
	}
	
	sync::Track &track = syncDevice->getTrack("test");
	
	timer.play();
	while (1)
	{
		float row = timer.getRow();
		if (!syncDevice->update(row)) break;
		
		printf("%2.2f: %2.2f                \n", row, track.getValue(row));
		Sleep(1000);
	}

	return 0;
}
