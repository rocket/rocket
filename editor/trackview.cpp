/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in COPYING
 */

#include "trackview.h"
#include <vector>
#include <algorithm>
#include <stdio.h>

using std::min;
using std::max;

static const char *trackViewWindowClassName = "TrackView";

static DWORD darken(DWORD col, float amt)
{
	return RGB(GetRValue(col) * amt, GetGValue(col) * amt, GetBValue(col) * amt);
}

static int getMaxCharacterWidth(HDC hdc, const char *chars, size_t len)
{
	int maxDigitWidth = 0;
	for (size_t i = 0; i < len; ++i)
	{
		SIZE size;
		GetTextExtentPoint32(hdc, &chars[i], 1, &size);
		maxDigitWidth = max(maxDigitWidth, int(size.cx));
	}
	return maxDigitWidth;
}

static int getMaxCharacterWidthFromString(HDC hdc, const char *chars)
{
	return getMaxCharacterWidth(hdc, chars, strlen(chars));
}

TrackView::TrackView() :
	document(NULL)
{
	scrollPosX = 0;
	scrollPosY = 0;
	windowWidth  = -1;
	windowHeight = -1;
	
	editRow = 0;
	editTrack = 0;
	
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

	handCursor = LoadCursor(NULL, IDC_HAND);

	clipboardFormat = RegisterClipboardFormat("syncdata");
	assert(0 != clipboardFormat);
}

TrackView::~TrackView()
{
	DeleteObject(bgBaseBrush);
	DeleteObject(bgDarkBrush);
	DeleteObject(selectBaseBrush);
	DeleteObject(selectDarkBrush);
	DeleteObject(lerpPen);
	DeleteObject(cosinePen);
	DeleteObject(rampPen);
	DeleteObject(editBrush);
	DeleteObject(rowPen);
	DeleteObject(rowSelectPen);
	if (document)
		delete document;
}

void TrackView::setFont(HFONT font)
{
	this->font = font;
	
	assert(NULL != hwnd);
	HDC hdc = GetDC(hwnd);
	SelectObject(hdc, font);
	
	TEXTMETRIC tm;
	GetTextMetrics(hdc, &tm);
	
	rowHeight = tm.tmHeight + tm.tmExternalLeading;
	fontWidth = tm.tmAveCharWidth;
	trackWidth = getMaxCharacterWidthFromString(hdc, "0123456789.") * 16;
	
	topMarginHeight = rowHeight + 4;
	leftMarginWidth = getMaxCharacterWidthFromString(hdc, "0123456789abcdefh") * 8;
}

int TrackView::getScreenY(int row) const
{
	return topMarginHeight + (row * rowHeight) - scrollPosY;
}

int TrackView::getScreenX(size_t track) const
{
	return int(leftMarginWidth + (track * trackWidth)) - scrollPosX;
}

inline int divfloor(int a, int b)
{
	if (a < 0)
		return -abs(a) / b - 1;
	return a / b;
}

int TrackView::getTrackFromX(int x) const
{
	return divfloor(x + scrollPosX - leftMarginWidth, trackWidth);
}


LRESULT TrackView::onCreate()
{
//	setFont((HFONT)GetStockObject(SYSTEM_FONT));
	setFont((HFONT)GetStockObject(SYSTEM_FIXED_FONT));

	setupScrollBars();
	return FALSE;
}

LRESULT TrackView::onPaint()
{
	PAINTSTRUCT ps;
	HDC hdc = BeginPaint(hwnd, &ps);
	
//	SelectObject(hdc, GetStockObject(SYSTEM_FIXED_FONT));
	SelectObject(hdc, font);
	paintTracks(hdc, ps.rcPaint);
	
	EndPaint(hwnd, &ps);
	
	return FALSE;
}

