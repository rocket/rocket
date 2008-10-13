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

#include "trackview.h"
#include <vector>
const TCHAR *mainWindowClassName = _T("MainWindow");
const TCHAR *keyName = _T("SOFTWARE\\GNU Rocket");

HWND hwnd = NULL;
TrackView *trackView = NULL;
HWND trackViewWin = NULL;
HWND statusBarWin = NULL;
SyncDocument document;
HKEY regConfigKey = NULL;

#define WM_SETROWS (WM_USER+1)
#define WM_BIASSELECTION (WM_USER+2)

#include "../sync/network.h"

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
	std::string windowTitle = std::string("GNU Rocket System - ") + fileName;
	SetWindowText(hwnd, windowTitle.c_str());
}

bool setRegString(HKEY key, const std::string &name, const std::string &value)
{
	return ERROR_SUCCESS == RegSetValueEx(key, name.c_str(), 0, REG_SZ, (BYTE *)value.c_str(), (DWORD)value.size());
}

bool getRegString(HKEY key, const std::string &name, std::string &out)
{
	DWORD size = 0;
	DWORD type = 0;
	if (ERROR_SUCCESS != RegQueryValueEx(key, name.c_str(), 0, &type, (LPBYTE)NULL, &size)) return false;
	if (REG_SZ != type) return false;
	
	out.resize(size);
	DWORD ret = RegQueryValueEx(key, name.c_str(), 0, &type, (LPBYTE)&out[0], &size);
	while (out.size() > 0 && out[out.size() - 1] == '\0') out.resize(out.size() - 1);
	
	assert(ret == ERROR_SUCCESS);
	assert(REG_SZ == type);
	assert(size == out.size() + 1);

	return true;
}

std::list<std::string> mruList;

std::string getMruEntryName(size_t i)
{
	std::string temp = std::string("RecentFile");
	temp += char('0' + i);
	return temp;
}

void initMruList()
{
	for (size_t i = 0; i < 5; ++i)
	{
		std::string fileName;
		if (getRegString(regConfigKey, getMruEntryName(i), fileName))
		{
			mruList.push_back(fileName);
		}
	}
}

void saveMruList()
{
	std::list<std::string>::const_iterator it;
	size_t i;
	for (i = 0, it = mruList.begin(); it != mruList.end(); ++it, ++i)
	{
		assert(i <= 5);
		setRegString(regConfigKey, getMruEntryName(i), *it);
	}
}

void mruListInsert(const std::string fileName)
{
	mruList.remove(fileName); // remove, if present
	mruList.push_front(fileName); // add to front
	while (mruList.size() > 5) mruList.pop_back(); // remove old entries
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

HMENU mruFileMenu;

void mruListUpdate()
{
	while (0 != RemoveMenu(mruFileMenu, 0, MF_BYPOSITION));
	
	std::list<std::string>::const_iterator it;
	size_t i;
	for (i = 0, it = mruList.begin(); it != mruList.end(); ++it, ++i)
	{
		assert(i <= 5);
		AppendMenu(mruFileMenu, MF_STRING, ID_RECENTFILES_FILE1 + i, it->c_str());
	}
	DrawMenuBar(hwnd);
}

std::string fileName;

void fileNew()
{
/*	document.purgeUnusedTracks(); */
	for (size_t i = 0; i < document.getTrackCount(); ++i)
	{
		document.getTrack(i).truncate();
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
		
		mruListInsert(_fileName);
		mruListUpdate();
		
		document.clearUndoStack();
		document.clearRedoStack();
		
		InvalidateRect(trackViewWin, NULL, FALSE);
	}
	else MessageBox(trackViewWin, _T("failed to open file"), NULL, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
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
			setWindowFileName(temp);
			fileName = temp;
		}
		else MessageBox(trackViewWin, _T("Failed to save file"), NULL, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
	}
}

void fileSave()
{
	if (fileName.empty()) fileSaveAs();
	else if (!document.save(fileName.c_str()))
	{
		MessageBox(trackViewWin, _T("Failed to save file"), NULL, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
	}
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
			mruFileMenu = findSubMenuContaining(GetMenu(hwnd), ID_RECENTFILES_NORECENTFILES);
			initMruList();
			if (mruList.size() > 0) mruListUpdate();
		}
		break;
	
	case WM_DESTROY:
		saveMruList();
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
				std::list<std::string>::const_iterator it;
				int i;
				for (i = 0, it = mruList.begin(); it != mruList.end(); ++it, ++i)
				{
					if (i == index)
					{
						loadDocument(*it);
						break;
					}
				}
			}
			break;
		
		case ID_FILE_EXIT:
			DestroyWindow(hwnd);
			break;
		
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
				if (FAILED(result)) MessageBox(NULL, _T("unable to create dialog box"), NULL, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
			}
			break;
		
		case ID_EDIT_BIAS:
			{
				HINSTANCE hInstance = GetModuleHandle(NULL);
				int initialBias = 0;
				INT_PTR result = DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_BIASSELECTION), hwnd, (DLGPROC)biasSelectionDialogProc, (LPARAM)&initialBias);
				if (FAILED(result)) MessageBox(NULL, _T("unable to create dialog box"), NULL, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
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
			if (document.getTrackCount() > 0) _sntprintf_s(temp, 256, _T("%f"), document.getTrack(document.getTrackIndexFromPos(trackView->getEditTrack())).getValue(float(trackView->getEditRow())));
			else _sntprintf_s(temp, 256, _T("---"));
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

int _tmain(int argc, _TCHAR* argv[])
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG);
//	_CrtSetBreakAlloc(254);
#endif
	
	HINSTANCE hInstance = GetModuleHandle(NULL);
	CoInitialize(NULL);
	
