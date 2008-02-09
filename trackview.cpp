#include "stdafx.h"

#include "trackview.h"

static const TCHAR *trackViewWindowClassName = _T("TrackView");

static const int topMarginHeight = 20;
static const int leftMarginWidth = 60;

static const int fontHeight = 16;
static const int fontWidth = 6;

static const int trackWidth = fontWidth * 16;
static DWORD darken(DWORD col, float amt)
{
	return RGB(GetRValue(col) * amt, GetGValue(col) * amt, GetBValue(col) * amt);
}

TrackView::TrackView()
{
	scrollPosX = 0;
	scrollPosY = 0;
	windowWidth  = -1;
	windowHeight = -1;
	
	editRow = 0;
	editTrack = 0;
	rows = 0x80;
	
	selectStartTrack = selectStopTrack = 0;
	selectStartRow = selectStopRow = 0;
	selectActive = false;
	
	this->hwnd = NULL;
	
	bgBaseBrush = GetSysColorBrush(COLOR_WINDOW); // CreateSolidBrush(RGB(0xff, 0xff, 0xff));
	bgDarkBrush = CreateSolidBrush(darken(GetSysColor(COLOR_WINDOW), 0.9f));

//	selectBaseBrush = CreateSolidBrush(RGB(0xff, 0xdd, 0xff));
//	selectDarkBrush = CreateSolidBrush(RGB(0xdd, 0xbb, 0xdd));

	selectBaseBrush = GetSysColorBrush(COLOR_HIGHLIGHT);
	selectDarkBrush = CreateSolidBrush(darken(GetSysColor(COLOR_HIGHLIGHT), 0.9f));
	
	editBrush = CreateSolidBrush(RGB(255, 255, 0)); // yellow
	
	clipboardFormat = RegisterClipboardFormat(_T("syncdata"));
	assert(0 != clipboardFormat);
}

TrackView::~TrackView()
{
	DeleteObject(bgBaseBrush);
	DeleteObject(bgDarkBrush);
	DeleteObject(selectBaseBrush);
	DeleteObject(selectDarkBrush);
	DeleteObject(editBrush);
}

int TrackView::getScreenY(int row)
{
	return topMarginHeight + (row  * fontHeight) - scrollPosY;
}

int TrackView::getScreenX(int track)
{
	return leftMarginWidth + (track * trackWidth) - scrollPosX;
}

LRESULT TrackView::onCreate()
{
	setupScrollBars();
	return FALSE;
}

LRESULT TrackView::onPaint()
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);
	
	SelectObject(hdc, GetStockObject(SYSTEM_FIXED_FONT));
//	SelectObject(hdc, GetStockObject(SYSTEM_FONT));
	paintTracks(hdc, ps.rcPaint);
	
	EndPaint(hwnd, &ps);
	
	return FALSE;
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
	FillRect(hdc, &fillRect, GetSysColorBrush(COLOR_3DFACE));
	
	int firstTrack = min(max(scrollPosX / trackWidth, 0), getTrackCount() - 1);
	int lastTrack  = min(max(firstTrack + windowTracks + 1, 0), getTrackCount() - 1);
	
	SyncData *syncData = getSyncData();
	if (NULL == syncData) return;

	SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
	
	SyncData::TrackContainer::iterator trackIter = syncData->tracks.begin();
	for (int track = 0; track <= lastTrack; ++track, ++trackIter)
	{
		ASSERT(trackIter != syncData->tracks.end());
		if (track < firstTrack) continue;
		
		RECT topMargin;
		
		topMargin.top = 0;
		topMargin.bottom = topMarginHeight;
		
		topMargin.left = getScreenX(track);
		topMargin.right = topMargin.left + trackWidth;
		
		if (!RectVisible(hdc, &topMargin)) continue;
		
		RECT fillRect = topMargin;
		
		HBRUSH bgBrush = GetSysColorBrush(COLOR_3DFACE);
		if (track == editTrack) bgBrush = editBrush;
		
		DrawEdge(hdc, &fillRect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_LEFT | BF_RIGHT | BF_BOTTOM);
		FillRect(hdc, &fillRect, bgBrush);
		
		const std::basic_string<TCHAR> &trackName = trackIter->first;
		TextOut(hdc,
			fillRect.left, 0,
			trackName.data(), int(trackName.length())
		);
	}
	
	RECT topRightMargin;
	topRightMargin.top = 0;
	topRightMargin.bottom = topMarginHeight;
	topRightMargin.left = getScreenX(getTrackCount());
	topRightMargin.right = rcTracks.right;
	fillRect = topRightMargin;
	DrawEdge(hdc, &fillRect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_BOTTOM);
	FillRect(hdc, &fillRect, GetSysColorBrush(COLOR_3DFACE));
	
	// make sure that the top margin isn't overdrawn by the track-data
	ExcludeClipRect(hdc, 0, 0, rcTracks.right, topMarginHeight);
}

