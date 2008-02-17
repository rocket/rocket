// synctracker2.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>

const _TCHAR g_szClassName[] = _T("myWindowClass");

const int fontHeight = 16;
const int fontWidth = 12;
int scrollPosX;
int scrollPosY;

int windowWidth;
int windowHeight;
int windowLines;

void setupScrollBars(HWND hwnd)
{
	SCROLLINFO si = { sizeof(si) };
	si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE | SIF_DISABLENOSCROLL;
	si.nPos  = scrollPosY;
	si.nPage = windowLines;
	si.nMin  = 0;
	si.nMax  = 0x80;
	SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}

void scroll(HWND hwnd, int dx, int dy)
{
	const int oldScrollPosX = scrollPosX;
	const int oldScrollPosY = scrollPosY;

	// update scrollPos
	scrollPosX += dx;
	scrollPosY += dy;

	// clamp scrollPos
	scrollPosX = max(scrollPosX, 0);
	scrollPosY = max(scrollPosY, 0);

	if (oldScrollPosX != scrollPosX || oldScrollPosY != scrollPosY)
	{
		ScrollWindowEx(
			hwnd,
			oldScrollPosX - scrollPosX,
			(oldScrollPosY - scrollPosY) * fontHeight,
			NULL,
			NULL,
			NULL,
			NULL,
			SW_INVALIDATE
		);
		setupScrollBars(hwnd);
	}
}

void paintWindow(HWND hwnd)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);
	
	SetBkMode(hdc, OPAQUE);
//	SetBkColor(hdc, RGB(255, 0, 0));
	
	int firstLine = scrollPosY + ps.rcPaint.top    / fontHeight;
	int lastLine  = scrollPosY + (ps.rcPaint.bottom + fontHeight - 1) / fontHeight;
	for (int x = 0; x < 10; ++x)
	{
		for (int y = firstLine; y < lastLine; ++y)
		{
			int val = y | (x << 8);
			
			/* format the text */
			char temp[256];
			_snprintf_s(temp, 256, "%04X ", val);
			
			TextOut(hdc, x * fontWidth * 5 - scrollPosX, (y - scrollPosY) * fontHeight, temp, int(strlen(temp)));
		}
	}
	EndPaint(hwnd, &ps);
}

void onCreate(HWND hwnd)
{
	scrollPosX = 0;
	scrollPosY = 0;
	windowWidth  = -1;
	windowHeight = -1;
	setupScrollBars(hwnd);
}

void onScroll(HWND hwnd, UINT sbCode, int newPos)
{
	switch (sbCode)
	{
	case SB_LINEUP:
		scroll(hwnd, 0, -1);
	break;
	
	case SB_LINEDOWN:
		scroll(hwnd, 0,  1);
	break;
	
	case SB_PAGEUP:
		scroll(hwnd, 0, -windowLines);
	break;
	
	case SB_PAGEDOWN:
		scroll(hwnd, 0, windowLines);
	break;
	}
}

void onSize(HWND hwnd, int width, int height)
{
	const int oldWindowWidth = windowWidth;
	const int oldWindowHeight = windowHeight;

	windowWidth  = width;
	windowHeight = height;

	windowLines   = height / fontHeight;
	setupScrollBars(hwnd);
/*
	if (oldWindowWidth < windowWidth)
	{
		RECT rightRect;
		rightRect.left  = oldWindowWidth;
		rightRect.right = windowWidth;
		InvalidateRect(hwnd, &rightRect, FALSE);
	}
*/
/*
	if (oldWindowHeight < windowHeight)
	{
		RECT bottomRect;
		bottomRect.left   = 0;
		bottomRect.right  = width;
		bottomRect.top    = oldWindowHeight;
		bottomRect.bottom = windowHeight;
		printf("extending downwards! %d %d\n", bottomRect.top, bottomRect.bottom);
		InvalidateRect(hwnd, &bottomRect, FALSE);
	}
*/
}

void onPaint(HWND hwnd)
{
	paintWindow(hwnd);
}

int getScrollPos(HWND hwnd, int bar)
{
	SCROLLINFO si = { sizeof(si), SIF_TRACKPOS };
	GetScrollInfo(hwnd, bar, &si);
	return int(si.nTrackPos);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
		case WM_CREATE:
			onCreate(hwnd);
		break;
		
		case WM_SIZE:
			onSize(hwnd, LOWORD(lParam), HIWORD(lParam));
		break;
		
		case WM_CLOSE:
			DestroyWindow(hwnd);
		break;
		
		case WM_DESTROY:
			PostQuitMessage(0);
		break;
		
		case WM_VSCROLL:
			onScroll(hwnd, LOWORD(wParam), getScrollPos(hwnd, SB_VERT));
		break;
		
		case WM_PAINT:
			onPaint(hwnd);
		break;
		
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	WNDCLASSEX wc;
	HWND hwnd;
	MSG Msg;
	HINSTANCE hInstance = GetModuleHandle(NULL);
	
	//Step 1: Registering the Window Class
	wc.cbSize        = sizeof(WNDCLASSEX);
	wc.style         = 0;
	wc.lpfnWndProc   = WndProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = g_szClassName;
	wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
	
	if(!RegisterClassEx(&wc))
	{
		MessageBox(NULL, _T("Window Registration Failed!"), _T("Error!"), MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}
	
	// Step 2: Creating the Window
	hwnd = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		g_szClassName,
		_T("The title of my window"),
		WS_VSCROLL | WS_HSCROLL | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 240, 120,
		NULL, NULL, hInstance, NULL
	);
	
	if(hwnd == NULL)
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
	
	return int(Msg.wParam);
}