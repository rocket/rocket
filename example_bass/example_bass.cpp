/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <memory>
#include <exception>
#include <cstdio>
#include "../sync/device.h"
#include "bass.h"

class BassTimer : public sync::Timer
{
public:
	BassTimer(HSTREAM stream, float bpm, int rowsPerBeat) : stream(stream)
	{
		rowRate = (double(bpm) / 60) * rowsPerBeat;
	}
	
	// BASS hooks
	void  pause()           { BASS_ChannelPause(stream); }
	void  play()            { BASS_ChannelPlay(stream, false); }
	float getTime()         { return float(BASS_ChannelBytes2Seconds(stream, BASS_ChannelGetPosition(stream, BASS_POS_BYTE))); }
	float getRow()          { return float(getTime() * rowRate); }
	void  setRow(float row) { BASS_ChannelSetPosition(stream, BASS_ChannelSeconds2Bytes(stream, float(row / rowRate)), BASS_POS_BYTE); }
	bool  isPlaying()       { return (BASS_ChannelIsActive(stream) == BASS_ACTIVE_PLAYING); }
private:
	HSTREAM stream;
	double rowRate;
};

#define WINDOWED 1
const unsigned int width  = 800;
const unsigned int height = 600;

int main(int argc, char *argv[])
{
	int ret = 0;
	try
	{
		// initialize directx 
		IDirect3D9 *d3d = Direct3DCreate9(D3D_SDK_VERSION);
		if (NULL == d3d) throw std::string("update directx, fool.");
		
		// create a window
		HWND hwnd = CreateWindowEx(0, "static", "GNU Rocket Example", WS_POPUP | WS_VISIBLE, 0, 0, width, height, 0, 0, GetModuleHandle(0), 0);
		
		// create the device
		IDirect3DDevice9 *device = NULL;
		static D3DPRESENT_PARAMETERS present_parameters = {width, height, D3DFMT_X8R8G8B8, 3, D3DMULTISAMPLE_NONE, 0, D3DSWAPEFFECT_DISCARD, 0, WINDOWED, 1, D3DFMT_D24S8, 0, WINDOWED ? 0 : D3DPRESENT_RATE_DEFAULT, 0};
		if (D3D_OK != d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING, &present_parameters, &device))
			throw std::string("could not create device. you computer SUCKS!");
		
		// init BASS
		int soundDevice = 1;
		if (!BASS_Init(soundDevice, 44100, 0, hwnd, 0)) throw std::string("failed to init bass");
		
		// load tune
		HSTREAM stream = BASS_StreamCreateFile(false, "tune.ogg", 0, 0, BASS_MP3_SETPOS | ((0 == soundDevice) ? BASS_STREAM_DECODE : 0));
		if (!stream) throw std::string("failed to open tune");
		
		// let's just assume 150 BPM (this holds true for the included tune)
		float bpm = 150.0f;
		
		// setup timer and construct sync-device
		BassTimer timer(stream, bpm, 8);
		std::auto_ptr<sync::Device> syncDevice = std::auto_ptr<sync::Device>(sync::createDevice("sync", timer));
		if (NULL == syncDevice.get()) throw std::string("something went wrong - failed to connect to host?");
		
		// get tracks
		sync::Track &clearRTrack = syncDevice->getTrack("clear.r");
		sync::Track &clearGTrack = syncDevice->getTrack("clear.g");
		sync::Track &clearBTrack = syncDevice->getTrack("clear.b");

		sync::Track &camRotTrack  = syncDevice->getTrack("cam.rot");
		sync::Track &camDistTrack = syncDevice->getTrack("cam.dist");

		LPD3DXMESH cubeMesh = NULL;
		if (FAILED(D3DXCreateBox(
			device,
			1.0f, 1.0f, 1.0f,
			&cubeMesh, NULL
			)))
			return -1;
		
		// let's roll!
		BASS_Start();
		timer.play();
		
		bool done = false;
		while (!done)
		{
			float row = float(timer.getRow());
			if (!syncDevice->update(row)) done = true;
			
			// setup clear color
			D3DXCOLOR clearColor(
				clearRTrack.getValue(row),
				clearGTrack.getValue(row),
				clearBTrack.getValue(row),
				0.0
			);
			
			// paint the window
			device->BeginScene();
			device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, clearColor, 1.0f, 0);
			
/*			D3DXMATRIX world;
			device->SetTransform(D3DTS_WORLD, &world); */
			
			float rot = camRotTrack.getValue(row);
			float dist = camDistTrack.getValue(row);
			D3DXVECTOR3 eye(sin(rot) * dist, 0, cos(rot) * dist);
			D3DXVECTOR3 at(0, 0, 0);
			D3DXVECTOR3 up(0, 1, 0);
			D3DXMATRIX view;
			D3DXMatrixLookAtLH(&view, &(eye + at), &at, &up);
			device->SetTransform(D3DTS_WORLD, &view);
			
			D3DXMATRIX proj;
			D3DXMatrixPerspectiveFovLH(&proj, D3DXToRadian(60), 4.0f / 3, 0.1f, 1000.f);
			device->SetTransform(D3DTS_PROJECTION, &proj);
			
			cubeMesh->DrawSubset(0);
			
			device->EndScene();
			device->Present(0, 0, 0, 0);
			
			BASS_Update(0); // decrease the chance of missing vsync
			MSG msg;
			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				
				if (WM_QUIT == msg.message) done = true;
				if ((WM_KEYDOWN == msg.message) && (VK_ESCAPE == LOWORD(msg.wParam))) done = true;
			}
		}
		
		BASS_StreamFree(stream);
		BASS_Free();
		
		device->Release();
		d3d->Release();
		DestroyWindow(hwnd);
	}
	catch (const std::exception &e)
	{
#ifdef _CONSOLE
		fprintf(stderr, "*** error: %s\n", e.what());
#else
		MessageBox(NULL, e.what(), NULL, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
#endif
		ret = -1;
	}
	catch (const std::string &str)
	{
#ifdef _CONSOLE
		fprintf(stderr, "*** error: %s\n", str.c_str());
#else
		MessageBox(NULL, e.what(), NULL, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
#endif
		ret = -1;
	}
	
	return ret;
}
