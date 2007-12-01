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

	void paintTracks(HDC hdc, RECT rcTracks);

	void setupScrollBars(HWND hwnd);
	void setScrollPos(HWND hwnd, int newScrollPosX, int newScrollPosY);


	int tracks;
	int lines;
	
	int topMarginHeight, leftMarginWidth;
	int fontHeight, fontWidth;
	int scrollPosX, scrollPosY;

	int windowWidth, windowHeight;
	int windowLines;
};