void TrackView::paintTopMargin(HDC hdc, RECT rcTracks)
{
	RECT fillRect;
	RECT topLeftMargin;
	const SyncDocument *doc = getDocument();
	if (NULL == doc) return;
	
	topLeftMargin.top = 0;
	topLeftMargin.bottom = topMarginHeight;
	topLeftMargin.left = 0;
	topLeftMargin.right = leftMarginWidth;
	fillRect = topLeftMargin;
	DrawEdge(hdc, &fillRect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_BOTTOM);
	FillRect(hdc, &fillRect, GetSysColorBrush(COLOR_3DFACE));
	
	int startTrack = scrollPosX / trackWidth;
	int endTrack  = min(startTrack + windowTracks + 1, int(getTrackCount()));
	
	SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
	
	for (int track = startTrack; track < endTrack; ++track) {
		size_t index = doc->getTrackIndexFromPos(track);
		const sync_track *t = doc->tracks[index];

		RECT topMargin;
		topMargin.top = 0;
		topMargin.bottom = topMarginHeight;
		topMargin.left = getScreenX(track);
		topMargin.right = topMargin.left + trackWidth;

		if (!RectVisible(hdc, &topMargin))
			continue;

		RECT fillRect = topMargin;

		HBRUSH bgBrush = GetSysColorBrush(COLOR_3DFACE);
		if (track == editTrack)
			bgBrush = editBrush;

		DrawEdge(hdc, &fillRect, BDR_RAISEDINNER | BDR_RAISEDOUTER, BF_ADJUST | BF_LEFT | BF_RIGHT | BF_BOTTOM);
		FillRect(hdc, &fillRect, bgBrush);

		if (!doc->clientSocket.clientTracks.count(t->name))
			SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
		else
			SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
		TextOut(hdc, fillRect.left, 0, t->name, int(strlen(t->name)));
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
	const SyncDocument *doc = getDocument();
	if (NULL == doc) return;
	
	char temp[256];
	
	int firstRow = editRow - windowRows / 2 - 1;
	int lastRow  = editRow + windowRows / 2 + 1;
	
	/* clamp first & last row */
	firstRow = min(max(firstRow, 0), int(getRows()) - 1);
	lastRow  = min(max(lastRow,  0), int(getRows()) - 1);
	
	SetBkMode(hdc, TRANSPARENT);
	paintTopMargin(hdc, rcTracks);
	
	for (int row = firstRow; row <= lastRow; ++row)
	{
		RECT leftMargin;
		leftMargin.left  = 0;
		leftMargin.right = leftMarginWidth;
		leftMargin.top    = getScreenY(row);
		leftMargin.bottom = leftMargin.top + rowHeight;
		
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
		
		snprintf(temp, 256, "%0*Xh", 5, row);
		TextOut(hdc,
			leftMargin.left, leftMargin.top,
			temp, int(strlen(temp))
		);
	}
	
	SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
	
	int selectLeft  = min(selectStartTrack, selectStopTrack);
	int selectRight = max(selectStartTrack, selectStopTrack);
	int selectTop    = min(selectStartRow, selectStopRow);
	int selectBottom = max(selectStartRow, selectStopRow);
	
	int startTrack = scrollPosX / trackWidth;
	int endTrack  = min(startTrack + windowTracks + 1, int(getTrackCount()));
	
	for (int track = startTrack; track < endTrack; ++track) {
		const sync_track *t = doc->tracks[doc->getTrackIndexFromPos(track)];
		for (int row = firstRow; row <= lastRow; ++row) {
			RECT patternDataRect;
			patternDataRect.left = getScreenX(track);
			patternDataRect.right = patternDataRect.left + trackWidth;
			patternDataRect.top = getScreenY(row);
			patternDataRect.bottom = patternDataRect.top + rowHeight;
			
			if (!RectVisible(hdc, &patternDataRect))
				continue;

			int idx = sync_find_key(t, row);
			int fidx = idx >= 0 ? idx : -idx - 2;
			key_type interpolationType = (fidx >= 0) ? t->keys[fidx].type : KEY_STEP;
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
			FillRect( hdc, &fillRect, bgBrush);
			if (row % 8 == 0) {
				MoveToEx(hdc, patternDataRect.left, patternDataRect.top, (LPPOINT) NULL); 
				if (selected) SelectObject(hdc, rowSelectPen);
				else          SelectObject(hdc, rowPen);
				LineTo(hdc,   patternDataRect.right, patternDataRect.top); 
			}
			
			switch (interpolationType) {
			case KEY_STEP:
				break;
			case KEY_LINEAR:
				SelectObject(hdc, lerpPen);
				break;
			case KEY_SMOOTH:
				SelectObject(hdc, cosinePen);
				break;
			case KEY_RAMP:
				SelectObject(hdc, rampPen);
				break;
			}
			if (interpolationType != KEY_STEP) {
				MoveToEx(hdc, patternDataRect.right - 1, patternDataRect.top, (LPPOINT) NULL); 
				LineTo(hdc,   patternDataRect.right - 1, patternDataRect.bottom);
			}

			bool drawEditString = false;
			if (row == editRow && track == editTrack) {
				FrameRect(hdc, &fillRect, (HBRUSH)GetStockObject(BLACK_BRUSH));
				if (editString.size() > 0)
					drawEditString = true;
			}
			/* format the text */
			if (drawEditString)
				snprintf(temp, 256, editString.c_str());
			else if (idx < 0)
				snprintf(temp, 256, "  ---");
			else {
				float val = t->keys[idx].value;
				snprintf(temp, 256, "% .2f", val);
			}

			COLORREF oldCol;
			if (selected) oldCol = SetTextColor(hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
			TextOut(hdc,
				patternDataRect.left, patternDataRect.top,
				temp, int(strlen(temp))
			);
			if (selected) SetTextColor(hdc, oldCol);
		}
	}
	
	/* right margin */
	{
		RECT rightMargin;
		rightMargin.top    = getScreenY(0);
		rightMargin.bottom = getScreenY(int(getRows()));
		rightMargin.left  = getScreenX(getTrackCount());
		rightMargin.right = rcTracks.right;
		FillRect( hdc, &rightMargin, GetSysColorBrush(COLOR_APPWORKSPACE));
	}
	
	{
		RECT bottomPadding;
		bottomPadding.top = getScreenY(int(getRows()));
		bottomPadding.bottom = rcTracks.bottom;
		bottomPadding.left = rcTracks.left;
		bottomPadding.right = rcTracks.right;
		FillRect(hdc, &bottomPadding, GetSysColorBrush(COLOR_APPWORKSPACE));
	}
	
	{
		RECT topPadding;
		topPadding.top = max(int(rcTracks.top), topMarginHeight);
		topPadding.bottom = getScreenY(0);
		topPadding.left = rcTracks.left;
		topPadding.right = rcTracks.right;
		FillRect(hdc, &topPadding, GetSysColorBrush(COLOR_APPWORKSPACE));
	}
}

struct CopyEntry
{
	int track;
	track_key keyFrame;
};

void TrackView::editCopy()
{
	const SyncDocument *doc = getDocument();
	if (NULL == doc) return;
	
	if (0 == getTrackCount()) {
		MessageBeep(~0U);
		return;
	}
	
	int selectLeft  = min(selectStartTrack, selectStopTrack);
	int selectRight = max(selectStartTrack, selectStopTrack);
	int selectTop    = min(selectStartRow, selectStopRow);
	int selectBottom = max(selectStartRow, selectStopRow);
	
	if (FAILED(OpenClipboard(getWin())))
	{
		MessageBox(NULL, "Failed to open clipboard", NULL, MB_OK);
		return;
	}

	std::vector<struct CopyEntry> copyEntries;
	for (int track = selectLeft; track <= selectRight; ++track) {
		const size_t trackIndex  = doc->getTrackIndexFromPos(track);
		const sync_track *t = doc->tracks[trackIndex];

		for (int row = selectTop; row <= selectBottom; ++row) {
			int idx = sync_find_key(t, row);
			if (idx >= 0) {
				CopyEntry ce;
				ce.track = track - selectLeft;
				ce.keyFrame = t->keys[idx];
				ce.keyFrame.row -= selectTop;
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
	if (0 == getTrackCount()) {
		MessageBeep(~0U);
		return;
	}
	
	editCopy();
	editDelete();
}

void TrackView::editPaste()
{
	SyncDocument *doc = getDocument();
	if (NULL == doc) return;
	
	if (0 == getTrackCount()) {
		MessageBeep(~0U);
		return;
	}
	
	if (FAILED(OpenClipboard(getWin())))
	{
		MessageBox(NULL, "Failed to open clipboard", NULL, MB_OK);
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
		
		SyncDocument::MultiCommand *multiCmd = new SyncDocument::MultiCommand();
		for (int i = 0; i < buffer_width; ++i) {
			size_t trackPos = editTrack + i;
			if (trackPos >= getTrackCount()) continue;

			size_t trackIndex = doc->getTrackIndexFromPos(trackPos);
			const sync_track *t = doc->tracks[trackIndex];
			for (int j = 0; j < buffer_height; ++j) {
				int row = editRow + j;
				if (is_key_frame(t, row))
					multiCmd->addCommand(new SyncDocument::DeleteCommand(int(trackIndex), row));
			}
		}
		
		char *src = clipbuf + 2 * sizeof(int) + sizeof(size_t);
		for (int i = 0; i < buffer_size; ++i)
		{
			struct CopyEntry ce;
			memcpy(&ce, src, sizeof(CopyEntry));
			src += sizeof(CopyEntry);
			
			assert(ce.track >= 0);
			assert(ce.track < buffer_width);
			assert(ce.keyFrame.row >= 0);
			assert(ce.keyFrame.row < buffer_height);

			size_t trackPos = editTrack + ce.track;
			if (trackPos < getTrackCount())
			{
				size_t trackIndex = doc->getTrackIndexFromPos(trackPos);
				track_key key = ce.keyFrame;
				key.row += editRow;

				// since we deleted all keyframes in the edit-box already, we can just insert this one. 
				SyncDocument::Command *cmd = new SyncDocument::InsertCommand(int(trackIndex), key);
				multiCmd->addCommand(cmd);
			}
		}
		doc->exec(multiCmd);
		
		GlobalUnlock(hmem);
		clipbuf = NULL;
	}
	else
		MessageBeep(~0U);
	
	CloseClipboard();
}

void TrackView::setupScrollBars()
{
	SCROLLINFO si = { sizeof(si) };
	si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE | SIF_DISABLENOSCROLL;
	si.nPos  = editRow;
	si.nPage = windowRows;
	si.nMin  = 0;
	si.nMax  = int(getRows()) - 1 + windowRows - 1;
	SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
	
	si.fMask = SIF_POS | SIF_PAGE | SIF_RANGE | SIF_DISABLENOSCROLL;
	si.nPos  = editTrack;
	si.nPage = windowTracks;
	si.nMin  = 0;
	si.nMax  = int(getTrackCount()) - 1 + windowTracks - 1;
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
	SyncDocument *doc = getDocument();
	if (NULL == doc) return;
	
	int oldEditRow = editRow;
	editRow = newEditRow;
	
	// clamp to document
	editRow = min(max(editRow, 0), int(getRows()) - 1);
	
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
		} if (doc->clientSocket.clientPaused) {
			doc->clientSocket.sendSetRowCommand(editRow);
		}
		SendMessage(GetParent(getWin()), WM_ROWCHANGED, 0, editRow);
		SendMessage(GetParent(getWin()), WM_CURRVALDIRTY, 0, 0);
	}
	
	invalidateRow(oldEditRow);
	invalidateRow(editRow);
	
	setScrollPos(scrollPosX, (editRow * rowHeight) - ((windowHeight - topMarginHeight) / 2) + rowHeight / 2);
}

void TrackView::setEditTrack(int newEditTrack, bool autoscroll)
{
	if (0 == getTrackCount()) return;
	
	int oldEditTrack = editTrack;
	editTrack = newEditTrack;
	
	// clamp to document
	editTrack = max(editTrack, 0);
	editTrack = min(editTrack, int(getTrackCount()) - 1);
	
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
		SendMessage(GetParent(getWin()), WM_TRACKCHANGED, 0, editTrack);
		SendMessage(GetParent(getWin()), WM_CURRVALDIRTY, 0, 0);
	}
	
	invalidateTrack(oldEditTrack);
	invalidateTrack(editTrack);

	if (autoscroll) {
		int firstTrack = scrollPosX / trackWidth;
		int lastTrack  = firstTrack + windowTracks;

		int newFirstTrack = firstTrack;
		if (editTrack >= lastTrack)
			newFirstTrack = editTrack - lastTrack + firstTrack + 1;
		if (editTrack < firstTrack)
			newFirstTrack = editTrack;
		setScrollPos(newFirstTrack * trackWidth, scrollPosY);
	} else
		setupScrollBars();
}

static int getScrollPos(HWND hwnd, int bar)
{
	SCROLLINFO si = { sizeof(si), SIF_TRACKPOS };
	GetScrollInfo(hwnd, bar, &si);
	return int(si.nTrackPos);
}

void TrackView::setRows(size_t rows)
{
	SyncDocument *doc = getDocument();
	assert(doc);

	doc->setRows(rows);
	InvalidateRect(getWin(), NULL, FALSE);
	setEditRow(min(editRow, int(rows) - 1));
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
		setEditTrack(int(getTrackCount()) - 1);
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
	SyncDocument *doc = getDocument();
	if (NULL == doc) return;
	
	if (int(editString.size()) > 0 && editTrack < int(getTrackCount()))
	{
		size_t trackIndex = doc->getTrackIndexFromPos(editTrack);
		const sync_track *t = doc->tracks[trackIndex];

		track_key newKey;
		newKey.type = KEY_STEP;
		newKey.row = editRow;
		int idx = sync_find_key(t, editRow);
		if (idx >= 0)
			newKey = t->keys[idx]; // copy old key
		newKey.value = float(atof(editString.c_str())); // modify value
		editString.clear();

		SyncDocument::Command *cmd = doc->getSetKeyFrameCommand(int(trackIndex), newKey);
		doc->exec(cmd);

		SendMessage(GetParent(getWin()), WM_CURRVALDIRTY, 0, 0);
		InvalidateRect(getWin(), NULL, FALSE);
	}
	else
		MessageBeep(~0U);
}

void TrackView::editToggleInterpolationType()
{
	SyncDocument *doc = getDocument();
	if (NULL == doc) return;
	
	if (editTrack < int(getTrackCount())) {
		size_t trackIndex = doc->getTrackIndexFromPos(editTrack);
		const sync_track *t = doc->tracks[trackIndex];

		int idx = key_idx_floor(t, editRow);
		if (idx < 0) {
			MessageBeep(~0U);
			return;
		}

		// copy and modify
		track_key newKey = t->keys[idx];
		newKey.type = (enum key_type)
		    ((newKey.type + 1) % KEY_TYPE_COUNT);

		// apply change to data-set
		SyncDocument::Command *cmd = doc->getSetKeyFrameCommand(int(trackIndex), newKey);
		doc->exec(cmd);

		// update user interface
		SendMessage(GetParent(getWin()), WM_CURRVALDIRTY, 0, 0);
		InvalidateRect(getWin(), NULL, FALSE);
	}
	else
		MessageBeep(~0U);
}

void TrackView::editDelete()
{
	SyncDocument *doc = getDocument();
	if (NULL == doc) return;
	
	int selectLeft  = min(selectStartTrack, selectStopTrack);
	int selectRight = max(selectStartTrack, selectStopTrack);
	int selectTop    = min(selectStartRow, selectStopRow);
	int selectBottom = max(selectStartRow, selectStopRow);
	
	if (0 == getTrackCount()) return;
	assert(selectRight < int(getTrackCount()));
	
	SyncDocument::MultiCommand *multiCmd = new SyncDocument::MultiCommand();
	for (int track = selectLeft; track <= selectRight; ++track) {
		size_t trackIndex = doc->getTrackIndexFromPos(track);
		const sync_track *t = doc->tracks[trackIndex];

		for (int row = selectTop; row <= selectBottom; ++row) {
			if (is_key_frame(t, row)) {
				SyncDocument::Command *cmd = new SyncDocument::DeleteCommand(int(trackIndex), row);
				multiCmd->addCommand(cmd);
			}
		}
	}
	
	if (0 == multiCmd->getSize()) {
		MessageBeep(~0U);
		delete multiCmd;
	}
	else
	{
		doc->exec(multiCmd);
		
		SendMessage(GetParent(getWin()), WM_CURRVALDIRTY, 0, 0);
		InvalidateRect(getWin(), NULL, FALSE);
	}
}

void TrackView::editBiasValue(float amount)
{
	SyncDocument *doc = getDocument();
	if (NULL == doc) return;
	
	int selectLeft  = min(selectStartTrack, selectStopTrack);
	int selectRight = max(selectStartTrack, selectStopTrack);
	int selectTop    = min(selectStartRow, selectStopRow);
	int selectBottom = max(selectStartRow, selectStopRow);
	
	if (0 == getTrackCount()) {
		MessageBeep(~0U);
		return;
	}
	
	SyncDocument::MultiCommand *multiCmd = new SyncDocument::MultiCommand();
	for (int track = selectLeft; track <= selectRight; ++track) {
		assert(track < int(getTrackCount()));
		size_t trackIndex = doc->getTrackIndexFromPos(track);
		const sync_track *t = doc->tracks[trackIndex];

		for (int row = selectTop; row <= selectBottom; ++row) {
			int idx = sync_find_key(t, row);
			if (idx >= 0) {
				struct track_key k = t->keys[idx]; // copy old key
				k.value += amount; // modify value

				// add sub-command
				SyncDocument::Command *cmd = doc->getSetKeyFrameCommand(int(trackIndex), k);
				multiCmd->addCommand(cmd);
			}
		}
	}
	
	if (0 == multiCmd->getSize()) {
		MessageBeep(~0U);
		delete multiCmd;
	}
	else
	{
		doc->exec(multiCmd);
		
		SendMessage(GetParent(getWin()), WM_CURRVALDIRTY, 0, 0);
		invalidateRange(selectLeft, selectRight, selectTop, selectBottom);
	}
}

LRESULT TrackView::onKeyDown(UINT keyCode, UINT /*flags*/)
{
	SyncDocument *doc = getDocument();
	if (NULL == doc) return FALSE;
	
	if (!editString.empty())
	{
		switch(keyCode)
		{
		case VK_UP:
		case VK_DOWN:
		case VK_LEFT:
		case VK_RIGHT:
		case VK_PRIOR:
		case VK_NEXT:
		case VK_HOME:
		case VK_END:
			editEnterValue();
		}
	}
	
	if (editString.empty())
	{
		switch (keyCode)
		{
		case VK_LEFT:
			if (GetKeyState(VK_CONTROL) < 0) {
				if (0 < editTrack)
					doc->swapTrackOrder(editTrack, editTrack - 1);
				else
					MessageBeep(~0U);
			}
			if (0 != getTrackCount())
				setEditTrack(editTrack - 1);
			else
				MessageBeep(~0U);
			break;
		
		case VK_RIGHT:
			if (GetKeyState(VK_CONTROL) < 0) {
				if (int(getTrackCount()) > editTrack + 1)
					doc->swapTrackOrder(editTrack, editTrack + 1);
				else
					MessageBeep(~0U);
			}
			if (0 != getTrackCount())
				setEditTrack(editTrack + 1);
			else
				MessageBeep(~0U);
			break;
		}
	}

	if (editString.empty() && doc->clientSocket.clientPaused) {
		switch (keyCode) {
		case VK_UP:
			if (GetKeyState(VK_CONTROL) < 0)
			{
				float bias = 1.0f;
				if (GetKeyState(VK_SHIFT) < 0) bias = 0.1f;
				if (int(getTrackCount()) > editTrack) editBiasValue(bias);
				else
					MessageBeep(~0U);
			}
			else setEditRow(editRow - 1);
			break;
		
		case VK_DOWN:
			if (GetKeyState(VK_CONTROL) < 0)
			{
				float bias = 1.0f;
				if (GetKeyState(VK_SHIFT) < 0) bias = 0.1f;
				if (int(getTrackCount()) > editTrack) editBiasValue(-bias);
				else
					MessageBeep(~0U);
			}
			else setEditRow(editRow + 1);
			break;
		
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
			if (GetKeyState(VK_CONTROL) < 0) setEditTrack(0);
			else setEditRow(0);
			break;
		
		case VK_END:
			if (GetKeyState(VK_CONTROL) < 0) setEditTrack(int(getTrackCount()) - 1);
			else setEditRow(int(getRows()) - 1);
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
		else
			MessageBeep(~0U);
		break;
	
	case VK_CANCEL:
	case VK_ESCAPE:
		if (!editString.empty()) {
			// return to old value (i.e don't clear)
			editString.clear();
			invalidatePos(editTrack, editRow);
			MessageBeep(~0U);
		}
		break;
	case VK_SPACE:
		if (!editString.empty()) {
			editString.clear();
			invalidatePos(editTrack, editRow);
			MessageBeep(~0U);
		}
		doc->clientSocket.sendPauseCommand( !doc->clientSocket.clientPaused );
		break;
	}
	return FALSE;
}

LRESULT TrackView::onChar(UINT keyCode, UINT /*flags*/)
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
		if (std::string::npos != editString.find('.')) {
			MessageBeep(~0U);
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
		if (editTrack < int(getTrackCount()))
		{
			editString.push_back(char(keyCode));
			invalidatePos(editTrack, editRow);
		}
		else
			MessageBeep(~0U);
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
	
	windowRows   = (height - topMarginHeight) / rowHeight;
	windowTracks = (width  - leftMarginWidth) / trackWidth;
	
	setEditRow(editRow);
	setupScrollBars();
	return FALSE;
}

LRESULT TrackView::onSetCursor(HWND /*win*/, UINT hitTest, UINT message)
{
	POINT cpos;
	GetCursorPos(&cpos);
	ScreenToClient(hwnd, &cpos);
	int track = getTrackFromX(cpos.x);
	if (cpos.y < topMarginHeight &&
	    track >= 0 && track < int(getTrackCount())) {
		SetCursor(handCursor);
		return TRUE;
	}
	return DefWindowProc(this->hwnd, WM_SETCURSOR, (WPARAM)hwnd,
	    MAKELPARAM(hitTest, message));
}

LRESULT TrackView::onLButtonDown(UINT /*flags*/, POINTS pos)
{
	int track = getTrackFromX(pos.x);
	if (pos.y < topMarginHeight &&
	    track >= 0 && track < int(getTrackCount())) {
		setEditTrack(track, false);
		SetCapture(hwnd);
		anchorTrack = track;
	}
	return FALSE;
}

LRESULT TrackView::onLButtonUp(UINT /*flags*/, POINTS /*pos*/)
{
	ReleaseCapture();
	setEditTrack(editTrack);
	return FALSE;
}

LRESULT TrackView::onMouseMove(UINT /*flags*/, POINTS pos)
{
	if (GetCapture() == hwnd) {
		SyncDocument *doc = getDocument();
		const int posTrack = getTrackFromX(pos.x),
		    trackCount = getTrackCount();

		if (!doc || posTrack < 0 || posTrack >= trackCount)
			return FALSE;

		if (posTrack > anchorTrack) {
			for (int i = anchorTrack; i < posTrack; ++i)
				doc->swapTrackOrder(i, i + 1);
			anchorTrack = posTrack;
			setEditTrack(posTrack);
			InvalidateRect(hwnd, NULL, FALSE);
		} else if (posTrack < anchorTrack) {
			for (int i = anchorTrack; i > posTrack; --i)
				doc->swapTrackOrder(i, i - 1);
			anchorTrack = posTrack;
			setEditTrack(posTrack);
			InvalidateRect(hwnd, NULL, FALSE);
		}
	}
	return FALSE;
}

LRESULT TrackView::windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	assert(hwnd == this->hwnd);
	
	switch(msg)
	{
	case WM_CREATE:  return onCreate();
	case WM_SIZE:    return onSize(LOWORD(lParam), HIWORD(lParam));
	case WM_VSCROLL: return onVScroll(LOWORD(wParam), getScrollPos(hwnd, SB_VERT));
	case WM_HSCROLL: return onHScroll(LOWORD(wParam), getScrollPos(hwnd, SB_HORZ));
	case WM_PAINT:   return onPaint();
	case WM_KEYDOWN: return onKeyDown((UINT)wParam, (UINT)lParam);
	case WM_CHAR:    return onChar((UINT)wParam, (UINT)lParam);
	case WM_LBUTTONDOWN:
		return onLButtonDown((UINT)wParam, MAKEPOINTS(lParam));
	case WM_LBUTTONUP:
		return onLButtonUp((UINT)wParam, MAKEPOINTS(lParam));
	case WM_MOUSEMOVE:
		return onMouseMove((UINT)wParam, MAKEPOINTS(lParam));
	case WM_SETCURSOR:
		return onSetCursor((HWND)wParam, LOWORD(lParam),
		    HIWORD(lParam));

	case WM_COPY:  editCopy();  break;
	case WM_CUT:   editCut();   break;
	case WM_PASTE:
		editPaste();
		SendMessage(GetParent(getWin()), WM_CURRVALDIRTY, 0, 0);
		InvalidateRect(hwnd, NULL, FALSE);
		break;
	
	case WM_UNDO:
		if (NULL == getDocument()) return FALSE;
		if (!getDocument()->undo())
			MessageBeep(~0U);
		// unfortunately, we don't know how much to invalidate... so we'll just invalidate it all.
		InvalidateRect(hwnd, NULL, FALSE);
		break;
	
	case WM_REDO:
		if (NULL == getDocument()) return FALSE;
		if (!getDocument()->redo())
			MessageBeep(~0U);
		// unfortunately, we don't know how much to invalidate... so we'll just invalidate it all.
		InvalidateRect(hwnd, NULL, FALSE);
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
		trackViewWindowClassName, "",
		WS_VSCROLL | WS_HSCROLL | WS_CHILD | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, // x, y
		CW_USEDEFAULT, CW_USEDEFAULT, // width, height
		hwndParent, NULL, hInstance, (void*)this
	);
	return hwnd;
}

