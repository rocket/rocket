#include "stdafx.h"

#include "trackview.h"

static const TCHAR *trackViewWindowClassName = _T("TrackView");

static const int topMarginHeight = 20;
static const int leftMarginWidth = 60;

static const int fontHeight = 16;
static const int fontWidth = 6;

static const int trackWidth = fontWidth * 16;

static const int lines = 0x80;

TrackView::TrackView() : syncData(NULL)
{
	scrollPosX = 0;
	scrollPosY = 0;
	windowWidth  = -1;
	windowHeight = -1;
	
	editLine = 0;
	editTrack = 0;

	this->hwnd = NULL;

	editBrush = CreateSolidBrush(RGB(255, 255, 0));
}

TrackView::~TrackView()
{
	DeleteObject(editBrush);
}

int TrackView::getScreenY(int line)
{
	return topMarginHeight + (line  * fontHeight) - scrollPosY;
}

int TrackView::getScreenX(int track)
{
	return leftMarginWidth + (track * trackWidth) - scrollPosX;
}

void TrackView::onCreate()
{
	setupScrollBars();
}

void TrackView::onPaint()
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);
	
	SelectObject(hdc, GetStockObject(SYSTEM_FIXED_FONT));
//	SelectObject(hdc, GetStockObject(SYSTEM_FONT));
	paintTracks(hdc, ps.rcPaint);
	
	EndPaint(hwnd, &ps);
}

