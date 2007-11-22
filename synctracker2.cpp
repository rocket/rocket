// synctracker2.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>

const _TCHAR g_szClassName[] = _T("myWindowClass");

const int topMarginHeight = 20;

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
	
	si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE | SIF_DISABLENOSCROLL;
	si.nPos  = scrollPosX;
	si.nPage = windowWidth;
	si.nMin  = 0;
	si.nMax  = windowWidth * 2; // 0x80;
	SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
}

void setScrollPos(HWND hwnd, int newScrollPosX, int newScrollPosY)
{
	const int oldScrollPosX = scrollPosX;
	const int oldScrollPosY = scrollPosY;

	// update scrollPos
	scrollPosX = newScrollPosX;
	scrollPosY = newScrollPosY;

	// clamp scrollPos
	scrollPosX = max(scrollPosX, 0);
	scrollPosY = max(scrollPosY, 0);

	if (oldScrollPosX != scrollPosX || oldScrollPosY != scrollPosY)
	{
		int scrollX = oldScrollPosX - scrollPosX;
		int scrollY = (oldScrollPosY - scrollPosY) * fontHeight;
		
		RECT clip;
		GetClientRect(hwnd, &clip);
		
		if (scrollX == 0) clip.top = topMarginHeight; /* don't scroll the top line */
		
		ScrollWindowEx(
			hwnd,
			scrollX,
			scrollY,
			NULL,
			&clip,
			NULL,
			NULL,
			SW_INVALIDATE
		);
		setupScrollBars(hwnd);
	}
}

void paintTracks(HDC hdc, RECT rcTracks)
{
	char temp[256];
	
	int firstLine = max((rcTracks.top - topMarginHeight) / fontHeight, 0);
	int lastLine  = scrollPosY + ((rcTracks.bottom - topMarginHeight) + (fontHeight - 1)) / fontHeight;
	
	int trackLeft = -scrollPosX;
	for (int x = 0; x < 16; ++x)
	{
		int trackWidth = fontWidth * 5;
		
		RECT topMargin;

		topMargin.top = 0;
		topMargin.bottom = topMarginHeight;

		topMargin.left = trackLeft;
		topMargin.right = trackLeft + trackWidth;
		
		FillRect( hdc, &topMargin, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
		/* format the text */
		_snprintf_s(temp, 256, "track %d", x);
		SetBkMode(hdc, TRANSPARENT);
		TextOut(hdc,
			topMargin.left, 0,
			temp, int(strlen(temp))
		);
		ExcludeClipRect(hdc, topMargin.left, topMargin.top, topMargin.right, topMargin.bottom);

//		SetBkColor(hdc, RGB(255, 0, 0));

		for (int y = firstLine; y < lastLine; ++y)
		{
			int val = y | (x << 8);
			
			RECT patternDataRect;
			patternDataRect.left = trackLeft;
			patternDataRect.right = trackLeft + trackWidth;
			patternDataRect.top = topMarginHeight + (y - scrollPosY) * fontHeight;
			patternDataRect.bottom = patternDataRect.top + fontHeight;
			FillRect( hdc, &patternDataRect, (HBRUSH)GetStockObject(WHITE_BRUSH));
			
			/* format the text */
			if (y % 16 != 0) _snprintf_s(temp, 256, "- - -", val);
			else _snprintf_s(temp, 256, "%2.2f", (float(y) / 16));
			TextOut(hdc,
				trackLeft,
				topMarginHeight + (y - scrollPosY) * fontHeight,
				temp, int(strlen(temp))
			);
		}
		trackLeft += trackWidth;
	}

	/* pad top margin to the left edge */
	RECT topMargin;
	topMargin.top = 0;
	topMargin.bottom = topMarginHeight;
	topMargin.left = trackLeft;
	topMargin.right = rcTracks.right;
	FillRect( hdc, &topMargin, (HBRUSH)GetStockObject(LTGRAY_BRUSH));

	/* right margin */
	RECT rightMargin;
	topMargin.top = topMarginHeight;
	topMargin.bottom = rcTracks.bottom;
	topMargin.left = trackLeft;
	topMargin.right = rcTracks.right;
	FillRect( hdc, &topMargin, (HBRUSH)GetStockObject(LTGRAY_BRUSH));


}

void paintWindow(HWND hwnd)
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);
	
	SetBkMode(hdc, OPAQUE);
//	SetBkColor(hdc, RGB(255, 0, 0));

	/*
	RECT margin = rect;
	
	margin.left		= 0;
	margin.right	= LeftMarginWidth();
	rect.left	   += LeftMarginWidth();

	PaintMargin(hdc, nLineNo, &margin);
*/
	
	paintTracks(hdc, ps.rcPaint);
	
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

int getScrollPos(HWND hwnd, int bar)
{
	SCROLLINFO si = { sizeof(si), SIF_TRACKPOS };
	GetScrollInfo(hwnd, bar, &si);
	return int(si.nTrackPos);
}

void onVScroll(HWND hwnd, UINT sbCode, int newPos)
{
	switch (sbCode)
	{
	case SB_TOP:
		setScrollPos(hwnd, scrollPosX, 0);
	break;
	
	case SB_LINEUP:
		setScrollPos(hwnd, scrollPosX, scrollPosY - 1);
	break;
	
	case SB_LINEDOWN:
		setScrollPos(hwnd, scrollPosX, scrollPosY + 1);
	break;
	
	case SB_PAGEUP:
		setScrollPos(hwnd, scrollPosX, scrollPosY - windowLines);
	break;
	
	case SB_PAGEDOWN:
		setScrollPos(hwnd, scrollPosX, scrollPosY + windowLines);
	break;

	case SB_THUMBPOSITION:
	case SB_THUMBTRACK:
		setScrollPos(hwnd, scrollPosX, getScrollPos(hwnd, SB_VERT));
	break;
	}
}

void onHScroll(HWND hwnd, UINT sbCode, int newPos)
{
	switch (sbCode)
	{
	case SB_LEFT:
		setScrollPos(hwnd, 0, scrollPosY);
	break;
	// SB_RIGHT currently missing.
	
	case SB_LINELEFT:
		setScrollPos(hwnd, scrollPosX - 1, scrollPosY);
	break;
	
	case SB_LINERIGHT:
		setScrollPos(hwnd, scrollPosX + 1, scrollPosY);
	break;
	
	case SB_PAGELEFT:
		setScrollPos(hwnd, scrollPosX - 20, scrollPosY);
	break;
	
	case SB_PAGEDOWN:
		setScrollPos(hwnd, scrollPosX + 20, scrollPosY);
	break;

	case SB_THUMBPOSITION:
	case SB_THUMBTRACK:
		setScrollPos(hwnd, getScrollPos(hwnd, SB_HORZ), scrollPosY);
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
}

void onPaint(HWND hwnd)
{
	paintWindow(hwnd);
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
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
			onVScroll(hwnd, LOWORD(wParam), getScrollPos(hwnd, SB_VERT));
		break;
		
		case WM_HSCROLL:
			onHScroll(hwnd, LOWORD(wParam), getScrollPos(hwnd, SB_HORZ));
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
	wc.lpfnWndProc   = windowProc;
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
		_T("SyncTracker 3000"),
		WS_VSCROLL | WS_HSCROLL | WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, // x, y
		CW_USEDEFAULT, CW_USEDEFAULT, // width, height
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