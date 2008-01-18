// synctracker2.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>
#include <commctrl.h>

#include "trackview.h"

const TCHAR *mainWindowClassName = _T("MainWindow");

TrackView *trackView;
HWND trackViewWin;
HWND statusBarWin;

static LRESULT CALLBACK mainWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
		case WM_CREATE:
		{
			trackViewWin = trackView->create(GetModuleHandle(NULL), hwnd);
			
			InitCommonControls();
			statusBarWin = CreateWindowEx(
				0,                               // no extended styles
				STATUSCLASSNAME,                 // status bar
				(LPCTSTR)"mordi",                   // no text 
				SBARS_SIZEGRIP | WS_VISIBLE | WS_CHILD,       // styles
				0, 0, 0, 0,                      // x, y, cx, cy
				hwnd,                            // parent window
				(HMENU)100,                      // window ID
				GetModuleHandle(NULL),           // instance
				NULL                             // window data
			);
			
			int statwidths[] = {100, -1};
			SendMessage(statusBarWin, SB_SETPARTS, sizeof(statwidths)/sizeof(int), (LPARAM)statwidths);
			SendMessage(statusBarWin, SB_SETTEXT, 0, (LPARAM)"Hi there :)");
			
			HMENU fileMenu = CreatePopupMenu();
			AppendMenu(fileMenu, MF_STRING, 0, "&Open\tCtrl+O");
			AppendMenu(fileMenu, MF_STRING, 2, "&Save\tCtrl+S");
			AppendMenu(fileMenu, MF_STRING, 3, "Save &As");
			AppendMenu(fileMenu, MF_SEPARATOR, 0, NULL);
			AppendMenu(fileMenu, MF_STRING, 3, "&Exit");
			
			HMENU editMenu = CreatePopupMenu();
			AppendMenu(editMenu, MF_STRING, WM_UNDO, "&Undo\tCtrl+Z");
			AppendMenu(editMenu, MF_STRING, WM_REDO, "&Redo\tShift+Ctrl+Z");
			AppendMenu(editMenu, MF_SEPARATOR, 0, NULL);
			AppendMenu(editMenu, MF_STRING, WM_CUT,   "Cu&t\tCtrl+X");
			AppendMenu(editMenu, MF_STRING, WM_COPY,  "&Copy\tCtrl+C");
			AppendMenu(editMenu, MF_STRING, WM_PASTE, "&Paste\tCtrl+V");
			
			HMENU rootMenu = CreateMenu();
			AppendMenu(rootMenu, MF_STRING | MF_POPUP, (UINT_PTR)fileMenu, "&File");
			AppendMenu(rootMenu, MF_STRING | MF_POPUP, (UINT_PTR)editMenu, "&Edit");
			SetMenu(hwnd, rootMenu);
		}
		break;
		
		case WM_SIZE:
		{
			int width  = LOWORD(lParam);
			int height = HIWORD(lParam);
			
			RECT statusBarRect;
			GetClientRect(statusBarWin, &statusBarRect);
			int statusBarHeight = statusBarRect.bottom - statusBarRect.top;
			
			MoveWindow(trackViewWin, 0, 0, width, height - statusBarHeight, TRUE);
			MoveWindow(statusBarWin, 0, height - statusBarHeight, width, statusBarHeight, TRUE);
		}
		break;
		
		case WM_SETFOCUS:
			SetFocus(trackViewWin); // needed to forward keyboard input
		break;
		
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case WM_COPY:
					/* PostMessage(m_hWnd, WM_COPY, 0, 0); */
					/* HMMMM.... not working... */
					printf("copy!\n");
				break;
				
				// simply forward these
				case WM_UNDO:
				case WM_REDO:
					SendMessage(trackViewWin, LOWORD(wParam), 0, 0);
				break;

				default:
					printf("cmd %d %d\n", wParam, lParam);
			}
		break;
		
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

static ATOM registerMainWindowClass(HINSTANCE hInstance)
{
	WNDCLASSEX wc;
	
	wc.cbSize        = sizeof(WNDCLASSEX);
	wc.style         = 0;
	wc.lpfnWndProc   = mainWindowProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)0;
	wc.lpszMenuName  = NULL;
	wc.lpszClassName = mainWindowClassName;
	wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
	
	return RegisterClassEx(&wc);
}


#include <winsock2.h>

