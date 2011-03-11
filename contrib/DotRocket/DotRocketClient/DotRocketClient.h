// DotRocketClient.h

#pragma once

struct sync_device;
struct sync_track;

using namespace System;
using namespace System::Collections::Generic;

namespace DotRocket {
	public ref class ClientDevice: public Device {
	protected:
		sync_device *device;
		Dictionary<System::String ^, Track ^> ^tracks;
	public:
		ClientDevice(System::String ^name);
		~ClientDevice() { this->!ClientDevice(); }
		!ClientDevice();
		virtual Track^ GetTrack(String ^name) override;
		virtual bool Connect(System::String ^host, unsigned short port) override;
		virtual bool Update(int row) override;
	};
}
