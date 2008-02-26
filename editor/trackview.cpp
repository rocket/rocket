/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#include "trackview.h"
#include <vector>

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
	
	this->hwnd = NULL;
	
	bgBaseBrush = GetSysColorBrush(COLOR_WINDOW);
	bgDarkBrush = CreateSolidBrush(darken(GetSysColor(COLOR_WINDOW), 0.9f));
	
	selectBaseBrush = GetSysColorBrush(COLOR_HIGHLIGHT);
	selectDarkBrush = CreateSolidBrush(darken(GetSysColor(COLOR_HIGHLIGHT), 0.9f));
	
	rowPen       = CreatePen(PS_SOLID, 1, darken(GetSysColor(COLOR_WINDOW), 0.7f));
	rowSelectPen = CreatePen(PS_SOLID, 1, darken(GetSysColor(COLOR_HIGHLIGHT), 0.7f));

	lerpPen   = CreatePen(PS_SOLID, 2, RGB(255, 0, 0));
	cosinePen = CreatePen(PS_SOLID, 2, RGB(0, 255, 0));
	rampPen   = CreatePen(PS_SOLID, 2, RGB(0, 0, 255));
	
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
	DeleteObject(rowPen);
	DeleteObject(rowSelectPen);
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
	
	SyncDocument *document = getDocument();
	if (NULL == document) return;

	SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
	
	sync::Data::TrackContainer::iterator trackIter = document->tracks.begin();
	for (int track = 0; track <= lastTrack; ++track, ++trackIter)
	{
		assert(trackIter != document->tracks.end());
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
		
		if (this->document->clientRemap.count(track) == 0) SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
		else SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
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
	
	int selectLeft  = min(selectStartTrack, selectStopTrack);
	int selectRight = max(selectStartTrack, selectStopTrack);
	int selectTop    = min(selectStartRow, selectStopRow);
	int selectBottom = max(selectStartRow, selectStopRow);
	
	sync::Data::TrackContainer::iterator trackIter = document->tracks.begin();
	for (int track = 0; track <= lastTrack; ++track, ++trackIter)
	{
		assert(trackIter != document->tracks.end());
		size_t trackIndex = trackIter->second;
		const sync::Track &t = document->getTrack(trackIndex);

		if (track < firstTrack) continue;
		for (int row = firstRow; row <= lastRow; ++row)
		{
			RECT patternDataRect;
			patternDataRect.left = getScreenX(track);
			patternDataRect.right = patternDataRect.left + trackWidth;
			patternDataRect.top = getScreenY(row);
			patternDataRect.bottom = patternDataRect.top + fontHeight;
			
			if (!RectVisible(hdc, &patternDataRect)) continue;
			
			sync::Track::KeyFrame::InterpolationType interpolationType = sync::Track::KeyFrame::IT_STEP;
			sync::Track::KeyFrameContainer::const_iterator upper = t.keyFrames.upper_bound(row);
			sync::Track::KeyFrameContainer::const_iterator lower = upper;
			if (lower != t.keyFrames.end())
			{
				lower--;
				if (lower != t.keyFrames.end()) interpolationType = lower->second.interpolationType;
			}
			
			bool selected = (track >= selectLeft && track <= selectRight) && (row >= selectTop && row <= selectBottom);
			
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
			if (row % 8 == 0)
			{
				MoveToEx(hdc, patternDataRect.left, patternDataRect.top, (LPPOINT) NULL); 
				if (selected) SelectObject(hdc, rowSelectPen);
				else          SelectObject(hdc, rowPen);
				LineTo(hdc,   patternDataRect.right, patternDataRect.top); 
			}
			
			switch (interpolationType)
			{
			case sync::Track::KeyFrame::IT_LERP:
				SelectObject(hdc, lerpPen);
				break;
			case sync::Track::KeyFrame::IT_COSINE:
				SelectObject(hdc, cosinePen);
				break;
			case sync::Track::KeyFrame::IT_RAMP:
				SelectObject(hdc, rampPen);
				break;
			case sync::Track::KeyFrame::IT_STEP:
				break;
			}
			if (interpolationType != sync::Track::KeyFrame::IT_STEP)
			{
				MoveToEx(hdc, patternDataRect.right - 1, patternDataRect.top, (LPPOINT) NULL); 
				LineTo(hdc,   patternDataRect.right - 1, patternDataRect.bottom);
			}
			
			bool drawEditString = false;
			if (row == editRow && track == editTrack)
			{
				FrameRect(hdc, &fillRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
//				DrawFocusRect(hdc, &fillRect);
//				Rectangle(hdc, fillRect.left, fillRect.top, fillRect.right, fillRect.bottom);
				if (editString.size() > 0) drawEditString = true;
			}
			
			bool key = t.isKeyFrame(row);
			
			/* format the text */
			if (drawEditString) _sntprintf_s(temp, 256, editString.c_str());
			else if (!key) _sntprintf_s(temp, 256, _T("  ---"));
			else
			{
				float val = t.getKeyFrame(row)->value;
				_sntprintf_s(temp, 256, _T("% .2f"), val);
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

struct CopyEntry
{
	int track, row;
	sync::Track::KeyFrame keyFrame;
};


void TrackView::editCopy()
{
	int selectLeft  = min(selectStartTrack, selectStopTrack);
	int selectRight = max(selectStartTrack, selectStopTrack);
	int selectTop    = min(selectStartRow, selectStopRow);
	int selectBottom = max(selectStartRow, selectStopRow);
	
	if (FAILED(OpenClipboard(getWin())))
	{
		MessageBox(NULL, _T("Failed to open clipboard"), NULL, MB_OK);
		return;
	}
	
	// gather data
	int rows = selectBottom - selectTop + 1;
	int columns = selectRight - selectLeft + 1;
	size_t cells = columns * rows;
	
	std::vector<struct CopyEntry> copyEntries;
	for (int track = selectLeft; track <= selectRight; ++track)
	{
		const size_t trackIndex  = document->getTrackIndexFromPos(track);
		const sync::Track &t = document->getTrack(trackIndex);
		
		for (int row = selectTop; row <= selectBottom; ++row)
		{
			int localRow = row - selectTop;
			if (t.isKeyFrame(row))
			{
				const sync::Track::KeyFrame *keyFrame = t.getKeyFrame(row);
				assert(NULL != keyFrame);
				
				CopyEntry ce;
				ce.track = int(trackIndex);
				ce.row = localRow;
				ce.keyFrame = *keyFrame;
				
				copyEntries.push_back(ce);
			}
		}
	}
	
	int buffer_width  = selectRight - selectLeft + 1;
	int buffer_height = selectBottom - selectTop + 1;
	size_t buffer_size = copyEntries.size();
	
	HGLOBAL hmem = GlobalAlloc(GMEM_MOVEABLE, sizeof(int) * 3 + sizeof(CopyEntry) * copyEntries.size());
	char *clipbuf = (char *)GlobalLock(hmem);
	
	// copy data
	memcpy(clipbuf + 0,                                &buffer_width,  sizeof(int));
	memcpy(clipbuf + sizeof(int),                      &buffer_height, sizeof(int));
	memcpy(clipbuf + 2 * sizeof(int),                  &buffer_size,   sizeof(size_t));
	if (copyEntries.size() > 0 ) memcpy(clipbuf + 2 * sizeof(int) + sizeof(size_t), &copyEntries[0], sizeof(CopyEntry) * copyEntries.size());
	
	GlobalUnlock(hmem);
	clipbuf = NULL;
	
	// update clipboard
	EmptyClipboard();
	SetClipboardData(clipboardFormat, hmem);
	CloseClipboard();
}

void TrackView::editCut()
{
	editCopy();
	editDelete();
}

void TrackView::editPaste()
{
#if 1
	if (FAILED(OpenClipboard(getWin())))
	{
		MessageBox(NULL, _T("Failed to open clipboard"), NULL, MB_OK);
		return;
	}
	
	if (IsClipboardFormatAvailable(clipboardFormat))
	{
		HGLOBAL hmem = GetClipboardData(clipboardFormat);
		char *clipbuf = (char *)GlobalLock(hmem);
		
		// copy data
		int buffer_width, buffer_height, buffer_size;
		memcpy(&buffer_width,  clipbuf + 0,               sizeof(int));
		memcpy(&buffer_height, clipbuf + sizeof(int),     sizeof(int));
		memcpy(&buffer_size,   clipbuf + 2 * sizeof(int), sizeof(size_t));
			
		if (buffer_size > 0)
		{
			char *src = clipbuf + 2 * sizeof(int) + sizeof(size_t);
			
			SyncDocument::MultiCommand *multiCmd = new SyncDocument::MultiCommand();
			for (int i = 0; i < buffer_size; ++i)
			{
				struct CopyEntry ce;
				memcpy(&ce, src, sizeof(CopyEntry));
				
				size_t trackIndex = document->getTrackIndexFromPos(editTrack + ce.track);
				
				SyncDocument::Command *cmd = document->getSetKeyFrameCommand(int(trackIndex), editRow + ce.row, ce.keyFrame);
				multiCmd->addCommand(cmd);
				src += sizeof(CopyEntry);
			}
			document->exec(multiCmd);
		}
		
		GlobalUnlock(hmem);
		clipbuf = NULL;
	}
	else MessageBeep(0);
	
	CloseClipboard();
#endif
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
	
	if (oldEditRow != editRow)
	{
		if (GetKeyState(VK_SHIFT) < 0)
		{
			selectStopRow = editRow;
			invalidateRange(selectStartTrack, selectStopTrack, oldEditRow, editRow);
		}
		else
		{
			invalidateRange(selectStartTrack, selectStopTrack, selectStartRow, selectStopRow);
			selectStartRow   = selectStopRow   = editRow;
			selectStartTrack = selectStopTrack = editTrack;
		}
		if (document->clientPaused)
		{
			document->sendSetRowCommand(editRow);
		}
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
	
	if (oldEditTrack != editTrack)
	{
		if (GetKeyState(VK_SHIFT) < 0)
		{
			selectStopTrack = editTrack;
			invalidateRange(oldEditTrack, editTrack, selectStartRow, selectStopRow);
		}
		else
		{
			invalidateRange(selectStartTrack, selectStopTrack, selectStartRow, selectStopRow);
			selectStartRow   = selectStopRow   = editRow;
			selectStartTrack = selectStopTrack = editTrack;
		}
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
	this->rows = rows;
	InvalidateRect(getWin(), NULL, FALSE);
	setEditRow(min(editRow, rows - 1));
}


LRESULT TrackView::onVScroll(UINT sbCode, int /*newPos*/)
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

LRESULT TrackView::onHScroll(UINT sbCode, int /*newPos*/)
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

void TrackView::editEnterValue()
{
	if (int(editString.size()) > 0 && editTrack < int(document->getTrackCount()))
	{
		size_t trackIndex = document->getTrackIndexFromPos(editTrack);
		sync::Track &t = document->getTrack(trackIndex);
		
		sync::Track::KeyFrame newKey;
		if (t.isKeyFrame(editRow)) newKey = *t.getKeyFrame(editRow); // copy old key
		newKey.value = float(_tstof(editString.c_str())); // modify value
		
		SyncDocument::Command *cmd = document->getSetKeyFrameCommand(int(trackIndex), editRow, newKey);
		document->exec(cmd);
		
		editString.clear();
//		invalidatePos(editTrack, editRow);
		InvalidateRect(getWin(), NULL, FALSE);
	}
	else MessageBeep(0);
}

void TrackView::editToggleInterpolationType()
{
	if (editTrack < int(document->getTrackCount()))
	{
		size_t trackIndex = document->getTrackIndexFromPos(editTrack);
		sync::Track &t = document->getTrack(trackIndex);
		
		// find key to modify
		sync::Track::KeyFrameContainer::const_iterator upper = t.keyFrames.upper_bound(editRow);
		// bounds check
		if (upper == t.keyFrames.end())
		{
			MessageBeep(0);
			return;
		}
		
		sync::Track::KeyFrameContainer::const_iterator lower = upper;
		lower--;
		// bounds check again
		if (lower == t.keyFrames.end())
		{
			MessageBeep(0);
			return;
		}
		
		sync::Track::KeyFrame newKey = lower->second;
		// modify interpolation type
		newKey.interpolationType = sync::Track::KeyFrame::InterpolationType(
			(int(newKey.interpolationType) + 1) % sync::Track::KeyFrame::IT_COUNT
		);
		
		SyncDocument::Command *cmd = document->getSetKeyFrameCommand(int(trackIndex), int(lower->first), newKey);
		document->exec(cmd);
		
		invalidateRange(editTrack, editTrack, int(lower->first), int(upper->first));
	}
	else MessageBeep(0);
}

void TrackView::editDelete()
{
	int selectLeft  = min(selectStartTrack, selectStopTrack);
	int selectRight = max(selectStartTrack, selectStopTrack);
	int selectTop    = min(selectStartRow, selectStopRow);
	int selectBottom = max(selectStartRow, selectStopRow);
	
	if (0 == document->getTrackCount()) return;
	assert(selectRight < int(document->getTrackCount()));
	
	SyncDocument::MultiCommand *multiCmd = new SyncDocument::MultiCommand();
	for (int track = selectLeft; track <= selectRight; ++track)
	{
		size_t trackIndex = document->getTrackIndexFromPos(track);
		sync::Track &t = document->getTrack(trackIndex);
		
		for (int row = selectTop; row <= selectBottom; ++row)
		{
			if (t.isKeyFrame(row))
			{
				SyncDocument::Command *cmd = new SyncDocument::DeleteCommand(int(trackIndex), row);
				multiCmd->addCommand(cmd);
			}
		}
	}
	
	if (0 == multiCmd->getSize())
	{
		MessageBeep(0);
		delete multiCmd;
	}
	else
	{
		document->exec(multiCmd);
		InvalidateRect(getWin(), NULL, FALSE);
//		invalidateRange(selectLeft, selectRight, selectTop, selectBottom);
	}
}

void TrackView::editBiasValue(float amount)
{
	int selectLeft  = min(selectStartTrack, selectStopTrack);
	int selectRight = max(selectStartTrack, selectStopTrack);
	int selectTop    = min(selectStartRow, selectStopRow);
	int selectBottom = max(selectStartRow, selectStopRow);

	SyncDocument::MultiCommand *multiCmd = new SyncDocument::MultiCommand();
	for (int track = selectLeft; track <= selectRight; ++track)
	{
		size_t trackIndex = document->getTrackIndexFromPos(track);
		sync::Track &t = document->getTrack(trackIndex);
		
		for (int row = selectTop; row <= selectBottom; ++row)
		{
			if (t.isKeyFrame(row))
			{
				sync::Track::KeyFrame newKey = *t.getKeyFrame(row); // copy old key
				newKey.value += amount; // modify value
				
				// add sub-command
				SyncDocument::Command *cmd = document->getSetKeyFrameCommand(int(trackIndex), row, newKey);
				multiCmd->addCommand(cmd);
			}
		}
	}
	
	if (0 == multiCmd->getSize())
	{
		MessageBeep(0);
		delete multiCmd;
	}
	else
	{
		document->exec(multiCmd);
		invalidateRange(selectLeft, selectRight, selectTop, selectBottom);
	}
}

LRESULT TrackView::onKeyDown(UINT keyCode, UINT /*flags*/)
{
	if (editString.empty() && document->clientPaused)
	{
		switch (keyCode)
		{
		case VK_UP:
			if (GetKeyState(VK_CONTROL) < 0)
			{
				float bias = 1.0f;
				if (GetKeyState(VK_SHIFT) < 0) bias = 0.1f;
				editBiasValue(bias);
			}
			else setEditRow(editRow - 1);
			break;
		
		case VK_DOWN:
			if (GetKeyState(VK_CONTROL) < 0)
			{
				float bias = 1.0f;
				if (GetKeyState(VK_SHIFT) < 0) bias = 0.1f;
				editBiasValue(-bias);
			}
			else setEditRow(editRow + 1);
			break;
		
		case VK_LEFT:  setEditTrack(editTrack - 1); break;
		case VK_RIGHT: setEditTrack(editTrack + 1); break;
		
		case VK_PRIOR:
			if (GetKeyState(VK_CONTROL) < 0)
			{
				float bias = 10.0f;
				if (GetKeyState(VK_SHIFT) < 0) bias = 100.0f;
				editBiasValue(bias);
			}
			else setEditRow(editRow - 0x10);
			break;
		
		case VK_NEXT:
			if (GetKeyState(VK_CONTROL) < 0)
			{
				float bias = 10.0f;
				if (GetKeyState(VK_SHIFT) < 0) bias = 100.0f;
				editBiasValue(-bias);
			}
			else setEditRow(editRow + 0x10);
			break;
		case VK_HOME:
			if (GetKeyState(VK_CONTROL) < 0)
			{
				setEditRow(0);
			}
			else
			{
				int remainder = editRow % 0x80;
				if(remainder)
				{
					setEditRow(editRow - remainder);
				}
				else
				{
					setEditRow(editRow - 0x80);
				}
			}
			break;
		case VK_END:
			if (GetKeyState(VK_CONTROL) < 0)
			{
				setEditRow(rows-1);
			}
			else
			{
				int remainder = editRow % 0x80;
				if(remainder)
				{
					setEditRow(0x80 + editRow - remainder);
				}
				else
				{
					setEditRow(editRow + 0x80);
				}
			}
			break;
		}
	}
	
	switch (keyCode)
	{
	case VK_RETURN: editEnterValue(); break;
	case VK_DELETE: editDelete(); break;
	
	case VK_BACK:
		if (!editString.empty())
		{
			editString.resize(editString.size() - 1);
			invalidatePos(editTrack, editRow);
		}
		else MessageBeep(0);
		break;
	
	case VK_CANCEL:
	case VK_ESCAPE:
		if (!editString.empty())
		{
			// return to old value (i.e don't clear)
			editString.clear();
			invalidatePos(editTrack, editRow);
			MessageBeep(0);
		}
		break;
	case VK_SPACE:
		if (!editString.empty())
		{
			editString.clear();
			invalidatePos(editTrack, editRow);
			MessageBeep(0);
		}
		document->sendPauseCommand( !document->clientPaused );
		break;
	}
	return FALSE;
}

LRESULT TrackView::onChar(UINT keyCode, UINT flags)
{
	switch (char(keyCode))
	{
	case '-':
		if (editString.empty())
		{
			editString.push_back(char(keyCode));
			invalidatePos(editTrack, editRow);
		}
		break;
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
		if (editTrack < int(document->getTrackCount()))
		{
			editString.push_back(char(keyCode));
			invalidatePos(editTrack, editRow);
		}
		else MessageBeep(0);
		break;
	
	case 'i':
		editToggleInterpolationType();
		break;
	}
	return FALSE;
}

LRESULT TrackView::onSize(int width, int height)
{
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
	assert(hwnd == this->hwnd);
	
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
	
	case WM_COPY:  editCopy();  break;
	case WM_CUT:   editCut();   break;
	case WM_PASTE:
		editPaste();
		InvalidateRect(hwnd, NULL, FALSE);
		break;
	
	case WM_UNDO:
		if (!document->undo()) MessageBeep(0);
		// unfortunately, we don't know how much to invalidate... so we'll just invalidate it all.
		InvalidateRect(hwnd, NULL, FALSE);
		break;
	
	case WM_REDO:
		if (!document->redo()) MessageBeep(0);
		// unfortunately, we don't know how much to invalidate... so we'll just invalidate it all.
		InvalidateRect(hwnd, NULL, FALSE);
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
		assert(NULL != trackView);
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
		assert(NULL != trackView);
		return trackView->windowProc(hwnd, msg, wParam, lParam);
	}
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
	wc.hCursor       = NULL; // LoadCursor(NULL, IDC_IBEAM);
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
		hwndParent, NULL, hInstance, (void*)this
	);
	return hwnd;
}