void TrackView::paintTracks(HDC hdc, RECT rcTracks)
{
	TCHAR temp[256];
	
	int firstTrack = min(max(scrollPosX / trackWidth, 0), getTrackCount() - 1);
	int lastTrack  = min(max(firstTrack + windowTracks + 1, 0), getTrackCount() - 1);
	
	int firstRow = editRow - windowRows / 2 - 1;
	int lastRow  = editRow + windowRows / 2 + 1;
	/* clamp first & last row */
	firstRow = min(max(firstRow, 0), rows - 1);
	lastRow  = min(max(lastRow,  0), rows - 1);
	
	SetBkMode(hdc, TRANSPARENT);
	paintTopMargin(hdc, rcTracks);
	
	for (int row = firstRow; row <= lastRow; ++row)
	{
		RECT leftMargin;
		leftMargin.left  = 0;
		leftMargin.right = leftMarginWidth;
		leftMargin.top    = getScreenY(row);
		leftMargin.bottom = leftMargin.top + fontHeight;
		
		if (!RectVisible(hdc, &leftMargin)) continue;
		
		HBRUSH fillBrush;
		if (row == editRow) fillBrush = editBrush;
		else fillBrush = GetSysColorBrush(COLOR_3DFACE);
		FillRect(hdc, &leftMargin, fillBrush);
		
		DrawEdge(hdc, &leftMargin, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_RIGHT | BF_BOTTOM | BF_TOP);
		if ((row % 8) == 0)      SetTextColor(hdc, RGB(0, 0, 0));
		else if ((row % 4) == 0) SetTextColor(hdc, RGB(64, 64, 64));
		else                     SetTextColor(hdc, RGB(128, 128, 128));
		
/*		if ((row % 4) == 0) SetTextColor(hdc, GetSysColor(COLOR_BTNTEXT));
		else                SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT)); */
		
		_sntprintf_s(temp, 256, _T("%0*Xh"), 5, row);
		TextOut(hdc,
			leftMargin.left, leftMargin.top,
			temp, int(_tcslen(temp))
		);
	}
	
	SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
	
	SyncData *syncData = getSyncData();
	if (NULL == syncData) return;
	
	int selectLeft  = min(selectStartTrack, selectStopTrack);
	int selectRight = max(selectStartTrack, selectStopTrack);
	int selectTop    = min(selectStartRow, selectStopRow);
	int selectBottom = max(selectStartRow, selectStopRow);
	
	SyncData::TrackContainer::iterator trackIter = syncData->tracks.begin();
	for (int track = 0; track <= lastTrack; ++track, ++trackIter)
	{
		ASSERT(trackIter != syncData->tracks.end());
		if (track < firstTrack) continue;
		
		for (int row = firstRow; row <= lastRow; ++row)
		{
			RECT patternDataRect;
			patternDataRect.left = getScreenX(track);
			patternDataRect.right = patternDataRect.left + trackWidth;
			patternDataRect.top = getScreenY(row);
			patternDataRect.bottom = patternDataRect.top + fontHeight;
			
			if (!RectVisible(hdc, &patternDataRect)) continue;
			
			bool selected = selectActive && (track >= selectLeft && track <= selectRight) && (row >= selectTop && row <= selectBottom);
			
			HBRUSH baseBrush = bgBaseBrush;
			HBRUSH darkBrush = bgDarkBrush;
			if (selected)
			{
				baseBrush = selectBaseBrush;
				darkBrush = selectDarkBrush;
			}
			
			HBRUSH bgBrush = baseBrush;
			if (row % 8 == 0) bgBrush = darkBrush;
			
			RECT fillRect = patternDataRect;
//			if (row == editRow && track == editTrack) DrawEdge(hdc, &fillRect, BDR_RAISEDINNER | BDR_SUNKENOUTER, BF_ADJUST | BF_TOP | BF_BOTTOM | BF_LEFT | BF_RIGHT);
			FillRect( hdc, &fillRect, bgBrush);
/*			if (row % 8 == 0)
			{
				MoveToEx(hdc, patternDataRect.left, patternDataRect.top, (LPPOINT) NULL); 
				LineTo(hdc,   patternDataRect.right, patternDataRect.top); 
			} */
			
			bool drawEditString = false;
			if (row == editRow && track == editTrack)
			{
				FrameRect(hdc, &fillRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
//				DrawFocusRect(hdc, &fillRect);
//				Rectangle(hdc, fillRect.left, fillRect.top, fillRect.right, fillRect.bottom);
				if (editString.size() > 0) drawEditString = true;
			}
			
//			InvertRect(hdc, &fillRect);
			
			const SyncTrack &track = trackIter->second;
			bool key = track.isKeyFrame(row);
			
			/* format the text */
			if (drawEditString) _sntprintf_s(temp, 256, editString.c_str());
			else if (!key) _sntprintf_s(temp, 256, _T("---"));
			else
			{
				float val = track.getKeyFrame(row)->value;
				_sntprintf_s(temp, 256, _T("%.2f"), val);
			}
			
			COLORREF oldCol;
			if (selected) oldCol = SetTextColor(hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
			TextOut(hdc,
				patternDataRect.left, patternDataRect.top,
				temp, int(_tcslen(temp))
			);
			if (selected) SetTextColor(hdc, oldCol);
		}
	}
	
	/* right margin */
	{
		RECT rightMargin;
		rightMargin.top    = getScreenY(0);
		rightMargin.bottom = getScreenY(rows);
		rightMargin.left  = getScreenX(getTrackCount());
		rightMargin.right = rcTracks.right;
		FillRect( hdc, &rightMargin, GetSysColorBrush(COLOR_APPWORKSPACE));
	}
	
	{
		RECT bottomPadding;
		bottomPadding.top = getScreenY(rows);
		bottomPadding.bottom = rcTracks.bottom;
		bottomPadding.left = rcTracks.left;
		bottomPadding.right = rcTracks.right;
		FillRect(hdc, &bottomPadding, GetSysColorBrush(COLOR_APPWORKSPACE));
	}
	
	{
		RECT topPadding;
		topPadding.top = max(rcTracks.top, topMarginHeight);
		topPadding.bottom = getScreenY(0);
		topPadding.left = rcTracks.left;
		topPadding.right = rcTracks.right;
		FillRect(hdc, &topPadding, GetSysColorBrush(COLOR_APPWORKSPACE));
	}
}

void TrackView::copy()
{
	if (!selectActive) return;
	
	int selectLeft  = min(selectStartTrack, selectStopTrack);
	int selectRight = max(selectStartTrack, selectStopTrack);
	int selectTop    = min(selectStartRow, selectStopRow);
	int selectBottom = max(selectStartRow, selectStopRow);
	printf("copying:\n");
	
#if 0
	struct CopyEntry
	{
		int track, row;
		float val;
		bool valExisting;
	};
	std::vector<CopyEntry> copyBuffer;
#endif
	
	if (FAILED(OpenClipboard(getWin())))
	{
		MessageBox(NULL, _T("Failed to open clipboard"), NULL, MB_OK);
		return;
	}
	
	// gather data
	int rows = selectBottom - selectTop + 1;
	int columns = selectRight - selectLeft + 1;
	size_t cells = columns * rows;
	HGLOBAL hmem = GlobalAlloc(GMEM_MOVEABLE, sizeof(float) * cells);
	
	std::string copyString;
	for (int row = selectTop; row <= selectBottom; ++row)
	{
		for (int track = selectLeft; track <= selectRight; ++track)
		{
			const SyncTrack &t = syncData->getTrack(track);
			char temp[128];
			if (t.isKeyFrame(row)) sprintf(temp, "%.2f\t", t.getKeyFrame(row)->value);
			else sprintf(temp, "--- \t");
			copyString += temp;
			printf("(%d %d) = %s", track, row, temp);
		}
		puts("");
		copyString += "\n";
	}
	
	HGLOBAL hmem_text = GlobalAlloc(GMEM_MOVEABLE, strlen(copyString.c_str()) + 1);
	char *clipbuf = (char *)GlobalLock(hmem_text);
	memcpy(clipbuf, copyString.c_str(), strlen(copyString.c_str()) + 1);
	GlobalUnlock(hmem_text);
	
	// update clipboard
	EmptyClipboard();
	SetClipboardData(clipboardFormat, hmem);
	SetClipboardData(CF_TEXT, hmem_text);
	CloseClipboard();
	
	// should this memory be free'd or not? freeing seems to cause some hick-ups some times...
//	GlobalFree(hmem);
//	GlobalFree(hmem_text);
}

void TrackView::cut()
{
	int selectLeft  = min(selectStartTrack, selectStopTrack);
	int selectRight = max(selectStartTrack, selectStopTrack);
	int selectTop    = min(selectStartRow, selectStopRow);
	int selectBottom = max(selectStartRow, selectStopRow);
	
	copy();
#if 0
	for (int track = selectLeft; track <= selectRight; ++track)
	{
		SyncTrack &t = syncDataEdit.getSyncData()->getTrack(track);
		for (int row = selectTop; row <= selectBottom; ++row)
		{
			if (t.isKeyFrame(row)) t.deleteKeyFrame(row);
		}
	}
	invalidateRange(selectLeft, selectRight, selectTop, selectBottom);
#endif
}

void TrackView::paste()
{
	int selectLeft  = min(selectStartTrack, selectStopTrack);
	int selectRight = max(selectStartTrack, selectStopTrack);
	int selectTop    = min(selectStartRow, selectStopRow);
	int selectBottom = max(selectStartRow, selectStopRow);
}

void TrackView::setupScrollBars()
{
	SCROLLINFO si = { sizeof(si) };
	si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE | SIF_DISABLENOSCROLL;
	si.nPos  = editRow;
	si.nPage = windowRows;
	si.nMin  = 0;
	si.nMax  = rows - 1 + windowRows - 1;
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
	
	if (scrollX == 0) clip.top  = topMarginHeight; // don't scroll the top margin
	if (scrollY == 0) clip.left = leftMarginWidth; // don't scroll the left margin
	
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
	}
	setupScrollBars();
}

void TrackView::setEditRow(int newEditRow)
{
	int oldEditRow = editRow;
	editRow = newEditRow;
	
	// clamp to document
	editRow = min(max(editRow, 0), rows - 1);
	
	bool selecting  = GetKeyState(VK_SHIFT) < 0 ? true : false;
	if (selecting)
	{
		selectActive = true;
		selectStopRow = editRow;
		invalidateRange(selectStartTrack, selectStopTrack, oldEditRow, editRow);
	}
	else if (selectActive)
	{
		// leave select mode
		selectActive = false;
		invalidateRange(selectStartTrack, selectStopTrack, selectStartRow, selectStopRow);
	}
	
	invalidateRow(oldEditRow);
	invalidateRow(editRow);
	
	setScrollPos(scrollPosX, (editRow * fontHeight) - ((windowHeight - topMarginHeight) / 2) + fontHeight / 2);
}

void TrackView::setEditTrack(int newEditTrack)
{
	int oldEditTrack = editTrack;
	editTrack = newEditTrack;
	
	// clamp to document
	editTrack = max(editTrack, 0);
	editTrack = min(editTrack, getTrackCount() - 1);
	
	bool selecting  = GetKeyState(VK_SHIFT) < 0 ? true : false;
	if (selecting)
	{
		selectActive = true;
		selectStopTrack = editTrack;
		invalidateRange(oldEditTrack, editTrack, selectStartRow, selectStopRow);
	}
	else if (selectActive)
	{
		// leave select mode
		selectActive = false;
		invalidateRange(selectStartTrack, selectStopTrack, selectStartRow, selectStopRow);
	}
	
	invalidateTrack(oldEditTrack);
	invalidateTrack(editTrack);
	
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

void TrackView::setRows(int rows)
{
	int oldRows = getRows();
	this->rows = rows;
	InvalidateRect(getWin(), NULL, FALSE);
	setEditRow(min(editRow, rows - 1));
}


LRESULT TrackView::onVScroll(UINT sbCode, int newPos)
{
	switch (sbCode)
	{
	case SB_TOP:
		setEditRow(0);
		break;
	
	case SB_LINEUP:
		setEditRow(editRow - 1);
		break;
	
	case SB_LINEDOWN:
		setEditRow(editRow + 1);
		break;
	
	case SB_PAGEUP:
		setEditRow(editRow - windowRows / 2);
		break;
	
	case SB_PAGEDOWN:
		setEditRow(editRow + windowRows / 2);
		break;
	
	case SB_THUMBPOSITION:
	case SB_THUMBTRACK:
		setEditRow(getScrollPos(hwnd, SB_VERT));
		break;
	}
	
	return FALSE;
}

LRESULT TrackView::onHScroll(UINT sbCode, int newPos)
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
	
	return FALSE;
}

void TrackView::onReturn()
{
	if (editString.size() > 0)
	{
		syncData->setKey(editTrack, editRow, float(_tstof(editString.c_str())));
		
		editString.clear();
		invalidatePos(editTrack, editRow);
	}
	else MessageBeep(0);
}

void TrackView::onDelete()
{
	SyncTrack &track = syncData->getTrack(editTrack);
	if (track.isKeyFrame(editRow))
	{
		syncData->deleteKey(editTrack, editRow);
		invalidatePos(editTrack, editRow);
	}
	else MessageBeep(0);
}

LRESULT TrackView::onKeyDown(UINT keyCode, UINT flags)
{
	bool refreshCaret = false;
	bool ctrlDown   = GetKeyState(VK_CONTROL) < 0 ? true : false;
	bool shiftDown  = GetKeyState(VK_SHIFT) < 0 ? true : false;
	bool altDown    = GetKeyState(VK_MENU) < 0 ? true : false;
	
	if (editString.empty())
	{
		switch (keyCode)
		{
		case VK_UP:   setEditRow(editRow - 1); break;
		case VK_DOWN: setEditRow(editRow + 1); break;
		
		case VK_LEFT:  setEditTrack(editTrack - 1); break;
		case VK_RIGHT: setEditTrack(editTrack + 1); break;
		
		case VK_PRIOR: setEditRow(editRow - windowRows / 2); break;
		case VK_NEXT:  setEditRow(editRow + windowRows / 2); break;
		
		case VK_SHIFT:
//			if (selectActive) invalidateRange(selectStartTrack, selectStopTrack, selectStartRow, selectStopRow);
			if (!selectActive)
			{
				selectStartTrack = selectStopTrack = editTrack;
				selectStartRow   = selectStopRow   = editRow;
//				selectActive = true;
//				printf("select active\n");
			}
			break;
		
		default:
			break;
		}
	}
	
	switch (keyCode)
	{
	case VK_RETURN: onReturn(); break;
	case VK_DELETE: onDelete(); break;
	
	case VK_BACK:
		if (editString.size() > 0)
		{
			editString.resize(editString.size() - 1);
			invalidatePos(editTrack, editRow);
		}
		else MessageBeep(0);
		break;
	
	case VK_CANCEL:
	case VK_ESCAPE:
		if (selectActive)
		{
			selectActive = false;
			invalidateRange(selectStartTrack, selectStopTrack, selectStartRow, selectStopRow);
		}
		if (editString.size() > 0)
		{
			// return to old value (i.e don't clear)
			editString.clear();
			invalidatePos(editTrack, editRow);
			MessageBeep(0);
		}
		break;
	}
	return FALSE;
}

LRESULT TrackView::onChar(UINT keyCode, UINT flags)
{
	printf("char: \"%c\" (%d) - flags: %x\n", (char)keyCode, keyCode, flags);
	switch ((char)keyCode)
	{
	case '.':
		// only one '.' allowed
		if (std::string::npos != editString.find('.'))
		{
			MessageBeep(0);
			break;
		}
	
	case '0':
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		editString.push_back(keyCode);
		printf("accepted: %c - %s\n", (char)keyCode, editString.c_str());
		invalidatePos(editTrack, editRow);
		break;
	}
	return FALSE;
}

LRESULT TrackView::onSize(int width, int height)
{
	const int oldWindowWidth = windowWidth;
	const int oldWindowHeight = windowHeight;
	
	windowWidth  = width;
	windowHeight = height;
	
	windowRows   = (height - topMarginHeight) / fontHeight;
	windowTracks = (width  - leftMarginWidth) / trackWidth;
	
	setEditRow(editRow);
	setupScrollBars();
	return FALSE;
}

LRESULT TrackView::windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	ASSERT(hwnd == this->hwnd);
	
	switch(msg)
	{
	case WM_CREATE:  return onCreate();
	
	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;
	
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	
	case WM_SIZE:    return onSize(LOWORD(lParam), HIWORD(lParam));
	case WM_VSCROLL: return onVScroll(LOWORD(wParam), getScrollPos(hwnd, SB_VERT));
	case WM_HSCROLL: return onHScroll(LOWORD(wParam), getScrollPos(hwnd, SB_HORZ));
	case WM_PAINT:   return onPaint();
	case WM_KEYDOWN: return onKeyDown((UINT)wParam, (UINT)lParam);
	case WM_CHAR:    return onChar((UINT)wParam, (UINT)lParam);
	
	case WM_COPY:  copy();  break;
	case WM_CUT:   cut();   break;
	case WM_PASTE: paste(); break;
	
	case WM_UNDO:
		if (!syncData->undo()) MessageBeep(0);
		// unfortunately, we don't know how much to invalidate... so we'll just invalidate it all.
		InvalidateRect(hwnd, NULL, TRUE);
		break;
	
	case WM_REDO:
		if (!syncData->redo()) MessageBeep(0);
		// unfortunately, we don't know how much to invalidate... so we'll just invalidate it all.
		InvalidateRect(hwnd, NULL, TRUE);
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
		ASSERT(NULL != trackView);
		{
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

