// synctracker2.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>

#include "trackview.h"

const TCHAR *mainWindowClassName = _T("MainWindow");

TrackView *trackView;
HWND trackViewWin;

LRESULT CALLBACK mainWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
		case WM_CREATE:
			trackViewWin = trackView->create(GetModuleHandle(NULL), hwnd);
		break;
		
		case WM_SIZE:
		{
			int width  = LOWORD(lParam);
			int height = HIWORD(lParam);
			MoveWindow(trackViewWin, 0, 0, width, height, TRUE);
		}
		break;
		
		case WM_SETFOCUS:
			SetFocus(trackViewWin); // needed to forward keyboard input
		break;

		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

ATOM registerMainWindowClass(HINSTANCE hInstance)
{
	WNDCLASSEX wc;
	
	wc.cbSize        = sizeof(WNDCLASSEX);
	wc.style         = 0;
	wc.lpfnWndProc   = mainWindowProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)0;
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = mainWindowClassName;
	wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
	
	return RegisterClassEx(&wc);
}

int _tmain(int argc, _TCHAR* argv[])
{
	HWND hwnd;
	MSG Msg;
	HINSTANCE hInstance = GetModuleHandle(NULL);
	
	SyncData syncData;
	SyncTrack &testTrack = syncData.getTrack("test");
	SyncTrack &test2Track = syncData.getTrack("test2");
	for (int i = 0; i < 1 << 16; ++i)
	{
		char temp[256];
		sprintf(temp, "gen %02d", i);
		SyncTrack &temp2 = syncData.getTrack(temp);
	}

//	testTrack.setKeyFrame(0, SyncTrack::KeyFrame(1.0f));
	testTrack.setKeyFrame(1, SyncTrack::KeyFrame(2.0f));
	testTrack.setKeyFrame(4, SyncTrack::KeyFrame(3.0f));

	for (int i = 0; i < 5 * 2; ++i)
	{
		float time = float(i) / 2;
		printf("%f %d - %f\n", time, testTrack.isKeyFrame(i), testTrack.getValue(time));
	}
	
	ATOM mainClass      = registerMainWindowClass(hInstance);
	ATOM trackViewClass = registerTrackViewWindowClass(hInstance);
	if(!mainClass || ! trackViewClass)
	{
		MessageBox(NULL, _T("Window Registration Failed!"), _T("Error!"), MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}
	
	trackView = new TrackView();
	trackView->setSyncData(&syncData);
	
	// Step 2: Creating the Window
	hwnd = CreateWindowEx(
		0,
		mainWindowClassName,
		_T("SyncTracker 3000"),
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		CW_USEDEFAULT, CW_USEDEFAULT, // x, y
		CW_USEDEFAULT, CW_USEDEFAULT, // width, height
		NULL, NULL, hInstance, NULL
	);
	
	if (NULL == hwnd)
	{
		MessageBox(NULL, _T("Window Creation Failed!"), _T("Error!"), MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}
	
	ShowWindow(hwnd, TRUE);
	UpdateWindow(hwnd);
	
	// Step 3: The Message Loop
	while(GetMessage(&Msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}
	
	delete trackView;
	trackView = NULL;
	
	UnregisterClass(mainWindowClassName, hInstance);
	return int(Msg.wParam);
}