void TrackView::paintTopMargin(HDC hdc, RECT rcTracks)
{
	RECT fillRect;
	RECT topLeftMargin;
	topLeftMargin.top = 0;
	topLeftMargin.bottom = topMarginHeight;
	topLeftMargin.left = 0;
	topLeftMargin.right = leftMarginWidth;
	fillRect = topLeftMargin;
	DrawEdge(hdc, &fillRect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_BOTTOM);
	FillRect(hdc, &fillRect, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
	ExcludeClipRect(hdc, topLeftMargin.left, topLeftMargin.top, topLeftMargin.right, topLeftMargin.bottom);

	int firstTrack = min(max(scrollPosX / trackWidth, 0), getTrackCount() - 1);
	int lastTrack  = min(max(firstTrack + windowTracks + 1, 0), getTrackCount() - 1);

	for (int track = firstTrack; track <= lastTrack; ++track)
	{
		RECT topMargin;

		topMargin.top = 0;
		topMargin.bottom = topMarginHeight;

		topMargin.left = getScreenX(track);
		topMargin.right = topMargin.left + trackWidth;
		
		if (!RectVisible(hdc, &topMargin)) continue;

		RECT fillRect = topMargin;

		HBRUSH bgBrush = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
		if (track == editTrack) bgBrush = editBrush;

		DrawEdge(hdc, &fillRect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_LEFT | BF_RIGHT | BF_BOTTOM);
		FillRect(hdc, &fillRect, bgBrush);

		/* format the text */
		TCHAR temp[256];
		_sntprintf_s(temp, 256, _T("track %d"), track);

		std::string trackName(temp);
		TextOut(hdc,
			fillRect.left, 0,
			trackName.c_str(), int(trackName.length())
		);
		ExcludeClipRect(hdc, topMargin.left, topMargin.top, topMargin.right, topMargin.bottom);
	}

	RECT topRightMargin;
	topRightMargin.top = 0;
	topRightMargin.bottom = topMarginHeight;
	topRightMargin.left = getScreenX(getTrackCount());
	topRightMargin.right = rcTracks.right;
	fillRect = topRightMargin;
	DrawEdge(hdc, &fillRect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_BOTTOM);
	FillRect(hdc, &fillRect, (HBRUSH)GetStockObject(LTGRAY_BRUSH));
	ExcludeClipRect(hdc, topRightMargin.left, topRightMargin.top, topRightMargin.right, topRightMargin.bottom);

}

void TrackView::paintTracks(HDC hdc, RECT rcTracks)
{
	TCHAR temp[256];

	int firstTrack = min(max(scrollPosX / trackWidth, 0), getTrackCount() - 1);
	int lastTrack  = min(max(firstTrack + windowTracks + 1, 0), getTrackCount() - 1);
	
	int firstLine = editLine - windowLines / 2 - 1;
	int lastLine  = editLine + windowLines / 2 + 1;
	/* clamp first & last line */
	firstLine = min(max(firstLine, 0), lines - 1);
	lastLine = min(max(lastLine, 0), lines - 1);

	SetBkMode(hdc, TRANSPARENT);

	SelectObject(hdc, editBrush);
	

	paintTopMargin(hdc, rcTracks);

	
	for (int line = firstLine; line <= lastLine; ++line)
	{
		RECT leftMargin;
		leftMargin.left  = 0;
		leftMargin.right = leftMarginWidth;
		leftMargin.top    = getScreenY(line);
		leftMargin.bottom = leftMargin.top + fontHeight;

		if (!RectVisible(hdc, &leftMargin)) continue;

		if (line == editLine) FillRect(hdc, &leftMargin, editBrush);
		else                  FillRect(hdc, &leftMargin, (HBRUSH)GetStockObject(LTGRAY_BRUSH));

		DrawEdge(hdc, &leftMargin, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_RIGHT | BF_BOTTOM | BF_TOP);
//		Rectangle( hdc, leftMargin.left, leftMargin.top, leftMargin.right, leftMargin.bottom + 1);
		if ((line % 16) == 0)      SetTextColor(hdc, RGB(0, 0, 0));
		else if ((line % 8) == 0)  SetTextColor(hdc, RGB(32, 32, 32));
		else if ((line % 4) == 0)  SetTextColor(hdc, RGB(64, 64, 64));
		else                    SetTextColor(hdc, RGB(128, 128, 128));
		
		_sntprintf_s(temp, 256, _T("%0*Xh"), 5, line);
		TextOut(hdc,
			leftMargin.left, leftMargin.top,
			temp, int(_tcslen(temp))
		);

		ExcludeClipRect(hdc, leftMargin.left, leftMargin.top, leftMargin.right, leftMargin.bottom);
	}
	
	SetTextColor(hdc, RGB(0, 0, 0));
	
	for (int track = firstTrack; track <= lastTrack; ++track)
	{
		for (int line = firstLine; line <= lastLine; ++line)
		{
			RECT patternDataRect;
			patternDataRect.left = getScreenX(track);
			patternDataRect.right = patternDataRect.left + trackWidth;
			patternDataRect.top = getScreenY(line);
			patternDataRect.bottom = patternDataRect.top + fontHeight;

			if (!RectVisible(hdc, &patternDataRect)) continue;

			HBRUSH bgBrush = (HBRUSH)GetStockObject(WHITE_BRUSH);
			if (line % 8 == 0) bgBrush = (HBRUSH)GetStockObject(LTGRAY_BRUSH);
			if (line == editLine) bgBrush = editBrush;

			RECT fillRect = patternDataRect;
//			if (line == editLine && track == editTrack) DrawEdge(hdc, &fillRect, BDR_RAISEDINNER | BDR_SUNKENOUTER, BF_ADJUST | BF_TOP | BF_BOTTOM | BF_LEFT | BF_RIGHT);
			FillRect( hdc, &fillRect, bgBrush);
			if (line == editLine && track == editTrack)
			{
//				DrawFocusRect(hdc, &fillRect);
				Rectangle(hdc, fillRect.left, fillRect.top, fillRect.right, fillRect.bottom);
			}
			
			bool key = (line % 8 == 0);
			float val = (float(line) / 16);
			
			/* format the text */
			if (!key) _sntprintf_s(temp, 256, _T("---"));
			else _sntprintf_s(temp, 256, _T("%2.2f"), val);
			TextOut(hdc,
				patternDataRect.left, patternDataRect.top,
				temp, int(_tcslen(temp))
			);
		}
	}

	/* right margin */
	{
		RECT rightMargin;
		rightMargin.top    = getScreenY(0);
		rightMargin.bottom = getScreenY(lines);
		rightMargin.left  = getScreenX(getTrackCount());
		rightMargin.right = rcTracks.right;
		FillRect( hdc, &rightMargin, (HBRUSH)GetStockObject(GRAY_BRUSH));
	}

	{
		RECT bottomPadding;
		bottomPadding.top = getScreenY(lines);
		bottomPadding.bottom = rcTracks.bottom;
		bottomPadding.left = rcTracks.left;
		bottomPadding.right = rcTracks.right;
		FillRect(hdc, &bottomPadding, (HBRUSH)GetStockObject(GRAY_BRUSH));
	}
	
	{
		RECT topPadding;
		topPadding.top = max(rcTracks.top, topMarginHeight);
		topPadding.bottom = getScreenY(0);
		topPadding.left = rcTracks.left;
		topPadding.right = rcTracks.right;
		FillRect(hdc, &topPadding, (HBRUSH)GetStockObject(GRAY_BRUSH));
	}
}

void TrackView::setupScrollBars()
{
	SCROLLINFO si = { sizeof(si) };
	si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE | SIF_DISABLENOSCROLL;
	si.nPos  = editLine;
	si.nPage = windowLines;
	si.nMin  = 0;
	si.nMax  = lines - 1 + windowLines - 1;
	SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
	
	si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE | SIF_DISABLENOSCROLL;
	si.nPos  = editTrack;
	si.nPage = windowTracks;
	si.nMin  = 0;
	si.nMax  = getTrackCount() - 1 + windowTracks - 1;
	SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
}

void TrackView::scrollWindow(int scrollX, int scrollY)
{
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
}

void TrackView::setScrollPos(int newScrollPosX, int newScrollPosY)
{
	// clamp newscrollPosX
	newScrollPosX = max(newScrollPosX, 0);
	
	if (newScrollPosX != scrollPosX || newScrollPosY != scrollPosY)
	{
		int scrollX = scrollPosX - newScrollPosX;
		int scrollY = scrollPosY - newScrollPosY;
		
		// update scrollPos
		scrollPosX = newScrollPosX;
		scrollPosY = newScrollPosY;
		
		scrollWindow(scrollX, scrollY);
		setupScrollBars();
	}
}

void TrackView::setEditLine(int newEditLine)
{
	int oldEditLine = editLine;
	editLine = newEditLine;
	
	// clamp to document
	editLine = max(editLine, 0);
	editLine = min(editLine, lines - 1);

	RECT clientRect;
	GetClientRect(hwnd, &clientRect);

	RECT lineRect;
	lineRect.left  = clientRect.left;
	lineRect.right = clientRect.right;

	lineRect.top    = getScreenY(oldEditLine);
	lineRect.bottom = lineRect.top + fontHeight;
	InvalidateRect(hwnd, &lineRect, TRUE);

	lineRect.top    = getScreenY(editLine);
	lineRect.bottom = lineRect.top + fontHeight;
	InvalidateRect(hwnd, &lineRect, TRUE);

	setScrollPos(scrollPosX, (editLine * fontHeight) - ((windowHeight - topMarginHeight) / 2) + fontHeight / 2);
}

void TrackView::setEditTrack(int newEditTrack)
{
	int oldEditTrack = editTrack;
	editTrack = newEditTrack;
	
	// clamp to document
	editTrack = max(editTrack, 0);
	editTrack = min(editTrack, getTrackCount() - 1);

	RECT trackRect;

	/* dirty marker */
	trackRect.top    = getScreenY(editLine);
	trackRect.bottom = trackRect.top + fontHeight;

	trackRect.left  = getScreenX(oldEditTrack);
	trackRect.right = trackRect.left + trackWidth;
	InvalidateRect(hwnd, &trackRect, TRUE);

	trackRect.left  = getScreenX(editTrack);
	trackRect.right = trackRect.left + trackWidth;
	InvalidateRect(hwnd, &trackRect, TRUE);

	/* dirty track-marker */
	trackRect.top    = 0;
	trackRect.bottom = topMarginHeight;

	trackRect.left  = getScreenX(oldEditTrack);
	trackRect.right = trackRect.left + trackWidth;
	InvalidateRect(hwnd, &trackRect, TRUE);

	trackRect.left  = getScreenX(editTrack);
	trackRect.right = trackRect.left + trackWidth;
	InvalidateRect(hwnd, &trackRect, TRUE);

	int firstTrack = scrollPosX / trackWidth;
	int lastTrack  = firstTrack + windowTracks;

	int newFirstTrack = firstTrack;
	if (editTrack >= lastTrack) newFirstTrack = editTrack - (lastTrack - firstTrack - 1);
	if (editTrack < firstTrack) newFirstTrack = editTrack;
	setScrollPos(newFirstTrack * trackWidth, scrollPosY);
}

static int getScrollPos(HWND hwnd, int bar)
{
	SCROLLINFO si = { sizeof(si), SIF_TRACKPOS };
	GetScrollInfo(hwnd, bar, &si);
	return int(si.nTrackPos);
}

void TrackView::onVScroll(UINT sbCode, int newPos)
{
	switch (sbCode)
	{
	case SB_TOP:
		setEditLine(0);
	break;
	
	case SB_LINEUP:
		setEditLine(editLine - 1);
	break;
	
	case SB_LINEDOWN:
		setEditLine(editLine + 1);
	break;
	
	case SB_PAGEUP:
		setEditLine(editLine - windowLines / 2);
	break;
	
	case SB_PAGEDOWN:
		setEditLine(editLine + windowLines / 2);
	break;

	case SB_THUMBPOSITION:
	case SB_THUMBTRACK:
		setEditLine(getScrollPos(hwnd, SB_VERT));
	break;
	}
}

void TrackView::onHScroll(UINT sbCode, int newPos)
{
	switch (sbCode)
	{
	case SB_LEFT:
		setEditTrack(0);
	break;
	
	case SB_RIGHT:
		setEditTrack(getTrackCount() - 1);
	break;
	
	case SB_LINELEFT:
		setEditTrack(editTrack - 1);
	break;
	
	case SB_LINERIGHT:
		setEditTrack(editTrack + 1);
	break;
	
	case SB_PAGELEFT:
		setEditTrack(editTrack - windowTracks);
	break;
	
	case SB_PAGEDOWN:
		setEditTrack(editTrack + windowTracks);
	break;

	case SB_THUMBPOSITION:
	case SB_THUMBTRACK:
		setEditTrack(getScrollPos(hwnd, SB_HORZ));
	break;
	}
}

void TrackView::onKeyDown(UINT keyCode, UINT flags)
{
	switch (keyCode)
	{
		case VK_UP:   setEditLine(editLine - 1); break;
		case VK_DOWN: setEditLine(editLine + 1); break;

		case VK_LEFT:  setEditTrack(editTrack - 1); break;
		case VK_RIGHT: setEditTrack(editTrack + 1); break;

		case VK_PRIOR: setEditLine(editLine - windowLines / 2); break;
		case VK_NEXT:  setEditLine(editLine + windowLines / 2); break;
	}
}

void TrackView::onSize(int width, int height)
{
	const int oldWindowWidth = windowWidth;
	const int oldWindowHeight = windowHeight;

	windowWidth  = width;
	windowHeight = height;

	windowLines  = (height - topMarginHeight) / fontHeight;
	windowTracks = (width  - leftMarginWidth) / trackWidth;
	
	setEditLine(editLine);
	setupScrollBars();
}

LRESULT TrackView::windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	ASSERT(hwnd == this->hwnd);
	
	switch(msg)
	{
		case WM_CREATE:
			onCreate();
		break;
		
		case WM_CLOSE:
			DestroyWindow(hwnd);
		break;
		
		case WM_DESTROY:
			PostQuitMessage(0);
		break;
		
		case WM_SIZE:
			onSize(LOWORD(lParam), HIWORD(lParam));
		break;
		
		case WM_VSCROLL:
			onVScroll(LOWORD(wParam), getScrollPos(hwnd, SB_VERT));
		break;
		
		case WM_HSCROLL:
			onHScroll(LOWORD(wParam), getScrollPos(hwnd, SB_HORZ));
		break;
		
		case WM_PAINT:
			onPaint();
		break;
		
		case WM_KEYDOWN:
			onKeyDown((UINT)wParam, (UINT)lParam);
		break;
		
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return FALSE;
}

static LRESULT CALLBACK trackViewWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Get the TrackView instance (if any)
#pragma warning(suppress:4312) /* remove a pointless warning */
	TrackView *trackView = (TrackView*)GetWindowLongPtr(hwnd, 0);
	
	switch(msg)
	{
		case WM_NCCREATE:
			// Get TrackView from createstruct
			trackView = (TrackView*)((CREATESTRUCT*)lParam)->lpCreateParams;
			trackView->hwnd = hwnd;
			
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

			// get rid of the TrackView instance
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

HWND TrackView::create(HINSTANCE hInstance, HWND hwndParent)
{
	HWND hwnd = CreateWindowEx(
		WS_EX_CLIENTEDGE,
		trackViewWindowClassName, _T(""),
		WS_VSCROLL | WS_HSCROLL | WS_CHILD | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, // x, y
		CW_USEDEFAULT, CW_USEDEFAULT, // width, height
		hwndParent, NULL, GetModuleHandle(NULL), (void*)this
	);
	return hwnd;
}

