/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#include <afxres.h>
#include "resource.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <objbase.h>
#include <commdlg.h>

// Windows XP look and feel. Seems to enable Vista look as well.
#pragma comment(linker, \
	"\"/manifestdependency:type='Win32' " \
	"name='Microsoft.Windows.Common-Controls' " \
	"version='6.0.0.0' " \
	"processorArchitecture='*' " \
	"publicKeyToken='6595b64144ccf1df' " \
	"language='*'\"")

#include "trackview.h"
#include "recentfiles.h"

#include <vector>
const TCHAR *mainWindowClassName = _T("MainWindow");
const TCHAR *mainWindowTitle = _T("GNU Rocket System");
const TCHAR *keyName = _T("SOFTWARE\\GNU Rocket");

HWND hwnd = NULL;
TrackView *trackView = NULL;
HWND trackViewWin = NULL;
HWND statusBarWin = NULL;
SyncDocument document;
HKEY regConfigKey = NULL;
RecentFiles mruFileList(NULL);

#define WM_SETROWS (WM_USER+1)
#define WM_BIASSELECTION (WM_USER+2)

#include "../sync/sync.h"

static LRESULT CALLBACK setRowsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
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

void setWindowFileName(std::string fileName)
{
	TCHAR drive[_MAX_DRIVE],dir[_MAX_DIR],fname[_MAX_FNAME],ext[_MAX_EXT];
	_tsplitpath(fileName.c_str(), drive, dir, fname, ext);
	std::string windowTitle = std::string(fname) + std::string(" - ") + std::string(mainWindowTitle);
	SetWindowText(hwnd, windowTitle.c_str());
}

HMENU findSubMenuContaining(HMENU menu, UINT id)
{
	for (int i = 0; i < GetMenuItemCount(menu); ++i)
	{
		if (GetMenuItemID(menu, i) == id) return menu;
		else
		{
			HMENU subMenu = GetSubMenu(menu, i);
			if ((HMENU)0 != subMenu)
			{
				HMENU ret = findSubMenuContaining(subMenu, id);
				if ((HMENU)0 != ret) return ret;
			}
		}
	}
	return (HMENU)0;
}

std::string fileName;

void fileNew()
{
	// document.purgeUnusedTracks();
	for (size_t i = 0; i < document.num_tracks; ++i)
	{
		sync_track *t = document.tracks[i];
		free(t->keys);
		t->keys = NULL;
		t->num_keys = 0;
	}
	setWindowFileName("Untitled");
	fileName.clear();
	
	document.clearUndoStack();
	document.clearRedoStack();
}


