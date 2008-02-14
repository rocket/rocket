// synctracker2.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>
#include <commctrl.h>

#include "trackview.h"
#include <vector>
const TCHAR *mainWindowClassName = _T("MainWindow");

TrackView *trackView;
HWND trackViewWin;
HWND statusBarWin;

#define WM_SETROWS (WM_USER+1)
#define WM_BIASSELECTION (WM_USER+2)

#include "network.h"

static LRESULT CALLBACK setRowsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
/*	case WM_CHAR:
		printf("char: %d %d\n", wParam, lParam);
		break; */
	
	case WM_INITDIALOG:
		{
			int *rows = (int*)lParam;
			assert(NULL != rows);
			
			/* create row-string */
			TCHAR temp[256];
			_sntprintf_s(temp, 256, _T("%d"), *rows);
			
			/* set initial row count */
			SetDlgItemText(hDlg, IDC_SETROWS_EDIT, temp);
			return TRUE;
		}
		break;
	
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK)
		{
			/* get value */
			TCHAR temp[256];
			GetDlgItemText(hDlg, IDC_SETROWS_EDIT, temp, 256);
			int result = _tstoi(temp);
			
			/* update editor */
			SendMessage(GetParent(hDlg), WM_SETROWS, 0, result);
			
			/* end dialog */
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		else if(LOWORD(wParam)== IDCANCEL)
		{
			EndDialog( hDlg, LOWORD(wParam));
			return TRUE;
		}
		break;
	
	case WM_CLOSE:
		EndDialog(hDlg, LOWORD(wParam));
		return TRUE;
	}
	
	return FALSE;
}

static LRESULT CALLBACK biasSelectionDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		{
			int *intialBias = (int*)lParam;
			assert(NULL != intialBias);
			
			/* create bias-string */
			TCHAR temp[256];
			_sntprintf_s(temp, 256, _T("%d"), *intialBias);
			
			/* set initial bias */
			SetDlgItemText(hDlg, IDC_SETROWS_EDIT, temp);
		}
		break;
	
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK)
		{
			/* get value */
			TCHAR temp[256];
			GetDlgItemText(hDlg, IDC_BIASSELECTION_EDIT, temp, 256);
			int bias = _tstoi(temp);
			
			/* update editor */
			SendMessage(GetParent(hDlg), WM_BIASSELECTION, 0, LPARAM(bias));
			
			/* end dialog */
			EndDialog(hDlg, LOWORD(wParam));
			return TRUE;
		}
		else if(LOWORD(wParam)== IDCANCEL)
		{
			EndDialog( hDlg, LOWORD(wParam));
		}
		break;
	
	case WM_CLOSE:
		EndDialog(hDlg, LOWORD(wParam));
		return TRUE;
	}
	
	return FALSE;
}

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
			SendMessage(statusBarWin, SB_SETTEXT, 0, (LPARAM)_T("Hi there :)"));
			SendMessage(statusBarWin, SB_SETTEXT, 1, (LPARAM)_T("Hi there :)"));
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
	
	case WM_SETROWS:
		printf("rows: %d\n", int(lParam));
		trackView->setRows(int(lParam));
		break;
	
	case WM_BIASSELECTION:
		trackView->editBiasValue(float(lParam));
		break;
	
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_FILE_SAVE:
		case ID_FILE_SAVE_AS:
		case ID_FILE_OPEN:
			MessageBox(trackViewWin, "Not implemented", NULL, MB_OK | MB_ICONERROR);
			break;
		
		case ID_FILE_EXIT:  PostQuitMessage(0); break;
		case ID_EDIT_UNDO:  SendMessage(trackViewWin, WM_UNDO,  0, 0); break;
		case ID_EDIT_REDO:  SendMessage(trackViewWin, WM_REDO,  0, 0); break;
		case ID_EDIT_COPY:  SendMessage(trackViewWin, WM_COPY,  0, 0); break;
		case ID_EDIT_CUT:   SendMessage(trackViewWin, WM_CUT,   0, 0); break;
		case ID_EDIT_PASTE: SendMessage(trackViewWin, WM_PASTE, 0, 0); break;
		
		case ID_EDIT_SETROWS:
			{
				HINSTANCE hInstance = GetModuleHandle(NULL);
				int rows = trackView->getRows();
				INT_PTR result = DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_SETROWS), hwnd, (DLGPROC)setRowsDialogProc, (LPARAM)&rows);
				if (FAILED(result)) MessageBox(NULL, _T("unable to create dialog box"), NULL, MB_OK);
			}
			break;
		
		case ID_EDIT_BIAS:
			{
				HINSTANCE hInstance = GetModuleHandle(NULL);
				int initialBias = 0;
				INT_PTR result = DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_BIASSELECTION), hwnd, (DLGPROC)biasSelectionDialogProc, (LPARAM)&initialBias);
				if (FAILED(result)) MessageBox(NULL, _T("unable to create dialog box"), NULL, MB_OK);
			}
			break;
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
	wc.lpszMenuName  = MAKEINTRESOURCE(IDR_MENU);
	wc.lpszClassName = mainWindowClassName;
	wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
	
	return RegisterClassEx(&wc);
}

int _tmain(int argc, _TCHAR* argv[])
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
/*	_CrtSetBreakAlloc(68); */
#endif
	
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
	
	SyncEditData syncData;
	syncData.clientSocket = INVALID_SOCKET;
