/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in COPYING
 */

#pragma once

#include <string>
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>

// custom messages
#define WM_REDO         (WM_USER + 0x40 + 3)
#define WM_POSCHANGED   (WM_USER + 0x40 + 4)
#define WM_CURRVALDIRTY (WM_USER + 0x40 + 5)

class SyncDocument;

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
	size_t getRows() const;

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
	
	void setEditTrack(int newEditTrack, bool autoscroll = true, bool selecting = false);
	int  getEditTrack() const { return editTrack; }

	void selectAll()
	{
		selectStartTrack = int(this->getTrackCount()) - 1;
		selectStopTrack = editTrack = 0;
		selectStartRow = int(this->getRows()) - 1;
		selectStopRow = editRow = 0;
		update();
	}
	
	void selectTrack(int track)
	{
		selectStartTrack = selectStopTrack = editTrack = track;
		selectStartRow = int(this->getRows()) - 1;
		selectStopRow = editRow = 0;
		update();
	}
	
	void selectRow(int row)
	{
		selectStartTrack = int(this->getTrackCount()) - 1;
		selectStopTrack = editTrack = 0;
		selectStartRow = selectStopRow = editRow = row;
		update();
	}
	
	void selectNone()
	{
		selectStartTrack = selectStopTrack = editTrack;
		selectStartRow = selectStopRow = editRow;
		update();
	}

	void update()
	{
		InvalidateRect(hwnd, NULL, FALSE);
	}

	void update(int left, int top, int right, int bottom)
	{
		RECT rect;
		rect.left = left;
		rect.top = top;
		rect.right = right;
		rect.bottom = bottom;
		InvalidateRect(hwnd, &rect, FALSE);
	}

	int width() const
	{
		RECT rect;
		GetClientRect(hwnd, &rect);
		return rect.right - rect.left;
	}

	int height() const
	{
		RECT rect;
		GetClientRect(hwnd, &rect);
		return rect.bottom - rect.top;
	}

	void dirtyCurrentValue()
	{
		SendMessage(GetParent(getWin()), WM_CURRVALDIRTY, 0, 0);
	}

	void dirtyPosition()
	{
		SendMessage(GetParent(getWin()), WM_POSCHANGED, 0, 0);
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
		update(getScreenX(std::min(startTrack, stopTrack)),
		       getScreenY(std::min(startRow, stopRow)),
		       getScreenX(std::max(startTrack, stopTrack) + 1),
		       getScreenY(std::max(startRow, stopRow) + 1));
	}

	void invalidatePos(int track, int row)
	{
		invalidateRange(track, track, row, row);
	}

	void invalidateRow(int row)
	{
		invalidateRange(0, getTrackCount(), row, row);
	}

	void invalidateTrack(int track)
	{
		invalidateRange(track, track, 0, getRows());
	}

	int getScreenY(int row) const;
	int getScreenX(size_t track) const;
	int getTrackFromX(int x) const;

	size_t getTrackCount() const;

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
	HBRUSH editBrush, bookmarkBrush;
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
