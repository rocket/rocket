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
			HINSTANCE hInstance = GetModuleHandle(NULL);
			trackViewWin = trackView->create(hInstance, hwnd);
			InitCommonControls();
			statusBarWin = CreateWindowEx(
				0,                               // no extended styles
				STATUSCLASSNAME,                 // status bar
				(LPCTSTR)NULL,                   // no text 
				SBARS_SIZEGRIP | WS_VISIBLE | WS_CHILD, // styles
				0, 0, 0, 0,                      // x, y, cx, cy
				hwnd,                            // parent window
				NULL,                            // menu
				hInstance,                       // instance
				NULL                             // window data
			);
			
			int statwidths[] = { 100, -1 };
			SendMessage(statusBarWin, SB_SETPARTS, sizeof(statwidths) / sizeof(int), (LPARAM)statwidths);
			SendMessage(statusBarWin, SB_SETTEXT, 0, (LPARAM)"Hi there :)");
			SendMessage(statusBarWin, SB_SETTEXT, 1, (LPARAM)"Hi there :)");
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
			case ID_FILE_SAVE: /* meh.*/ break;
			case ID_EDIT_UNDO:  SendMessage(trackViewWin, WM_UNDO,  0, 0); break;
			case ID_EDIT_REDO:  SendMessage(trackViewWin, WM_REDO,  0, 0); break;
			case ID_EDIT_COPY:  SendMessage(trackViewWin, WM_COPY,  0, 0); break;
			case ID_EDIT_CUT:   SendMessage(trackViewWin, WM_CUT,   0, 0); break;
			case ID_EDIT_PASTE: SendMessage(trackViewWin, WM_PASTE, 0, 0); break;
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
	wc.lpszMenuName  = (LPCSTR)IDR_MENU;
	wc.lpszClassName = mainWindowClassName;
	wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
	
	return RegisterClassEx(&wc);
}

#include "network.h"

int _tmain(int argc, _TCHAR* argv[])
{
	HWND hwnd;
	HINSTANCE hInstance = GetModuleHandle(NULL);

#if 1
	if (false == initNetwork())
	{
		fputs("Failed to init WinSock", stderr);
		exit(1);
	}

	SOCKET serverSocket = socket( AF_INET, SOCK_STREAM, 0 );

	struct sockaddr_in sin;
	memset( &sin, 0, sizeof sin );

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons( 1338 );

	puts("binding...");
	if (SOCKET_ERROR == bind( serverSocket, (struct sockaddr *)&sin, sizeof(sin)))
	{
		fputs("Coult not start server", stderr);
		exit(1);
	}

	puts("listening...");
	while ( listen( serverSocket, SOMAXCONN ) == SOCKET_ERROR );

/*	ULONG nonBlock = 1;
	if (ioctlsocket(serverSocket, FIONBIO, &nonBlock) == SOCKET_ERROR)
	{
		printf("ioctlsocket() failed\n");
		return 0;
	} */
#endif

	SyncData syncData;
	SyncTrack &camXTrack = syncData.getTrack(_T("cam.x"));
	SyncTrack &camYTrack = syncData.getTrack(_T("cam.y"));
	SyncTrack &camZTrack = syncData.getTrack(_T("cam.z"));
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
	
	HACCEL accel = LoadAccelerators(hInstance, (LPCSTR)IDR_ACCELERATOR);
	printf("accel: %p\n", accel);
	
	ShowWindow(hwnd, TRUE);
	UpdateWindow(hwnd);

#if 1

	bool done = false;
	SOCKET clientSocket = INVALID_SOCKET;
	MSG msg;
	while (!done)
	{
		if (INVALID_SOCKET == clientSocket)
		{
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(serverSocket, &fds);
			struct timeval timeout;
			timeout.tv_sec = 0;
			timeout.tv_usec = 0;

			// look for new clients
			if (select(0, &fds, NULL, NULL, &timeout) > 0)
			{
				puts("accepting...");
				clientSocket = clientConnect(serverSocket);
				if (INVALID_SOCKET != clientSocket)
				{
					unsigned char cmd = 0x1;
					send(clientSocket, (char*)&cmd, 1, 0);
				}
			}
		}
		else
		{
			unsigned char cmd = 0x1;
			send(clientSocket, (char*)&cmd, 1, 0);
			
			// look for new commands
			while (pollRead(clientSocket))
			{
				unsigned char cmd = 0;
				int ret = recv(clientSocket, (char*)&cmd, 1, 0);
				if (1 != ret)
				{
					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
					break;
				}
				else
				{
					printf("cmd: %02x\n", cmd);
					if (cmd == 1) printf("yes, master!\n");
				}
			}

			// terminate connection
/*			cmd = 0x0;
			send(clientSocket, (char*)&cmd, 1, 0);
			closesocket(clientSocket);
			clientSocket = INVALID_SOCKET; */
		}

		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (!TranslateAccelerator(hwnd, accel, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				if (WM_QUIT == msg.message) done = true;
			}
		}
	}
	
	closesocket(serverSocket);
	closeNetwork();

#else
	// Step 3: The Message Loop
	while(GetMessage(&msg, NULL, 0, 0) > 0)
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
#endif
	
	delete trackView;
	trackView = NULL;
	
	UnregisterClass(mainWindowClassName, hInstance);
	return int(msg.wParam);
}