int _tmain(int argc, _TCHAR* argv[])
{
	HWND hwnd;
	MSG Msg;
	HINSTANCE hInstance = GetModuleHandle(NULL);

#if 0
	WSADATA wsaData;
	int error = WSAStartup( MAKEWORD( 2, 0 ), &wsaData );
	if (0 != error)
	{
		fputs("Failed to init WinSock", stderr);
		exit(1);
	}

	if (LOBYTE( wsaData.wVersion ) != 2 || HIBYTE( wsaData.wVersion ) != 0)
	{
		fputs("Wrong version of WinSock", stderr);
		exit(1);
	}

	SOCKET server = socket( AF_INET, SOCK_STREAM, 0 );

	struct sockaddr_in sin;
	memset( &sin, 0, sizeof sin );

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons( 21 );

	if (SOCKET_ERROR == bind( server, (struct sockaddr *)&sin, sizeof(sin)))
	{
		fputs("Coult not start server", stderr);
		exit(1);
	}

	while ( listen( server, SOMAXCONN ) == SOCKET_ERROR );

	int length = sizeof(sin);
	SOCKET client = accept( server,  (sockaddr *)&sin, &length );

	const char *test = "TEST123";
	int sent = send( client, test, strlen(test), 0);
	if (SOCKET_ERROR == sent)
	{
		int err = WSAGetLastError();
		fprintf(stderr, "failed to send - err %d\n", err);
		exit(1);
	}
	

	unsigned long int nonBlock = 1;
	if (ioctlsocket(client, FIONBIO, &nonBlock) == SOCKET_ERROR)
	{
		printf("ioctlsocket() failed \n");
		exit(1);
	}

	int read = 0;
	while (read < 20)
	{
		char buffer[1024];
		int len = recv(client, buffer, 1024, 0);
		if (0 < len)
		{
			printf("got \"%s\" - left: %d\n", buffer, 20 - (read + len));
			read += len;
		}
	}

	closesocket(client);
	closesocket(server);
	WSACleanup();
#endif

	SyncData syncData;
	SyncTrack &camXTrack = syncData.getTrack("cam.x");
	SyncTrack &camYTrack = syncData.getTrack("cam.y");
	SyncTrack &camZTrack = syncData.getTrack("cam.z");
/*	for (int i = 0; i < 1 << 16; ++i)
	{
		char temp[256];
		sprintf(temp, "gen %02d", i);
		SyncTrack &temp2 = syncData.getTrack(temp);
	} */

	camXTrack.setKeyFrame(1, SyncTrack::KeyFrame(2.0f));
	camXTrack.setKeyFrame(4, SyncTrack::KeyFrame(3.0f));

	camYTrack.setKeyFrame(0, SyncTrack::KeyFrame(100.0f));
	camYTrack.setKeyFrame(8, SyncTrack::KeyFrame(999.0f));

	camYTrack.setKeyFrame(16, SyncTrack::KeyFrame(float(1E-5)));

	for (int i = 0; i < 5 * 2; ++i)
	{
		float time = float(i) / 2;
		printf("%f %d - %f\n", time, camXTrack.isKeyFrame(i), camXTrack.getValue(time));
	}
	
	ATOM mainClass      = registerMainWindowClass(hInstance);
	ATOM trackViewClass = registerTrackViewWindowClass(hInstance);
	if(!mainClass || ! trackViewClass)
	{
		MessageBox(NULL, _T("Window Registration Failed!"), _T("Error!"), MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}
	
	trackView = new TrackView();
	trackView->setSyncData(&syncData);
	
	// Step 2: Creating the Window
	hwnd = CreateWindowEx(
		0,
		mainWindowClassName,
		_T("SyncTracker 3000"),
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		CW_USEDEFAULT, CW_USEDEFAULT, // x, y
		CW_USEDEFAULT, CW_USEDEFAULT, // width, height
		NULL, NULL, hInstance, NULL
	);
	
	if (NULL == hwnd)
	{
		MessageBox(NULL, _T("Window Creation Failed!"), _T("Error!"), MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}
	
	ShowWindow(hwnd, TRUE);
	UpdateWindow(hwnd);

#if 0
/*	while (1)
	{
		SOCKET client;
		int length;
		length = sizeof sin;
		client = accept( server,  (sockaddr *)&sin, &length );
	} */
#else
	// Step 3: The Message Loop
	while(GetMessage(&Msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&Msg);
		DispatchMessage(&Msg);
	}
#endif
	
	delete trackView;
	trackView = NULL;
	
	UnregisterClass(mainWindowClassName, hInstance);
	return int(Msg.wParam);
}