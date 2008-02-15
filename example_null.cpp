#define WIN32_LEAN_AND_MEAN
#include <windows.h> // needed for Sleep()

#include <stdio.h>
#include <memory>
#include <cmath>

#include "sync/device.h"
#include "sync/timer.h"

class NullTimer : public sync::Timer
{
public:
	NullTimer(float delta) : paused(true), row(0), delta(delta) {}
	
	void  pause()           { paused = true; }
	void  play()            { paused = false; }
	
	float getRow()
	{
		float ret = row;
		if (!paused) row += delta;
		return ret;
	}
	
	void  setRow(float row) { this->row = row; }
	bool  isPlaying()       { return !paused; }
	
private:
	bool paused;
	float row;
	float delta;
};

int main(int argc, char *argv[])
{
	NullTimer timer(0.1f);
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
		float row = float(timer.getRow());
		if (!syncDevice->update(row)) break;
		
		printf("%2.2f: %2.2f\n", row, track.getValue(row));
		Sleep(100);
	}

	return 0;
}
