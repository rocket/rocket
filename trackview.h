#pragma once

class TrackView
{
public:
	// events
	void onCreate(HWND hwnd);
	void onPaint(HWND hwnd);
	void onVScroll(HWND hwnd, UINT sbCode, int newPos);
	void onHScroll(HWND hwnd, UINT sbCode, int newPos);
	void onSize(HWND hwnd, int width, int height);
	void onKeyDown(HWND hwnd, UINT keyCode, UINT flags);
	
	// the window procedure
	LRESULT windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	void paintTracks(HDC hdc, RECT rcTracks);
	
	void setupScrollBars(HWND hwnd);
	void setScrollPos(HWND hwnd, int newScrollPosX, int newScrollPosY);
	void setEditLine(HWND hwnd, int newEditLine);
	void scrollWindow(HWND hwnd, int newScrollPosX, int newScrollPosY);

	int getScreenY(int line);
	
	int editLine;
	int scrollPosX, scrollPosY;
	int windowWidth, windowHeight;
	int windowLines;
};

ATOM registerTrackViewWindowClass(HINSTANCE hInstance);
HWND createTrackViewWindow(HINSTANCE hInstance, HWND hwndParent);
