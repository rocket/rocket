// DotRocket.h

#pragma once

struct sync_device;

using namespace System;
using namespace System::Collections::Generic;

namespace DotRocket
{
	public ref class Track abstract
	{
	public:
		virtual double GetValue(double time) = 0;
	};

	public delegate void PauseEventHandler(bool flag);
	public delegate void SetRowEventHandler(int row);
	public delegate bool IsPlayingEventHandler();

	public ref class Device abstract
	{
	public:
		virtual bool Update(int row) { return true; }
		virtual bool Connect(System::String ^host, unsigned short port)
		{
			throw gcnew Exception("Connect() not valid on this type of DotRocket.Device");
		}

		virtual Track^ GetTrack(String ^name) = 0;
		event PauseEventHandler ^OnPause;
		event SetRowEventHandler ^OnSetRow;
		event IsPlayingEventHandler ^OnIsPlaying;
		void Pause(bool flag) { OnPause(flag); }
		void SetRow(int row) { OnSetRow(row); }
		bool IsPlaying() { return OnIsPlaying(); }
	};

	// Player-specific implementation
	public ref class PlayerDevice: public Device
	{
		sync_device *device;
		Dictionary<String ^, Track ^> ^tracks;
	public:
		PlayerDevice(System::String ^name);
		~PlayerDevice() { this->!PlayerDevice(); }
		!PlayerDevice();
		virtual Track^ GetTrack(String ^name) override;
	};
}
