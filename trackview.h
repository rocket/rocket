#pragma once

class TrackView
{
public:
	TrackView(HWND hwnd);
	~TrackView();

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
	
	/* cursor position */
	int editLine, editTrack;

	int scrollPosX, scrollPosY;
	int windowWidth, windowHeight;
	int windowLines, windowTracks;
	
	HWND hwnd;
	HBRUSH editBrush;
};

ATOM registerTrackViewWindowClass(HINSTANCE hInstance);
HWND createTrackViewWindow(HINSTANCE hInstance, HWND hwndParent);
