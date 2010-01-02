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
#include <string>
#include <stdarg.h>

#include "../sync/sync.h"
#include "bass.h"

const float bpm = 150.0f; /* beats per minute */
const int rpb = 8; /* rows per beat */
const double row_rate = (double(bpm) / 60) * rpb;

double bass_get_row(HSTREAM h)
{
	QWORD pos = BASS_ChannelGetPosition(h, BASS_POS_BYTE);
	double time = BASS_ChannelBytes2Seconds(h, pos);
	return time * row_rate;
}

#ifndef SYNC_PLAYER

void bass_pause(void *d, int flag)
{
	if (flag)
		BASS_ChannelPause((HSTREAM)d);
	else
		BASS_ChannelPlay((HSTREAM)d, false);
}

void bass_set_row(void *d, int row)
{
	QWORD pos = BASS_ChannelSeconds2Bytes((HSTREAM)d, row / row_rate);
	BASS_ChannelSetPosition((HSTREAM)d, pos, BASS_POS_BYTE);
}

int bass_is_playing(void *d)
{
	return BASS_ChannelIsActive((HSTREAM)d) == BASS_ACTIVE_PLAYING;
}

struct sync_cb bass_cb = {
	bass_pause,
	bass_set_row,
	bass_is_playing
};

#endif /* !defined(SYNC_PLAYER) */

void die(const char *fmt, ...)
{
	char temp[4096];
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	vsnprintf(temp, sizeof(temp), fmt, va);
	va_end(va);

#ifdef _CONSOLE
	fprintf(stderr, "*** error: %s\n", temp);
#else
	MessageBox(NULL, temp, mainWindowTitle, MB_OK | MB_ICONERROR);
#endif

	exit(EXIT_FAILURE);
}

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
		if (D3D_OK != d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present_parameters, &device))
			throw std::string("could not create device. you computer SUCKS!");
		
		// init BASS
		int soundDevice = 1;
		if (!BASS_Init(soundDevice, 44100, 0, hwnd, 0)) throw std::string("failed to init bass");
		
		// load tune
		HSTREAM stream = BASS_StreamCreateFile(false, "tune.ogg", 0, 0, BASS_MP3_SETPOS | ((0 == soundDevice) ? BASS_STREAM_DECODE : 0));
		if (!stream) throw std::string("failed to open tune");

		sync_device *rocket = sync_create_device("sync");
		if (!rocket)
			die("something went wrong - failed to connect to host?");

#ifndef SYNC_PLAYER
		sync_set_callbacks(rocket, &bass_cb, (void *)stream);
#endif

		// get tracks
		struct sync_track
		    *clearRTrack = sync_get_track(rocket, "clear.r"),
		    *clearGTrack = sync_get_track(rocket, "clear.g"),
		    *clearBTrack = sync_get_track(rocket, "clear.b"),
		    *camRotTrack  = sync_get_track(rocket, "cam.rot"),
		    *camDistTrack = sync_get_track(rocket, "cam.dist");

		LPD3DXMESH cubeMesh = NULL;
		if (FAILED(D3DXCreateBox(
			device,
			1.0f, 1.0f, 1.0f,
			&cubeMesh, NULL
			)))
			return -1;
		
		// let's roll!
		BASS_Start();
		BASS_ChannelPlay(stream, false);

		bool done = false;
		while (!done)
		{
			double row = bass_get_row(stream);
			sync_update(rocket, row);

			// setup clear color
			D3DXCOLOR clearColor(
				sync_get_val(clearRTrack, row),
				sync_get_val(clearGTrack, row),
				sync_get_val(clearBTrack, row),
				0.0
			);

			// paint the window
			device->BeginScene();
			device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, clearColor, 1.0f, 0);

/*			D3DXMATRIX world;
			device->SetTransform(D3DTS_WORLD, &world); */

			float rot = sync_get_val(camRotTrack, row);
			float dist = sync_get_val(camDistTrack, row);
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
	} catch (const std::exception &e) {
		die(e.what());
	} catch (const std::string &str) {
		die(str.c_str());
	}

	return 0;
}
