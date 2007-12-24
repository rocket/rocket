#pragma once

#include "syncdata.h"

#include <string>
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
	void onChar(UINT keyCode, UINT flags);
	
	// the window procedure

	void paintTracks(HDC hdc, RECT rcTracks);
	void paintTopMargin(HDC hdc, RECT rcTracks);
	
	void setupScrollBars();
	void setScrollPos(int newScrollPosX, int newScrollPosY);
	void scrollWindow(int newScrollPosX, int newScrollPosY);

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

	/* cursor position */
	int editRow, editTrack;

	int scrollPosX,  scrollPosY;
	int windowWidth, windowHeight;
	int windowRows,  windowTracks;
	
	SyncData *syncData;

	std::string editString;
	
	HWND hwnd;
	HBRUSH editBrush;
};

ATOM registerTrackViewWindowClass(HINSTANCE hInstance);
