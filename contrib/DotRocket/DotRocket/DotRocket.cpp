// This is the main DLL file.

#include "stdafx.h"

#include "DotRocket.h"

#include "../../../lib/sync.h"
#include "../../../lib/track.h"

using System::Runtime::InteropServices::Marshal;
using DotRocket::Track;
using DotRocket::PlayerDevice;

private ref class PlayerTrack: public Track
{
	const sync_track *track;
public:
	PlayerTrack(const sync_track *track): track(track) {}
	virtual double GetValue(double time) override
	{
		return sync_get_val(track, time);
	}
};

PlayerDevice::PlayerDevice(System::String ^name)
{
	char *cname = (char *)(void *)Marshal::StringToHGlobalAnsi(name);
	device = sync_create_device(cname);
	tracks = gcnew Dictionary<String ^, Track ^>();
}

Track ^PlayerDevice::GetTrack(String^ name)
{
	Track ^track;
	if (!tracks->TryGetValue(name, track)) {
		char *ctrackName = (char *)(void *)Marshal::StringToHGlobalAnsi(name);
		track = gcnew PlayerTrack(sync_get_track(device, ctrackName));
		Marshal::FreeHGlobal((System::IntPtr)ctrackName);
		tracks->Add(name, track);
	}
	return track;
}

PlayerDevice::!PlayerDevice()
{
	sync_destroy_device(device);
}
