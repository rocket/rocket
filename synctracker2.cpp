// synctracker2.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>

#include "trackview.h"

const char *windowClassName = "test";
TrackView trackView;

static int getScrollPos(HWND hwnd, int bar)
{
	SCROLLINFO si = { sizeof(si), SIF_TRACKPOS };
	GetScrollInfo(hwnd, bar, &si);
	return int(si.nTrackPos);
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
		case WM_CREATE:
			trackView.onCreate(hwnd);
		break;
		
		case WM_SIZE:
			trackView.onSize(hwnd, LOWORD(lParam), HIWORD(lParam));
		break;

		case WM_GETMINMAXINFO:
			trackView.onGetMinMaxInfo((MINMAXINFO*)lParam);
		break;
		
		case WM_CLOSE:
			DestroyWindow(hwnd);
		break;
		
		case WM_DESTROY:
			PostQuitMessage(0);
		break;
		
		case WM_VSCROLL:
			trackView.onVScroll(hwnd, LOWORD(wParam), getScrollPos(hwnd, SB_VERT));
		break;
		
		case WM_HSCROLL:
			trackView.onHScroll(hwnd, LOWORD(wParam), getScrollPos(hwnd, SB_HORZ));
		break;
		
		case WM_PAINT:
			trackView.onPaint(hwnd);
		break;
		
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

ATOM registerWindowClass(HINSTANCE hInstance)
{
	WNDCLASSEX wc;
	
	wc.cbSize        = sizeof(WNDCLASSEX);
	wc.style         = 0;
	wc.lpfnWndProc   = windowProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = windowClassName;
	wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
	
	return RegisterClassEx(&wc);
}

int _tmain(int argc, _TCHAR* argv[])
{
	HWND hwnd;
	MSG Msg;
	HINSTANCE hInstance = GetModuleHandle(NULL);
	
	ATOM wc = registerWindowClass(hInstance);
	if(!wc)
	{
		MessageBox(NULL, _T("Window Registration Failed!"), _T("Error!"), MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}
	
	// Step 2: Creating the Window
	hwnd = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		windowClassName,
		_T("SyncTracker 3000"),
		WS_VSCROLL | WS_HSCROLL | WS_OVERLAPPEDWINDOW,
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
	
	UnregisterClass(windowClassName, hInstance);
	return int(Msg.wParam);
}