#if 0
	
	SyncTrack &camXTrack = syncData.getTrack(_T("cam.x"));
	SyncTrack &camXTrack2 = syncData.getTrack(_T("cam.x"));
	camXTrack.setKeyFrame(1, 2.0f);
	camXTrack.setKeyFrame(4, 3.0f);
	printf("%p %p\n", &camXTrack, &camXTrack2);
	
	SyncTrack &camYTrack = syncData.getTrack(_T("cam.y"));
	SyncTrack &camZTrack = syncData.getTrack(_T("cam.z"));
	
/*	for (int i = 0; i < 16; ++i)
	{
		TCHAR temp[256];
		_sntprintf_s(temp, 256, _T("gen %02d"), i);
		SyncTrack &temp2 = syncData.getTrack(temp);
	} */
	
	camXTrack.setKeyFrame(1, 2.0f);
	camXTrack.setKeyFrame(4, 3.0f);
	
	camYTrack.setKeyFrame(0, 100.0f);
	camYTrack.setKeyFrame(8, 999.0f);
	
	camYTrack.setKeyFrame(16, SyncTrack::KeyFrame(float(1E-5)));
	
	for (int i = 0; i < 5 * 2; ++i)
	{
		float time = float(i) / 2;
		printf("%f %d - %f\n", time, camXTrack.isKeyFrame(i), camXTrack.getValue(time));
	}
#endif
	
	ATOM mainClass      = registerMainWindowClass(hInstance);
	ATOM trackViewClass = registerTrackViewWindowClass(hInstance);
	if (!mainClass || !trackViewClass)
	{
		MessageBox(NULL, _T("Window Registration Failed!"), _T("Error!"), MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}
	
	trackView = new TrackView();
	trackView->setSyncData(&syncData);
	
	HWND hwnd = CreateWindowEx(
		0,
		mainWindowClassName,
		_T("SyncTracker 3000"),
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		CW_USEDEFAULT, CW_USEDEFAULT, // x, y
		CW_USEDEFAULT, CW_USEDEFAULT, // width, height
		NULL, NULL, hInstance, NULL
	);
	printf("main window: %p\n", hwnd);
	
	if (NULL == hwnd)
	{
		MessageBox(NULL, _T("Window Creation Failed!"), _T("Error!"), MB_ICONEXCLAMATION | MB_OK);
		return 0;
	}
	
	HACCEL accel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR));
	printf("accel: %p\n", accel);
	
	ShowWindow(hwnd, TRUE);
	UpdateWindow(hwnd);
	
#if 1
	
	printf("server socket %x\n", serverSocket);
	
	bool done = false;
	SOCKET clientSocket = INVALID_SOCKET;
	MSG msg;
	while (!done)
	{
#if 1
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
					puts("connected.");
					syncData.clientSocket = clientSocket;
					syncData.clientRemap.clear();
				}
			}
		}
		
		if (INVALID_SOCKET != clientSocket)
		{
			// look for new commands
			while (pollRead(clientSocket))
			{
				unsigned char cmd = 0;
				int ret = recv(clientSocket, (char*)&cmd, 1, 0);
				if (1 > ret)
				{
					closesocket(clientSocket);
					clientSocket = INVALID_SOCKET;
					syncData.clientSocket = INVALID_SOCKET;
					syncData.clientRemap.clear();
					InvalidateRect(trackViewWin, NULL, FALSE);
					break;
				}
				else
				{
					switch (cmd)
					{
					case GET_TRACK:
						{
							size_t clientIndex = 0;
							int ret = recv(clientSocket, (char*)&clientIndex, sizeof(int), 0);
							printf("client index: %d\n", clientIndex);
							
							// get len
							int str_len = 0;
							ret = recv(clientSocket, (char*)&str_len, sizeof(int), 0);
							
	//						int clientAddr = 0;
	//						int ret = recv(clientSocket, (char*)&clientAddr, sizeof(int), 0);
							
							// get string
							std::string trackName;
							trackName.resize(str_len);
							recv(clientSocket, &trackName[0], str_len, 0);
							
							// find track
							size_t serverIndex = syncData.getTrackIndex(trackName.c_str());
							printf("name: \"%s\"\n", trackName.c_str());
							
							// setup remap
							syncData.clientRemap[serverIndex] = clientIndex;
							
							const sync::Track &track = *syncData.actualTracks[serverIndex];
							
							sync::Track::KeyFrameContainer::const_iterator it;
							for (it = track.keyFrames.begin(); it != track.keyFrames.end(); ++it)
							{
								int row = int(it->first);
								const sync::Track::KeyFrame &key = it->second;
								syncData.sendSetKeyCommand(int(serverIndex), row, key);
							}
							
							InvalidateRect(trackViewWin, NULL, FALSE);
						}
						break;
					
					case SET_ROW:
						{
							int newRow = 0;
							int ret = recv(clientSocket, (char*)&newRow, sizeof(int), 0);
							printf("new row: %d\n", newRow);
							trackView->setEditRow(newRow);
						}
						break;
					}
				}
			}
		}
#endif
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (!TranslateAccelerator(hwnd, accel, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				if (WM_QUIT == msg.message) done = true;
			}
		}
		Sleep(1);
	}
	
	closesocket(clientSocket);
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