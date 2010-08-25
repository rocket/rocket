/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in COPYING
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
	vsnprintf(temp, sizeof(temp), fmt, va);
	va_end(va);

#ifdef _CONSOLE
	fprintf(stderr, "*** error: %s\n", temp);
#else
	MessageBox(NULL, temp, NULL, MB_OK | MB_ICONERROR);
#endif

	exit(EXIT_FAILURE);
}

const unsigned int width  = 800;
const unsigned int height = 600;

int main(int argc, char *argv[])
{
	IDirect3D9 *d3d;
	IDirect3DDevice9 *dev;
	HWND hwnd;
	HSTREAM stream;

	static D3DPRESENT_PARAMETERS present_parameters = {
		width, height, D3DFMT_X8R8G8B8, 3,
		D3DMULTISAMPLE_NONE, 0, D3DSWAPEFFECT_DISCARD,
		0, TRUE, 1, D3DFMT_D24S8, 0, 0
	};

	const struct sync_track *clear_r, *clear_g, *clear_b;
	const struct sync_track *cam_rot, *cam_dist;

	/* initialize directx */
	d3d = Direct3DCreate9(D3D_SDK_VERSION);
	if (!d3d)
		die("update directx, fool.");

	/* create a window */
	hwnd = CreateWindowEx(0, "static", "GNU Rocket Example",
	    WS_POPUP | WS_VISIBLE, 0, 0, width, height, 0, 0,
	    GetModuleHandle(0), 0);
	if (!hwnd)
		die("failed to create window");

	/* create the device */
	if (FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
	    hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &present_parameters,
	    &dev)))
		die("could not create device. you computer SUCKS!");

	/* init BASS */
	if (!BASS_Init(-1, 44100, 0, hwnd, 0))
		die("failed to init bass");
	stream = BASS_StreamCreateFile(false, "tune.ogg", 0, 0, 0);
	if (!stream)
		die("failed to open tune");

	sync_device *rocket = sync_create_device("sync");
	if (!rocket)
		die("out of memory?");

#ifndef SYNC_PLAYER
	sync_set_callbacks(rocket, &bass_cb, (void *)stream);
	if (sync_connect(rocket, "localhost", SYNC_DEFAULT_PORT))
		die("failed to connect to host");
#endif

	/* get tracks */
	clear_r = sync_get_track(rocket, "clear.r");
	clear_g = sync_get_track(rocket, "clear.g");
	clear_b = sync_get_track(rocket, "clear.b");
	cam_rot = sync_get_track(rocket, "cam.rot"),
	cam_dist = sync_get_track(rocket, "cam.dist");

	LPD3DXMESH cubeMesh = NULL;
	if (FAILED(D3DXCreateBox(dev, 1.0f, 1.0f, 1.0f,
		&cubeMesh, NULL)))
		return -1;

	/* let's roll! */
	BASS_Start();
	BASS_ChannelPlay(stream, false);

	bool done = false;
	while (!done) {
		double row = bass_get_row(stream);
#ifndef SYNC_PLAYER
		if (sync_update(rocket, (int)floor(row)))
			sync_connect(rocket, "localhost", SYNC_DEFAULT_PORT);
#endif

		/* draw */

		D3DXCOLOR clearColor(
			sync_get_val(clear_r, row),
			sync_get_val(clear_g, row),
			sync_get_val(clear_b, row),
			0.0
		);

		dev->BeginScene();
		dev->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, clearColor, 1.0f, 0);

		float rot = sync_get_val(cam_rot, row);
		float dist = sync_get_val(cam_dist, row);
		D3DXVECTOR3 eye(sin(rot) * dist, 0, cos(rot) * dist);
		D3DXVECTOR3 at(0, 0, 0);
		D3DXVECTOR3 up(0, 1, 0);
		D3DXMATRIX view;
		D3DXVECTOR3 dir = eye + at;
		D3DXMatrixLookAtLH(&view, &dir, &at, &up);
		dev->SetTransform(D3DTS_VIEW, &view);
		
		D3DXMATRIX proj;
		D3DXMatrixPerspectiveFovLH(&proj, D3DXToRadian(60), 4.0f / 3, 0.1f, 1000.f);
		dev->SetTransform(D3DTS_PROJECTION, &proj);

		cubeMesh->DrawSubset(0);

		dev->EndScene();
		dev->Present(0, 0, 0, 0);

		BASS_Update(0); /* decrease the chance of missing vsync */
		MSG msg;
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			if (WM_QUIT == msg.message ||
			    (WM_KEYDOWN == msg.message &&
			    VK_ESCAPE == LOWORD(msg.wParam)))
				done = true;
		}
	}

	BASS_StreamFree(stream);
	BASS_Free();

	dev->Release();
	d3d->Release();
	DestroyWindow(hwnd);

	return 0;
}