void loadDocument(const std::string &_fileName)
{
	fileNew();
	if (document.load(_fileName))
	{
		setWindowFileName(_fileName.c_str());
		fileName = _fileName;
		
		mruFileList.insert(_fileName);
		mruFileList.update();
		DrawMenuBar(hwnd);
		
		document.clearUndoStack();
		document.clearRedoStack();
		
		SendMessage(hwnd, WM_CURRVALDIRTY, 0, 0);
		InvalidateRect(trackViewWin, NULL, FALSE);
	}
	else MessageBox(hwnd, _T("failed to open file"), mainWindowTitle, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
}

void fileOpen()
{
	char temp[_MAX_FNAME + 1];
	temp[0] = '\0'; // clear string
	
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFile = temp;
	ofn.nMaxFile = _MAX_FNAME;
	ofn.lpstrDefExt = "rocket";
	ofn.lpstrFilter = "ROCKET File (*.rocket)\0*.rocket\0All Files (*.*)\0*.*\0\0";
	ofn.Flags = OFN_SHOWHELP | OFN_FILEMUSTEXIST;
	if (GetOpenFileName(&ofn))
	{
		loadDocument(temp);
	}
}

void fileSaveAs()
{
	char temp[_MAX_FNAME + 1];
	temp[0] = '\0';
	
	OPENFILENAME ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFile = temp;
	ofn.nMaxFile = _MAX_FNAME;
	ofn.lpstrDefExt = "rocket";
	ofn.lpstrFilter = "ROCKET File (*.rocket)\0*.rocket\0All Files (*.*)\0*.*\0\0";
	ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;
	
	if (GetSaveFileName(&ofn))
	{
		if (document.save(temp))
		{
			document.sendSaveCommand();
			setWindowFileName(temp);
			fileName = temp;
			
			mruFileList.insert(temp);
			mruFileList.update();
			DrawMenuBar(hwnd);
		}
		else MessageBox(hwnd, _T("Failed to save file"), mainWindowTitle, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
	}
}

void fileSave()
{
	if (fileName.empty()) fileSaveAs();
	else if (!document.save(fileName.c_str()))
	{
		document.sendSaveCommand();
		MessageBox(hwnd, _T("Failed to save file"), mainWindowTitle, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
	}
}

void attemptQuit()
{
	if (document.modified())
	{
		UINT res = MessageBox(hwnd, _T("Save before exit?"), mainWindowTitle, MB_YESNOCANCEL | MB_ICONQUESTION);
		if (IDYES == res) fileSave();
		if (IDCANCEL != res) DestroyWindow(hwnd);
	}
	else DestroyWindow(hwnd);
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
			
			int statwidths[] = { 150, 150 + 32, 150 + 32 * 2, 150 + 32 * 4};
			SendMessage(statusBarWin, SB_SETPARTS, sizeof(statwidths) / sizeof(int), (LPARAM)statwidths);
			SendMessage(statusBarWin, SB_SETTEXT, 0, (LPARAM)_T("Not connected"));
			SendMessage(statusBarWin, SB_SETTEXT, 1, (LPARAM)_T("0"));
			SendMessage(statusBarWin, SB_SETTEXT, 2, (LPARAM)_T("0"));
			SendMessage(statusBarWin, SB_SETTEXT, 3, (LPARAM)_T("---"));
			
			if (ERROR_SUCCESS != RegOpenKey(HKEY_CURRENT_USER, keyName, &regConfigKey))
			{
				if (ERROR_SUCCESS != RegCreateKey(HKEY_CURRENT_USER, keyName, &regConfigKey))
				{
					printf("failed to create reg key\n");
					exit(-1);
				}
			}
			
			/* Recent Files menu */
			mruFileList = RecentFiles(findSubMenuContaining(GetMenu(hwnd), ID_RECENTFILES_NORECENTFILES));
			mruFileList.load(regConfigKey);
		}
		break;
	
	case WM_CLOSE:
		attemptQuit();
		break;
	
	case WM_DESTROY:
		mruFileList.save(regConfigKey);
		RegCloseKey(regConfigKey);
		regConfigKey = NULL;
		PostQuitMessage(0);
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
		trackView->setRows(int(lParam));
		break;
	
	case WM_BIASSELECTION:
		trackView->editBiasValue(float(lParam));
		break;
	
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case ID_FILE_NEW:
			fileNew();
			InvalidateRect(trackViewWin, NULL, FALSE);
			break;
			
		case ID_FILE_OPEN:
			fileOpen();
			break;
		
		case ID_FILE_SAVE_AS:
			fileSaveAs();
			break;
		
		case ID_FILE_SAVE:
			fileSave();
			break;
		
		case ID_EDIT_SELECTALL:
			trackView->selectAll();
			break;
		
		case ID_EDIT_SELECTTRACK:
			trackView->selectTrack(trackView->getEditTrack());
			break;
		
		case ID_EDIT_SELECTROW:
			trackView->selectRow(trackView->getEditRow());
			break;
		
		case ID_FILE_REMOTEEXPORT:
			document.sendSaveCommand();
			break;
		
		case ID_RECENTFILES_FILE1:
		case ID_RECENTFILES_FILE2:
		case ID_RECENTFILES_FILE3:
		case ID_RECENTFILES_FILE4:
		case ID_RECENTFILES_FILE5:
			{
				int index = LOWORD(wParam) - ID_RECENTFILES_FILE1;
				std::string fileName;
				if (mruFileList.getEntry(index, fileName))
				{
					loadDocument(fileName);
				}
			}
			break;
		
		case ID_FILE_EXIT:
			attemptQuit();
			break;
		
		case ID_EDIT_UNDO:  SendMessage(trackViewWin, WM_UNDO,  0, 0); break;
		case ID_EDIT_REDO:  SendMessage(trackViewWin, WM_REDO,  0, 0); break;
		case ID_EDIT_COPY:  SendMessage(trackViewWin, WM_COPY,  0, 0); break;
		case ID_EDIT_CUT:   SendMessage(trackViewWin, WM_CUT,   0, 0); break;
		case ID_EDIT_PASTE: SendMessage(trackViewWin, WM_PASTE, 0, 0); break;
		
		case ID_EDIT_SETROWS:
			{
				HINSTANCE hInstance = GetModuleHandle(NULL);
				int rows = int(trackView->getRows());
				INT_PTR result = DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_SETROWS), hwnd, (DLGPROC)setRowsDialogProc, (LPARAM)&rows);
				if (FAILED(result)) MessageBox(hwnd, _T("unable to create dialog box"), mainWindowTitle, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
			}
			break;
		
		case ID_EDIT_BIAS:
			{
				HINSTANCE hInstance = GetModuleHandle(NULL);
				int initialBias = 0;
				INT_PTR result = DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_BIASSELECTION), hwnd, (DLGPROC)biasSelectionDialogProc, (LPARAM)&initialBias);
				if (FAILED(result)) MessageBox(hwnd, _T("unable to create dialog box"), mainWindowTitle, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
			}
			break;
		}
		break;
	
	case WM_ROWCHANGED:
		{
			TCHAR temp[256];
			_sntprintf_s(temp, 256, _T("%d"), lParam );
			SendMessage(statusBarWin, SB_SETTEXT, 1, (LPARAM)temp);
		}
		break;
	
	case WM_TRACKCHANGED:
		{
			TCHAR temp[256];
			_sntprintf_s(temp, 256, _T("%d"), lParam);
			SendMessage(statusBarWin, SB_SETTEXT, 2, (LPARAM)temp);
		}
		break;
	
	case WM_CURRVALDIRTY:
		{
			TCHAR temp[256];
			if (document.num_tracks > 0) {
				sync_track *t = document.tracks[document.getTrackIndexFromPos(trackView->getEditTrack())];
				float row = float(trackView->getEditRow());
				_sntprintf_s(temp, 256, _T("%f"), sync_get_val(t, row));
			} else
				_sntprintf_s(temp, 256, _T("---"));
			SendMessage(statusBarWin, SB_SETTEXT, 3, (LPARAM)temp);
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

#include <stdarg.h>
void die(const char *fmt, ...)
{
	char temp[4096];
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	vsnprintf(temp, sizeof(temp), fmt, va);
	va_end(va);

	MessageBox(NULL, temp, mainWindowTitle, MB_OK | MB_ICONERROR);
	exit(EXIT_FAILURE);
}

SOCKET clientConnect(SOCKET serverSocket, sockaddr_in *host)
{
	sockaddr_in hostTemp;
	int hostSize = sizeof(sockaddr_in);
	SOCKET clientSocket = accept(serverSocket, (sockaddr*)&hostTemp, &hostSize);
	if (INVALID_SOCKET == clientSocket) return INVALID_SOCKET;

	const char *expectedGreeting = client_greet;
	char recievedGreeting[128];

	recv(clientSocket, recievedGreeting, int(strlen(expectedGreeting)), 0);

	if (strncmp(expectedGreeting, recievedGreeting, strlen(expectedGreeting)) != 0)
	{
		closesocket(clientSocket);
		return INVALID_SOCKET;
	}

	const char *greeting = server_greet;
	send(clientSocket, greeting, int(strlen(greeting)), 0);

	if (NULL != host) *host = hostTemp;
	return clientSocket;
}

int _tmain(int argc, _TCHAR* argv[])
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
/*	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG); */
//	_CrtSetBreakAlloc(254);
#endif
	
	HINSTANCE hInstance = GetModuleHandle(NULL);
	CoInitialize(NULL);
	
#if 0
	{
		DWORD test = 0xdeadbeef;
		RegSetValueEx(key, "test2", 0, REG_DWORD, (BYTE *)&test, sizeof(DWORD));
		
		DWORD type = 0;
		DWORD test2 = 0;
		DWORD size = sizeof(DWORD);
		RegQueryValueEx(key, "test2", 0, &type, (LPBYTE)&test2, &size);
		assert(REG_DWORD == type);
		printf("%x\n", test2);
	}
#endif

	WSADATA wsa;
	if (0 != WSAStartup(MAKEWORD(2, 0), &wsa))
		die("Failed to init network");

	SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in sin;
	memset(&sin, 0, sizeof sin);

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons( 1338 );

	puts("binding...");
	if (SOCKET_ERROR == bind( serverSocket, (struct sockaddr *)&sin, sizeof(sin)))
		die("Could not start server");

	puts("listening...");
	while (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
		; /* nothing */

	ATOM mainClass      = registerMainWindowClass(hInstance);
	ATOM trackViewClass = registerTrackViewWindowClass(hInstance);
	if (!mainClass || !trackViewClass)
		die("Window Registration Failed!");

	trackView = new TrackView();
	trackView->setDocument(&document);
	
	hwnd = CreateWindowEx(
		0,
		mainWindowClassName,
		mainWindowTitle,
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		CW_USEDEFAULT, CW_USEDEFAULT, // x, y
		CW_USEDEFAULT, CW_USEDEFAULT, // width, height
		NULL, NULL, hInstance, NULL
	);

	if (NULL == hwnd)
		die("Window Creation Failed!");

	fileNew();
	
	HACCEL accel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR));
	
	ShowWindow(hwnd, TRUE);
	UpdateWindow(hwnd);
	
	bool done = false;
	MSG msg;
	bool guiConnected = false;
	while (!done)
	{
		if (!document.clientSocket.connected())
		{
			SOCKET clientSocket = INVALID_SOCKET;
			fd_set fds;
			FD_ZERO(&fds);
			FD_SET(serverSocket, &fds);
			struct timeval timeout;
			timeout.tv_sec = 0;
			timeout.tv_usec = 0;
			
			// look for new clients
			if (select(0, &fds, NULL, NULL, &timeout) > 0)
			{
				SendMessage(statusBarWin, SB_SETTEXT, 0, (LPARAM)_T("Accepting..."));
				sockaddr_in client;
				clientSocket = clientConnect(serverSocket, &client);
				if (INVALID_SOCKET != clientSocket)
				{
					TCHAR temp[256];
					_sntprintf_s(temp, 256, _T("Connected to %s"), inet_ntoa(client.sin_addr));
					SendMessage(statusBarWin, SB_SETTEXT, 0, (LPARAM)temp);
					document.clientSocket = NetworkSocket(clientSocket);
					document.clientRemap.clear();
					document.sendPauseCommand(true);
					document.sendSetRowCommand(trackView->getEditRow());
					guiConnected = true;
				}
				else SendMessage(statusBarWin, SB_SETTEXT, 0, (LPARAM)_T("Not Connected."));
			}
		}
		
		if (document.clientSocket.connected())
		{
			NetworkSocket &clientSocket = document.clientSocket;
			
			// look for new commands
			while (clientSocket.pollRead())
			{
				unsigned char cmd = 0;
				if (clientSocket.recv((char*)&cmd, 1, 0))
				{
					switch (cmd)
					{
					case GET_TRACK:
						{
							size_t clientIndex = 0;
							clientSocket.recv((char*)&clientIndex, sizeof(int), 0);
							
							// get len
							int str_len = 0;
							clientSocket.recv((char*)&str_len, sizeof(int), 0);
							
							// get string
							std::string trackName;
							trackName.resize(str_len);
							clientSocket.recv(&trackName[0], str_len, 0);
							
							// find track
							int serverIndex = sync_find_track(&document, trackName.c_str());
							if (0 > serverIndex) serverIndex = int(document.createTrack(trackName));
							
							// setup remap
							document.clientRemap[serverIndex] = clientIndex;

							const sync_track *t = document.tracks[serverIndex];
							for (int i = 0; i < (int)t->num_keys; ++i)
								document.sendSetKeyCommand(int(serverIndex), t->keys[i]);

							InvalidateRect(trackViewWin, NULL, FALSE);
						}
						break;
					
					case SET_ROW:
						{
							int newRow = 0;
							clientSocket.recv((char*)&newRow, sizeof(int), 0);
							trackView->setEditRow(newRow);
						}
						break;
					}
				}
			}
		}
		
		if (!document.clientSocket.connected() && guiConnected)
		{
			document.clientPaused = true;
			InvalidateRect(trackViewWin, NULL, FALSE);
			SendMessage(statusBarWin, SB_SETTEXT, 0, (LPARAM)_T("Not Connected."));
			guiConnected = false;
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
		Sleep(1);
	}

	closesocket(serverSocket);
	WSACleanup();

	delete trackView;
	trackView = NULL;

	UnregisterClass(mainWindowClassName, hInstance);
	return int(msg.wParam);
}