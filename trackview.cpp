#include "stdafx.h"

#include "trackview.h"

const TCHAR *trackViewWindowClassName = _T("TrackView");

static const int topMarginHeight = 20;
static const int leftMarginWidth = 60;

static const int fontHeight = 16;
static const int fontWidth = 12;

static const int lines = 0x80;
static const int tracks = 16;

int TrackView::getScreenY(int line)
{
	return topMarginHeight + (line - scrollPosY) * fontHeight;
}


void TrackView::onCreate(HWND hwnd)
{
	scrollPosX = 0;
	scrollPosY = 0;
	windowWidth  = -1;
	windowHeight = -1;
	setupScrollBars(hwnd);
}

void TrackView::onPaint(HWND hwnd)
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

void TrackView::paintTracks(HDC hdc, RECT rcTracks)
{
	TCHAR temp[256];
	
	int firstLine = scrollPosY + max((rcTracks.top - topMarginHeight) / fontHeight, 0);
	int lastLine  = scrollPosY + min(((rcTracks.bottom - topMarginHeight) + (fontHeight - 1)) / fontHeight, lines - 1);

	int editLine = firstLine + (lastLine - firstLine) / 2;
	HBRUSH editBrush = CreateSolidBrush(RGB(255, 255, 0));

	lastLine = min(lastLine, lines);

	RECT topLeftCorner;
	topLeftCorner.top = 0;
	topLeftCorner.bottom = topMarginHeight;
	topLeftCorner.left = 0;
	topLeftCorner.right = leftMarginWidth;
	FillRect(hdc, &topLeftCorner, (HBRUSH)GetStockObject(GRAY_BRUSH));
	ExcludeClipRect(hdc, topLeftCorner.left, topLeftCorner.top, topLeftCorner.right, topLeftCorner.bottom);

	SetBkMode(hdc, TRANSPARENT);

	for (int line = firstLine; line < lastLine; ++line)
	{
		RECT leftMargin;
		leftMargin.left  = 0;
		leftMargin.right = leftMarginWidth;
		leftMargin.top    = getScreenY(line);
		leftMargin.bottom = leftMargin.top + fontHeight;

//		if (y == editLine) FillRect(hdc, &leftMargin, editBrush);
//		else               
		FillRect(hdc, &leftMargin, (HBRUSH)GetStockObject(LTGRAY_BRUSH));

		DrawEdge(hdc, &leftMargin, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_RIGHT | BF_BOTTOM | BF_TOP);
//		Rectangle( hdc, leftMargin.left, leftMargin.top, leftMargin.right, leftMargin.bottom + 1);
		if ((line % 16) == 0)      SetTextColor(hdc, RGB(0, 0, 0));
		else if ((line % 8) == 0)  SetTextColor(hdc, RGB(32, 32, 32));
		else if ((line % 4) == 0)  SetTextColor(hdc, RGB(64, 64, 64));
		else                    SetTextColor(hdc, RGB(128, 128, 128));
		
		_sntprintf_s(temp, 256, _T("%04Xh"), line);
		TextOut(hdc,
			leftMargin.left, leftMargin.top,
			temp, int(_tcslen(temp))
		);

		ExcludeClipRect(hdc, leftMargin.left, leftMargin.top, leftMargin.right, leftMargin.bottom);
	}
	
	SetTextColor(hdc, RGB(0, 0, 0));
	
	int trackLeft = leftMarginWidth - scrollPosX;
	for (int x = 0; x < tracks; ++x)
	{
		int trackWidth = fontWidth * 5;
		
		RECT topMargin;

		topMargin.top = 0;
		topMargin.bottom = topMarginHeight;

		topMargin.left = trackLeft;
		topMargin.right = trackLeft + trackWidth;
		
		RECT fillRect = topMargin;
		DrawEdge(hdc, &fillRect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_LEFT | BF_RIGHT | BF_BOTTOM);
		FillRect(hdc, &fillRect, (HBRUSH)GetStockObject(LTGRAY_BRUSH));

		/* format the text */
		_sntprintf_s(temp, 256, _T("track %d"), x);
		TextOut(hdc,
			fillRect.left, 0,
			temp, int(_tcslen(temp))
		);
		ExcludeClipRect(hdc, topMargin.left, topMargin.top, topMargin.right, topMargin.bottom);

//		SetBkColor(hdc, RGB(255, 0, 0));
		for (int line = firstLine; line < lastLine; ++line)
		{
			RECT patternDataRect;
			patternDataRect.left = trackLeft;
			patternDataRect.right = trackLeft + trackWidth;
			patternDataRect.top = getScreenY(line);
			patternDataRect.bottom = patternDataRect.top + fontHeight;
			
			if (line % 8 == 0) FillRect( hdc, &patternDataRect, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
			else FillRect( hdc, &patternDataRect, (HBRUSH)GetStockObject(WHITE_BRUSH));
			
//			if (y % 8 == 0) DrawEdge(hdc, &patternDataRect, BDR_RAISEDINNER | BDR_SUNKENOUTER, BF_TOP | BF_BOTTOM | BF_FLAT);
			
			bool key = (line % 8 == 0);
			float val = (float(line) / 16);
			
			/* format the text */
			if (!key) _sntprintf_s(temp, 256, _T(" - - -"));
			else _sntprintf_s(temp, 256, _T("%2.2f"), val);
			TextOut(hdc,
				trackLeft, patternDataRect.top,
				temp, int(_tcslen(temp))
			);
		}

		RECT bottomMargin;
		bottomMargin.top = getScreenY(lines);
		bottomMargin.bottom = rcTracks.bottom;
		bottomMargin.left = trackLeft;
		bottomMargin.right = trackLeft + trackWidth;
		DrawEdge(hdc, &bottomMargin, BDR_SUNKENINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_TOP);
		FillRect(hdc, &bottomMargin, (HBRUSH)GetStockObject(WHITE_BRUSH));

		trackLeft += trackWidth;
	}

	/* pad top margin to the left edge */
	RECT topMargin;
	topMargin.top = 0;
	topMargin.bottom = topMarginHeight;
	topMargin.left = trackLeft;
	topMargin.right = rcTracks.right;
	DrawEdge(hdc, &topMargin, BDR_SUNKENINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_LEFT);
	FillRect(hdc, &topMargin, (HBRUSH)GetStockObject(LTGRAY_BRUSH));

	/* pad left margin to the left edge */
	RECT leftMargin;
	leftMargin.top = getScreenY(lines);
	leftMargin.bottom = rcTracks.bottom;
	leftMargin.left = 0;
	leftMargin.right = leftMarginWidth;
	DrawEdge(hdc, &leftMargin, BDR_SUNKENINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_RIGHT);
	FillRect(hdc, &leftMargin, (HBRUSH)GetStockObject(GRAY_BRUSH));

	/* right margin */
	RECT rightMargin;
	rightMargin.top    = topMarginHeight;
	rightMargin.bottom = rcTracks.bottom;
	rightMargin.left  = trackLeft;
	rightMargin.right = rcTracks.right;
	DrawEdge(hdc, &rightMargin, BDR_SUNKENINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_LEFT);
	FillRect( hdc, &rightMargin, (HBRUSH)GetStockObject(LTGRAY_BRUSH));

	DeleteObject(editBrush);
}

void TrackView::setupScrollBars(HWND hwnd)
{
	SCROLLINFO si = { sizeof(si) };
	si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE | SIF_DISABLENOSCROLL;
	si.nPos  = scrollPosY;
	si.nPage = windowLines;
	si.nMin  = 0;
	si.nMax  = lines - 1;
	SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
	
	si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE | SIF_DISABLENOSCROLL;
	si.nPos  = scrollPosX;
	si.nPage = windowWidth;
	si.nMin  = 0;
	si.nMax  = windowWidth * 2; // 0x80;
	SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
}

void TrackView::setScrollPos(HWND hwnd, int newScrollPosX, int newScrollPosY)
{
	const int oldScrollPosX = scrollPosX;
	const int oldScrollPosY = scrollPosY;

	// update scrollPos
	scrollPosX = newScrollPosX;
	scrollPosY = newScrollPosY;

	// clamp scrollPos
	scrollPosX = max(scrollPosX, 0);
	scrollPosY = max(scrollPosY, 0);

	scrollPosY = min(scrollPosY, (lines) - windowLines);

	if (oldScrollPosX != scrollPosX || oldScrollPosY != scrollPosY)
	{
		int scrollX = oldScrollPosX - scrollPosX;
		int scrollY = (oldScrollPosY - scrollPosY) * fontHeight;
		
		RECT clip;
		GetClientRect(hwnd, &clip);
		
		if (scrollX == 0) clip.top  = topMarginHeight; /* don't scroll the top line */
		if (scrollY == 0) clip.left = leftMarginWidth; /* don't scroll the top line */
		
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

static int getScrollPos(HWND hwnd, int bar)
{
	SCROLLINFO si = { sizeof(si), SIF_TRACKPOS };
	GetScrollInfo(hwnd, bar, &si);
	return int(si.nTrackPos);
}

void TrackView::onVScroll(HWND hwnd, UINT sbCode, int newPos)
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

void TrackView::onHScroll(HWND hwnd, UINT sbCode, int newPos)
{
	switch (sbCode)
	{
	case SB_LEFT:
		setScrollPos(hwnd, 0, scrollPosY);
	break;
	// SB_RIGHT currently missing.
	
	case SB_LINELEFT:
		setScrollPos(hwnd, scrollPosX - fontWidth, scrollPosY);
	break;
	
	case SB_LINERIGHT:
		setScrollPos(hwnd, scrollPosX + fontWidth, scrollPosY);
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

void TrackView::onKeyDown(HWND hwnd, UINT keyCode, UINT flags)
{
	switch (keyCode)
	{
		case VK_UP:   setScrollPos(hwnd, scrollPosX, scrollPosY - 1); break;
		case VK_DOWN: setScrollPos(hwnd, scrollPosX, scrollPosY + 1); break;

		case VK_PRIOR: setScrollPos(hwnd, scrollPosX, scrollPosY - windowLines); break;
		case VK_NEXT:  setScrollPos(hwnd, scrollPosX, scrollPosY + windowLines); break;
	}
}

void TrackView::onSize(HWND hwnd, int width, int height)
{
	const int oldWindowWidth = windowWidth;
	const int oldWindowHeight = windowHeight;

	windowWidth  = width;
	windowHeight = height;

	windowLines   = (height - topMarginHeight) / fontHeight;
//	windowLines   = min((height - topMarginHeight) / fontHeight, lines);
	setupScrollBars(hwnd);
}

LRESULT TrackView::windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
		case WM_CREATE:
			onCreate(hwnd);
		break;
		
		case WM_CLOSE:
			DestroyWindow(hwnd);
		break;
		
		case WM_DESTROY:
			PostQuitMessage(0);
		break;
		
		case WM_SIZE:
			onSize(hwnd, LOWORD(lParam), HIWORD(lParam));
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
		
		case WM_KEYDOWN:
			onKeyDown(hwnd, (UINT)wParam, (UINT)lParam);
		break;
		
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return FALSE;
}

LRESULT CALLBACK trackViewWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Get the TrackView instance (if any)
#pragma warning(suppress:4312) /* remove a pointless warning */
	TrackView *trackView = (TrackView*)GetWindowLongPtr(hwnd, 0);
	
	switch(msg)
	{
		case WM_NCCREATE:
			ASSERT(NULL == trackView);
			
			// allocate a TrackView instance
			trackView = new TrackView();
			
			// Set the TrackView instance
#pragma warning(suppress:4244) /* remove a pointless warning */
			SetWindowLongPtr(hwnd, 0, (LONG_PTR)trackView);

			// call the proper window procedure
			return trackView->windowProc(hwnd, msg, wParam, lParam);
		break;
		
		case WM_NCDESTROY:
		{
			ASSERT(NULL != trackView);
			
			// call the window proc and store away the return code
			LRESULT res = trackView->windowProc(hwnd, msg, wParam, lParam);

			// free the TrackView instance
			delete trackView;
			trackView = NULL;
			SetWindowLongPtr(hwnd, 0, (LONG_PTR)NULL);

			// return the stored return code
			return res;
		}
		break;
		
		default:
			ASSERT(NULL != trackView);
			return trackView->windowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

ATOM registerTrackViewWindowClass(HINSTANCE hInstance)
{
	WNDCLASSEX wc;
	
	wc.cbSize        = sizeof(WNDCLASSEX);
	wc.style         = 0;
	wc.lpfnWndProc   = trackViewWindowProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = sizeof(TrackView*);
	wc.hInstance     = hInstance;
	wc.hIcon         = 0;
	wc.hCursor       = LoadCursor(NULL, IDC_IBEAM);
	wc.hbrBackground = (HBRUSH)0;
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = trackViewWindowClassName;
	wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
	
	return RegisterClassEx(&wc);
}

HWND createTrackViewWindow(HINSTANCE hInstance, HWND hwndParent)
{
	HWND hwnd = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		trackViewWindowClassName, _T(""),
		WS_VSCROLL | WS_HSCROLL | WS_CHILD | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, // x, y
		CW_USEDEFAULT, CW_USEDEFAULT, // width, height
		hwndParent, NULL, GetModuleHandle(NULL), NULL
	);
	return hwnd;
}

