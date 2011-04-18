// This is the main DLL file.

#include "stdafx.h"

#include "../../../sync/sync.h"
#include "../../../sync/track.h"

using System::Runtime::InteropServices::Marshal;

#include "DotRocketClient.h"

using DotRocket::Track;
using DotRocket::Device;
using DotRocket::ClientDevice;

private ref class ClientTrack: public Track {
	const sync_track *track;
public:
	ClientTrack(const sync_track *track) : track(track) {}
	virtual float GetValue(double time) override
	{
		return sync_get_val(track, time);
	};
};

ref class Callbacks {
public:
	static Device ^DeviceCurrenltyBeingProcessed = nullptr;
};

void cb_pause(void *arg, int row)
{
	Callbacks::DeviceCurrenltyBeingProcessed->Pause(row);
}

void cb_set_row(void *arg, int row)
{
	Callbacks::DeviceCurrenltyBeingProcessed->SetRow(row);
}

int cb_is_playing(void *arg)
{
	return !!Callbacks::DeviceCurrenltyBeingProcessed->IsPlaying();
}

sync_cb callbacks[] = {
	cb_pause,
	cb_set_row,
	cb_is_playing
};

ClientDevice::ClientDevice(System::String^ name)
{
	char *cname = (char *)(void *)Marshal::StringToHGlobalAnsi(name);
	device = sync_create_device(cname);
	tracks = gcnew Dictionary<String^, Track^>();
}

Track ^ClientDevice::GetTrack(String^ name)
{
	Track ^track;
	if (!tracks->TryGetValue(name, track)) {
		char *ctrackName = (char *)(void *)Marshal::StringToHGlobalAnsi(name);
		track = gcnew ClientTrack(sync_get_track(device, ctrackName));
		Marshal::FreeHGlobal((System::IntPtr)ctrackName);
		tracks->Add(name, track);
	}
	return track;
}

ClientDevice::!ClientDevice()
{
	sync_destroy_device(device);
}

bool ClientDevice::Connect(System::String^ host, unsigned short port)
{
	char *chost = (char *)(void *)Marshal::StringToHGlobalAnsi(host);
	int result = sync_connect((sync_device *)device, chost, port);
	Marshal::FreeHGlobal((System::IntPtr)chost);
	return !result;
}

bool ClientDevice::Update(int row)
{
	Callbacks::DeviceCurrenltyBeingProcessed = this;
	return !sync_update(device, row, callbacks, device);
}