#if 0
	{
		DWORD test = 0xdeadbeef;
		setRegString(key, "test", "hallaballa!");
		RegSetValueEx(key, "test2", 0, REG_DWORD, (BYTE *)&test, sizeof(DWORD));
		
		DWORD type = 0;
		DWORD test2 = 0;
		DWORD size = sizeof(DWORD);
		RegQueryValueEx(key, "test2", 0, &type, (LPBYTE)&test2, &size);
		assert(REG_DWORD == type);
		printf("%x\n", test2);
		
		std::string string;
		if (getRegString(key, "test", string))
			printf("\"%s\"\n", string.c_str());
	}
#endif
	
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
		MessageBox(NULL, "Could not start server", NULL, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
		fputs("Coult not start server", stderr);
		return -1;
	}
	
	puts("listening...");
	while ( listen( serverSocket, SOMAXCONN ) == SOCKET_ERROR );
	
	ATOM mainClass      = registerMainWindowClass(hInstance);
	ATOM trackViewClass = registerTrackViewWindowClass(hInstance);
	if (!mainClass || !trackViewClass)
	{
		MessageBox(NULL, _T("Window Registration Failed!"), _T("Error!"), MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
		return -1;
	}
	
	trackView = new TrackView();
	trackView->setDocument(&document);
	
	hwnd = CreateWindowEx(
		0,
		mainWindowClassName,
		_T("GNU Rocket System"),
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		CW_USEDEFAULT, CW_USEDEFAULT, // x, y
		CW_USEDEFAULT, CW_USEDEFAULT, // width, height
		NULL, NULL, hInstance, NULL
	);
	
	if (NULL == hwnd)
	{
		MessageBox(NULL, _T("Window Creation Failed!"), NULL, MB_OK | MB_ICONEXCLAMATION | MB_SETFOREGROUND);
		return -1;
	}
	
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
							int serverIndex = document.getTrackIndex(trackName);
							if (0 > serverIndex)
							{
								serverIndex = int(document.createTrack(trackName));
								document.trackOrder.push_back(serverIndex);
							}
							
							// setup remap
							document.clientRemap[serverIndex] = clientIndex;
							
							const sync::Track &track = document.getTrack(serverIndex);
							
							sync::Track::KeyFrameContainer::const_iterator it;
							for (it = track.keyFrames.begin(); it != track.keyFrames.end(); ++it)
							{
								int row = int(it->first);
								const sync::Track::KeyFrame &key = it->second;
								document.sendSetKeyCommand(int(serverIndex), row, key);
							}
							
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
	closeNetwork();
	
	delete trackView;
	trackView = NULL;
	
	UnregisterClass(mainWindowClassName, hInstance);
	return int(msg.wParam);
}