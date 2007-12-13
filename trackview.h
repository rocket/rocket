#pragma once

#include "syncdata.h"

class SyncData;

class TrackView
{
public:
	TrackView();
	~TrackView();

	HWND create(HINSTANCE hInstance, HWND hwndParent);
	HWND getWin(){ return hwnd; }

	void setSyncData(SyncData *syncData) { this->syncData = syncData; }
	SyncData *getSyncData() { return syncData; }

private:
	// some nasty hackery to forward the window messages
	friend static LRESULT CALLBACK trackViewWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	// events
	void onCreate();
	void onPaint();
	void onVScroll(UINT sbCode, int newPos);
	void onHScroll(UINT sbCode, int newPos);
	void onSize(int width, int height);
	void onKeyDown(UINT keyCode, UINT flags);
	
	// the window procedure

	void paintTracks(HDC hdc, RECT rcTracks);
	void paintTopMargin(HDC hdc, RECT rcTracks);
	
	void setupScrollBars();
	void setScrollPos(int newScrollPosX, int newScrollPosY);
	void scrollWindow(int newScrollPosX, int newScrollPosY);

	void setEditLine(int newEditLine);
	void setEditTrack(int newEditTrack);

	int getScreenY(int line);
	int getScreenX(int track);
	
	int getTrackCount()
	{
		SyncData *syncData = getSyncData();
		if (NULL == syncData) return 0;
		return int(syncData->getTrackCount());
	};

	/* cursor position */
	int editLine, editTrack;

	int scrollPosX, scrollPosY;
	int windowWidth, windowHeight;
	int windowLines, windowTracks;
	
	SyncData *syncData;
	
	HWND hwnd;
	HBRUSH editBrush;
};

ATOM registerTrackViewWindowClass(HINSTANCE hInstance);
