#pragma once

class TrackView
{
public:
	TrackView(HWND hwnd);

	// events
	void onCreate();
	void onPaint();
	void onVScroll(UINT sbCode, int newPos);
	void onHScroll(UINT sbCode, int newPos);
	void onSize(int width, int height);
	void onKeyDown(UINT keyCode, UINT flags);
	
	// the window procedure
	LRESULT windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	void paintTracks(HDC hdc, RECT rcTracks);
	
	void setupScrollBars();
	void setScrollPos(int newScrollPosX, int newScrollPosY);
	void setEditLine(int newEditLine);
	void scrollWindow(int newScrollPosX, int newScrollPosY);

	int getScreenY(int line);
	
	int editLine;
	int scrollPosX, scrollPosY;
	int windowWidth, windowHeight;
	int windowLines;
	
	HWND hwnd;
};

ATOM registerTrackViewWindowClass(HINSTANCE hInstance);
HWND createTrackViewWindow(HINSTANCE hInstance, HWND hwndParent);
