#pragma once

#include "synceditdata.h"

#include <list>
#include <string>
#include <stack>

// custom messages
#define WM_REDO (WM_USER + 1)

class TrackView
{
public:
	TrackView();
	~TrackView();
	
	HWND create(HINSTANCE hInstance, HWND hwndParent);
	HWND getWin(){ return hwnd; }
	
	void setSyncData(SyncEditData *syncData) { this->syncData = syncData; }
	SyncEditData *getSyncData() { return syncData; }
	
private:
	// some nasty hackery to forward the window messages
	friend static LRESULT CALLBACK trackViewWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	
	// events
	LRESULT onCreate();
	LRESULT onPaint();
	LRESULT onVScroll(UINT sbCode, int newPos);
	LRESULT onHScroll(UINT sbCode, int newPos);
	LRESULT onSize(int width, int height);
	LRESULT onKeyDown(UINT keyCode, UINT flags);
	LRESULT onChar(UINT keyCode, UINT flags);
	
	void copy();
	void cut();
	void paste();
	
	// the window procedure
	
	void paintTracks(HDC hdc, RECT rcTracks);
	void paintTopMargin(HDC hdc, RECT rcTracks);
	
	void setupScrollBars();
	void setScrollPos(int newScrollPosX, int newScrollPosY);
	void scrollWindow(int newScrollPosX, int newScrollPosY);

	void invalidateRange(int startTrack, int stopTrack, int startRow, int stopRow)
	{
		RECT rect;
		rect.left  = getScreenX(min(startTrack, stopTrack));
		rect.right = getScreenX(max(startTrack, stopTrack) + 1);
		rect.top    = getScreenY(min(startRow, stopRow));
		rect.bottom = getScreenY(max(startRow, stopRow) + 1);
		InvalidateRect(hwnd, &rect, TRUE);
	}
	
	void invalidatePos(int track, int row)
	{
		RECT rect;
		rect.left  = getScreenX(track);
		rect.right = getScreenX(track + 1);
		rect.top    = getScreenY(row);
		rect.bottom = getScreenY(row + 1);
		InvalidateRect(hwnd, &rect, TRUE);
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
		
		InvalidateRect(hwnd, &rect, TRUE);
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
		
		InvalidateRect(hwnd, &rect, TRUE);
	}

	
	void setEditRow(int newEditRow);
	void setEditTrack(int newEditTrack);
	
	int getScreenY(int row);
	int getScreenX(int track);
	
	int getTrackCount()
	{
		SyncData *syncData = getSyncData();
		if (NULL == syncData) return 0;
		return int(syncData->getTrackCount());
	};
	
	bool selectActive;
	int selectStartTrack, selectStopTrack;
	int selectStartRow, selectStopRow;
	
	HBRUSH bgBaseBrush, bgDarkBrush;
	HBRUSH selectBaseBrush, selectDarkBrush;
	
	/* cursor position */
	int editRow, editTrack;
	
	int scrollPosX,  scrollPosY;
	int windowWidth, windowHeight;
	int windowRows,  windowTracks;
	
	SyncEditData *syncData;
	
	std::basic_string<TCHAR> editString;
	
	HWND hwnd;
	HBRUSH editBrush;

	UINT clipboardFormat;
};

ATOM registerTrackViewWindowClass(HINSTANCE hInstance);
