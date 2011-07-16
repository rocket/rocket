/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in COPYING
 */

#pragma once

#include "syncdocument.h"
#include <string>

// custom messages
#define WM_REDO         (WM_USER + 0x40 + 3)
#define WM_ROWCHANGED   (WM_USER + 0x40 + 4)
#define WM_TRACKCHANGED (WM_USER + 0x40 + 5)
#define WM_CURRVALDIRTY (WM_USER + 0x40 + 6)

class TrackView
{
public:
	TrackView();
	~TrackView();
	
	HWND create(HINSTANCE hInstance, HWND hwndParent);
	HWND getWin() const { return hwnd; }

	void setDocument(SyncDocument *document)
	{
		this->document = document;
		this->setupScrollBars();
	}

	const SyncDocument *getDocument() const { return document; }
	SyncDocument *getDocument() { return document; }
	
	void setRows(size_t rows);
	size_t getRows() const
	{
		const SyncDocument *doc = getDocument();
		if (!doc)
			return 0;
		return doc->getRows();
	}
	
	void setFont(HFONT font);
	
	void editEnterValue();
	void editDelete();
	void editCopy();
	void editCut();
	void editPaste();
	void editBiasValue(float amount);
	void editToggleInterpolationType();
	
	void setEditRow(int newEditRow);
	int  getEditRow() const { return editRow; }
	
	void setEditTrack(int newEditTrack, bool autoscroll = true);
	int  getEditTrack() const { return editTrack; }
	
	void selectAll()
	{
		selectStartTrack = int(this->getTrackCount()) - 1;
		selectStopTrack = editTrack = 0;
		selectStartRow = int(this->getRows()) - 1;
		selectStopRow = editRow = 0;
		
		InvalidateRect(hwnd, NULL, FALSE);
	}
	
	void selectTrack(int track)
	{
		selectStartTrack = selectStopTrack = editTrack = track;
		selectStartRow = int(this->getRows()) - 1;
		selectStopRow = editRow = 0;
		
		InvalidateRect(hwnd, NULL, FALSE);
	}
	
	void selectRow(int row)
	{
		selectStartTrack = int(this->getTrackCount()) - 1;
		selectStopTrack = editTrack = 0;
		selectStartRow = selectStopRow = editRow = row;
		
		InvalidateRect(hwnd, NULL, FALSE);
	}
	
	void selectNone()
	{
		selectStartTrack = selectStopTrack = editTrack;
		selectStartRow = selectStopRow = editRow;
		InvalidateRect(hwnd, NULL, FALSE);
	}
	
private:
	// some nasty hackery to forward the window messages
	friend LRESULT CALLBACK trackViewWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	
	// events
	LRESULT onCreate();
	LRESULT onPaint();
	LRESULT onVScroll(UINT sbCode, int newPos);
	LRESULT onHScroll(UINT sbCode, int newPos);
	LRESULT onSize(int width, int height);
	LRESULT onKeyDown(UINT keyCode, UINT flags);
	LRESULT onChar(UINT keyCode, UINT flags);
	LRESULT onSetCursor(HWND win, UINT hitTest, UINT message);
	LRESULT onLButtonDown(UINT flags, POINTS pos);
	LRESULT onLButtonUp(UINT flags, POINTS pos);
	LRESULT onMouseMove(UINT flags, POINTS pos);

	void paintTracks(HDC hdc, RECT rcTracks);
	void paintTopMargin(HDC hdc, RECT rcTracks);
	
	void setupScrollBars();
	void setScrollPos(int newScrollPosX, int newScrollPosY);
	void scrollWindow(int newScrollPosX, int newScrollPosY);
	
	void invalidateRange(int startTrack, int stopTrack, int startRow, int stopRow)
	{
		RECT rect;
		rect.left  = getScreenX(std::min(startTrack, stopTrack));
		rect.right = getScreenX(std::max(startTrack, stopTrack) + 1);
		rect.top    = getScreenY(std::min(startRow, stopRow));
		rect.bottom = getScreenY(std::max(startRow, stopRow) + 1);
		InvalidateRect(hwnd, &rect, FALSE);
	}
	
	void invalidatePos(int track, int row)
	{
		RECT rect;
		rect.left  = getScreenX(track);
		rect.right = getScreenX(track + 1);
		rect.top    = getScreenY(row);
		rect.bottom = getScreenY(row + 1);
		InvalidateRect(hwnd, &rect, FALSE);
	}
	
	void invalidateRow(int row)
	{
		RECT clientRect;
		GetClientRect(hwnd, &clientRect);
		
		RECT rect;
		rect.left  = clientRect.left;
		rect.right =  clientRect.right;
		rect.top    = getScreenY(row);
		rect.bottom = getScreenY(row + 1);
		
		InvalidateRect(hwnd, &rect, FALSE);
	}
	
	void invalidateTrack(int track)
	{
		RECT clientRect;
		GetClientRect(hwnd, &clientRect);
		
		RECT rect;
		rect.left  = getScreenX(track);
		rect.right = getScreenX(track + 1);
		rect.top    = clientRect.top;
		rect.bottom = clientRect.bottom;
		
		InvalidateRect(hwnd, &rect, FALSE);
	}
	
	int getScreenY(int row) const;
	int getScreenX(size_t track) const;
	int getTrackFromX(int x) const;

	size_t getTrackCount() const
	{
		const SyncDocument *doc = getDocument();
		if (NULL == doc) return 0;
		return int(doc->getTrackOrderCount());
	};
	
	int selectStartTrack, selectStopTrack;
	int selectStartRow, selectStopRow;
	
	HFONT font;
	int rowHeight;
	int fontWidth;
	int trackWidth;
	int topMarginHeight;
	int leftMarginWidth;

	
	HBRUSH bgBaseBrush, bgDarkBrush;
	HBRUSH selectBaseBrush, selectDarkBrush;
	HPEN rowPen, rowSelectPen;
	HBRUSH editBrush;
	HPEN lerpPen, cosinePen, rampPen;
	HCURSOR handCursor;
	
	/* cursor position */
	int editRow, editTrack;
	
	int scrollPosX,  scrollPosY;
	int windowWidth, windowHeight;
	int windowRows,  windowTracks;
	
	SyncDocument *document;
	
	std::string editString;
	
	HWND hwnd;
	
	UINT clipboardFormat;
	int anchorTrack;
};

ATOM registerTrackViewWindowClass(HINSTANCE hInstance);
