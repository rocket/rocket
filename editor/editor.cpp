/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in COPYING
 */

#include "../sync/base.h"
#include "afxres.h"
#include "resource.h"

#include <commctrl.h>
#include <objbase.h>
#include <commdlg.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdarg.h>

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
static const wchar_t *mainWindowClassName = L"MainWindow";
static const char    *mainWindowTitle  =  "GNU Rocket System";
static const wchar_t *mainWindowTitleW = L"GNU Rocket System";
static const char *keyName = "SOFTWARE\\GNU Rocket";

static void verror(const char *fmt, va_list va)
{
	char temp[4096];
	vsnprintf(temp, sizeof(temp), fmt, va);
	MessageBox(NULL, temp, mainWindowTitle, MB_OK | MB_ICONERROR);
}

static void error(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	verror(fmt, va);
	va_end(va);
}

static void die(const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	verror(fmt, va);
	va_end(va);
	exit(EXIT_FAILURE);
}

static HINSTANCE hInst;
static HWND hwnd = NULL;
static TrackView *trackView = NULL;
static HWND trackViewWin = NULL;
static HWND statusBarWin = NULL;
static HKEY regConfigKey = NULL;
static RecentFiles mruFileList(NULL);

#define WM_SETROWS (WM_USER+1)
#define WM_BIASSELECTION (WM_USER+2)

#include "../sync/sync.h"

static LRESULT CALLBACK setRowsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_INITDIALOG:
		{
			size_t *rows = (size_t *)lParam;
			SetDlgItemInt(hDlg, IDC_SETROWS_EDIT, *rows, FALSE);
			return TRUE;
		}
		break;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK) {
			/* get value */
			size_t result = GetDlgItemInt(hDlg, IDC_SETROWS_EDIT, NULL, FALSE);

			/* update editor */
			SendMessage(GetParent(hDlg), WM_SETROWS, 0, result);
			
			/* end dialog */
			return EndDialog(hDlg, LOWORD(wParam));
		} else if(LOWORD(wParam) == IDCANCEL)
			return EndDialog( hDlg, LOWORD(wParam));
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
			SetDlgItemInt(hDlg, IDC_BIASSELECTION_EDIT, *intialBias, TRUE);
			return TRUE;
		}
		break;
	
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK) {
			/* get value */
			int bias = GetDlgItemInt(hDlg, IDC_BIASSELECTION_EDIT, NULL, FALSE);

			/* update editor */
			SendMessage(GetParent(hDlg), WM_BIASSELECTION, 0, LPARAM(bias));
			
			/* end dialog */
			return EndDialog(hDlg, LOWORD(wParam));
		} else if(LOWORD(wParam) == IDCANCEL)
			return EndDialog( hDlg, LOWORD(wParam));
		break;
	
	case WM_CLOSE:
		EndDialog(hDlg, LOWORD(wParam));
		return TRUE;
	}
	
	return FALSE;
}

static void setWindowFileName(std::wstring fileName)
{
	wchar_t drive[_MAX_DRIVE],dir[_MAX_DIR],fname[_MAX_FNAME],ext[_MAX_EXT];
	_wsplitpath(fileName.c_str(), drive, dir, fname, ext);
	std::wstring windowTitle = std::wstring(fname) + std::wstring(L" - ") + std::wstring(mainWindowTitleW);
	SetWindowTextW(hwnd, windowTitle.c_str());
}

static HMENU findSubMenuContaining(HMENU menu, UINT id)
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

static void setDocument(SyncDocument *newDoc)
{
	SyncDocument *oldDoc = trackView->getDocument();

	if (oldDoc && oldDoc->clientSocket.connected()) {
		// delete old key-frames
		for (size_t i = 0; i < oldDoc->num_tracks; ++i) {
			sync_track *t = oldDoc->tracks[i];
			for (int j = 0; j < t->num_keys; ++j)
				oldDoc->clientSocket.sendDeleteKeyCommand(t->name, t->keys[j].row);
		}

		if (newDoc) {
			// add back missing client-tracks
			std::map<const std::string, size_t>::const_iterator it;
			for (it = oldDoc->clientSocket.clientTracks.begin(); it != oldDoc->clientSocket.clientTracks.end(); ++it) {
				int trackIndex = sync_find_track(newDoc, it->first.c_str());
				if (0 > trackIndex)
					trackIndex = int(newDoc->createTrack(it->first.c_str()));
			}

			// copy socket and update client
			newDoc->clientSocket = oldDoc->clientSocket;

			for (size_t i = 0; i < newDoc->num_tracks; ++i) {
				sync_track *t = newDoc->tracks[i];
				for (int j = 0; j < t->num_keys; ++j)
					newDoc->clientSocket.sendSetKeyCommand(t->name, t->keys[j]);
			}
		}
	}

	trackView->setDocument(newDoc);
	SendMessage(hwnd, WM_CURRVALDIRTY, 0, 0);
	InvalidateRect(trackViewWin, NULL, FALSE);

	if (oldDoc)
		delete oldDoc;
}

static void fileNew()
{
	setDocument(new SyncDocument);
	setWindowFileName(L"Untitled");
}

static void loadDocument(const std::wstring &_fileName)
{
	SyncDocument *newDoc = SyncDocument::load(_fileName);
	if (newDoc) {
		// update MRU list
		mruFileList.insert(_fileName);
		mruFileList.update();
		DrawMenuBar(hwnd);

		// set new document
		setDocument(newDoc);
		setWindowFileName(_fileName.c_str());
	}
	else
		error("failed to open file");
}

static void fileOpen()
{
	wchar_t temp[_MAX_FNAME + 1];
	temp[0] = L'\0'; // clear string

	OPENFILENAMEW ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFile = temp;
	ofn.nMaxFile = _MAX_FNAME;
	ofn.lpstrDefExt = L"rocket";
	ofn.lpstrFilter = L"ROCKET File (*.rocket)\0*.rocket\0All Files (*.*)\0*.*\0\0";
	ofn.Flags = OFN_SHOWHELP | OFN_FILEMUSTEXIST;
	if (GetOpenFileNameW(&ofn))
	{
		loadDocument(temp);
	}
}

static bool fileSaveAs()
{
	wchar_t temp[_MAX_FNAME + 1];
	temp[0] = '\0';
	
	OPENFILENAMEW ofn;
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFile = temp;
	ofn.nMaxFile = _MAX_FNAME;
	ofn.lpstrDefExt = L"rocket";
	ofn.lpstrFilter = L"ROCKET File (*.rocket)\0*.rocket\0All Files (*.*)\0*.*\0\0";
	ofn.Flags = OFN_SHOWHELP | OFN_OVERWRITEPROMPT;
	
	if (GetSaveFileNameW(&ofn)) {
		SyncDocument *doc = trackView->getDocument();
		if (doc->save(temp)) {
			doc->clientSocket.sendSaveCommand();
			setWindowFileName(temp);
			doc->fileName = temp;

			mruFileList.insert(temp);
			mruFileList.update();
			DrawMenuBar(hwnd);
			return true;
		} else
			error("Failed to save file");
	}
	return false;
}

static bool fileSave()
{
	SyncDocument *doc = trackView->getDocument();
	if (doc->fileName.empty())
		return fileSaveAs();

	if (!doc->save(doc->fileName)) {
		doc->clientSocket.sendSaveCommand();
		error("Failed to save file");
		return false;
	}
	return true;
}

static void attemptQuit()
{
	SyncDocument *doc = trackView->getDocument();
	if (doc->modified()) {
		UINT res = MessageBox(hwnd, "Save before exit?", mainWindowTitle, MB_YESNOCANCEL | MB_ICONQUESTION);
		if ((IDYES == res && fileSave()) || (IDNO == res))
			DestroyWindow(hwnd);
	}
	else DestroyWindow(hwnd);
}

static HWND createStatusBar(HINSTANCE hInstance, HWND hpwnd)
{
	HWND hwnd = CreateWindowEx(
		0,                               // no extended styles
		STATUSCLASSNAME,                 // status bar
		(LPCTSTR)NULL,                   // no text
		SBARS_SIZEGRIP | WS_VISIBLE | WS_CHILD, // styles
		0, 0, 0, 0,                      // x, y, cx, cy
		hpwnd,                           // parent window
		NULL,                            // menu
		hInstance,                       // instance
		NULL                             // window data
	);

	int statwidths[] = { 150, 150 + 32, 150 + 32 * 2, 150 + 32 * 4, 150 + 32 * 6};
	SendMessage(hwnd, SB_SETPARTS, sizeof(statwidths) / sizeof(int), (LPARAM)statwidths);
	SendMessage(hwnd, SB_SETTEXT, 0, (LPARAM)"Not connected");
	SendMessage(hwnd, SB_SETTEXT, 1, (LPARAM)"0");
	SendMessage(hwnd, SB_SETTEXT, 2, (LPARAM)"0");
	SendMessage(hwnd, SB_SETTEXT, 3, (LPARAM)"---");
	return hwnd;
}

static LRESULT CALLBACK mainWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	SyncDocument *doc = trackView ? trackView->getDocument() : NULL;
	switch(msg)
	{
	case WM_CREATE:
		{
			trackViewWin = trackView->create(hInst, hwnd);
			InitCommonControls();
			statusBarWin = createStatusBar(hInst, hwnd);

			if (ERROR_SUCCESS != RegOpenKey(HKEY_CURRENT_USER, keyName, &regConfigKey))
			{
				if (ERROR_SUCCESS != RegCreateKey(HKEY_CURRENT_USER, keyName, &regConfigKey))
					die("failed to create registry key");
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
			doc->clientSocket.sendSaveCommand();
			break;
		
		case ID_RECENTFILES_FILE1:
		case ID_RECENTFILES_FILE2:
		case ID_RECENTFILES_FILE3:
		case ID_RECENTFILES_FILE4:
		case ID_RECENTFILES_FILE5:
			{
				int index = LOWORD(wParam) - ID_RECENTFILES_FILE1;
				std::wstring fileName;
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

		case ID_EDIT_BOOKMARK_PREV:
			{
				int row = doc->prevRowBookmark(trackView->getEditRow());
				if (row >= 0)
					trackView->setEditRow(row);
			}
			break;

		case ID_EDIT_BOOKMARK_NEXT:
			{
				int row = doc->nextRowBookmark(trackView->getEditRow());
				if (row >= 0)
					trackView->setEditRow(row);
			}
			break;


		case ID_EDIT_SETROWS:
			{
				size_t rows = trackView->getRows();
				INT_PTR result = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_SETROWS), hwnd, (DLGPROC)setRowsDialogProc, (LPARAM)&rows);
				if (FAILED(result))
					error("unable to create dialog box");
			}
			break;
		
		case ID_EDIT_BIAS:
			{
				int initialBias = 0;
				INT_PTR result = DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_BIASSELECTION), hwnd, (DLGPROC)biasSelectionDialogProc, (LPARAM)&initialBias);
				if (FAILED(result))
					error("unable to create dialog box");
			}
			break;
		}
		break;
	
	case WM_ROWCHANGED:
		{
			char temp[256];
			snprintf(temp, 256, "%d", lParam );
			SendMessage(statusBarWin, SB_SETTEXT, 1, (LPARAM)temp);
		}
		break;
	
	case WM_TRACKCHANGED:
		{
			char temp[256];
			snprintf(temp, 256, "%d", lParam);
			SendMessage(statusBarWin, SB_SETTEXT, 2, (LPARAM)temp);
		}
		break;
	
	case WM_CURRVALDIRTY:
		{
			char temp[256];
			if (doc->num_tracks > 0) {
				const sync_track *t = doc->tracks[doc->getTrackIndexFromPos(trackView->getEditTrack())];
				int row = trackView->getEditRow();
				int idx = key_idx_floor(t, row);
				snprintf(temp, 256, "%f", sync_get_val(t, row));
				const char *str = "---";
				if (idx >= 0) {
					switch (t->keys[idx].type) {
					case KEY_STEP:   str = "step"; break;
					case KEY_LINEAR: str = "linear"; break;
					case KEY_SMOOTH: str = "smooth"; break;
					case KEY_RAMP:   str = "ramp"; break;
					}
				}
				SendMessage(statusBarWin, SB_SETTEXT, 4, (LPARAM)str);
			} else
				snprintf(temp, 256, "---");
			SendMessage(statusBarWin, SB_SETTEXT, 3, (LPARAM)temp);
		}
		break;
	
	default:
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}
	return 0;
}

static ATOM registerMainWindowClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wc;

	wc.cbSize        = sizeof(wc);
	wc.style         = 0;
	wc.lpfnWndProc   = mainWindowProc;
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = hInstance;
	wc.hIcon         = LoadIcon(hInstance, IDI_APPLICATION);
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)0;
	wc.lpszMenuName  = MAKEINTRESOURCEW(IDR_MENU);
	wc.lpszClassName = mainWindowClassName;
	wc.hIconSm       = wc.hIcon;

	return RegisterClassExW(&wc);
}

static SOCKET clientConnect(SOCKET serverSocket, sockaddr_in *host)
{
	sockaddr_in hostTemp;
	int hostSize = sizeof(sockaddr_in);
	SOCKET clientSocket = accept(serverSocket, (sockaddr*)&hostTemp, &hostSize);
	if (INVALID_SOCKET == clientSocket) return INVALID_SOCKET;

	const char *expectedGreeting = CLIENT_GREET;
	char recievedGreeting[128];

	recv(clientSocket, recievedGreeting, int(strlen(expectedGreeting)), 0);

	if (strncmp(expectedGreeting, recievedGreeting, strlen(expectedGreeting)) != 0)
	{
		closesocket(clientSocket);
		return INVALID_SOCKET;
	}

	const char *greeting = SERVER_GREET;
	send(clientSocket, greeting, int(strlen(greeting)), 0);

	if (NULL != host) *host = hostTemp;
	return clientSocket;
}

static size_t clientIndex;
static void processCommand(ClientSocket &sock)
{
	SyncDocument *doc = trackView->getDocument();
	int strLen, serverIndex, newRow;
	std::string trackName;
	const sync_track *t;
	unsigned char cmd = 0;
	if (sock.recv((char*)&cmd, 1, 0)) {
		switch (cmd) {
		case GET_TRACK:
			// read data
			sock.recv((char *)&strLen, sizeof(int), 0);
			strLen = ntohl(strLen);
			if (!sock.connected())
				return;

			trackName.resize(strLen);
			if (!sock.recv(&trackName[0], strLen, 0))
				return;

			// find track
			serverIndex = sync_find_track(doc,
			    trackName.c_str());
			if (0 > serverIndex)
				serverIndex =
				    int(doc->createTrack(trackName));

			// setup remap
			doc->clientSocket.clientTracks[trackName] = clientIndex++;

			// send key-frames
			t = doc->tracks[serverIndex];
			for (int i = 0; i < (int)t->num_keys; ++i)
				doc->clientSocket.sendSetKeyCommand(trackName,
				    t->keys[i]);

			InvalidateRect(trackViewWin, NULL, FALSE);
			break;

		case SET_ROW:
			sock.recv((char*)&newRow, sizeof(int), 0);
			trackView->setEditRow(ntohl(newRow));
			break;
		}
	}
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/,
    LPSTR /*lpCmdLine*/, int /*nShowCmd*/)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
/*	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_DEBUG);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_DEBUG); */
//	_CrtSetBreakAlloc(254);
#endif
	
	hInst = hInstance;
	CoInitialize(NULL);

	WSADATA wsa;
	if (0 != WSAStartup(MAKEWORD(2, 0), &wsa))
		die("Failed to init network");

	SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in sin;
	memset(&sin, 0, sizeof sin);

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(1338);

	if (SOCKET_ERROR == bind(serverSocket, (struct sockaddr *)&sin,
	    sizeof(sin)))
		die("Could not start server");

	while (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
		; /* nothing */

	ATOM mainClass      = registerMainWindowClass(hInstance);
	ATOM trackViewClass = registerTrackViewWindowClass(hInstance);
	if (!mainClass || !trackViewClass)
		die("Window Registration Failed!");

	trackView = new TrackView();

	hwnd = CreateWindowExW(
		0,
		mainWindowClassName,
		mainWindowTitleW,
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		CW_USEDEFAULT, CW_USEDEFAULT, // x, y
		CW_USEDEFAULT, CW_USEDEFAULT, // width, height
		NULL, NULL, hInstance, NULL
	);

	if (NULL == hwnd)
		die("Window Creation Failed!");

	int argc;
	LPWSTR *argv = argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argv && argc > 1) {
		if (argc > 2) {
			char prog[MAX_PATH];
			GetModuleFileNameA(NULL, prog, sizeof(prog));
			die("usage: %s [filename.rocket]", prog);
		}
		loadDocument(argv[1]);
	} else
		fileNew();
	
	HACCEL accel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR));
	
	ShowWindow(hwnd, TRUE);
	UpdateWindow(hwnd);
	
	bool done = false;
	MSG msg;
	bool guiConnected = false;
	while (!done) {
		SyncDocument *doc = trackView->getDocument();
		if (!doc->clientSocket.connected()) {
			SOCKET clientSocket = INVALID_SOCKET;
			fd_set fds;
			FD_ZERO(&fds);
#pragma warning(suppress: 4127)
			FD_SET(serverSocket, &fds);
			struct timeval timeout;
			timeout.tv_sec = 0;
			timeout.tv_usec = 0;
			
			// look for new clients
			if (select(0, &fds, NULL, NULL, &timeout) > 0)
			{
				SendMessage(statusBarWin, SB_SETTEXT, 0, (LPARAM)"Accepting...");
				sockaddr_in client;
				clientSocket = clientConnect(serverSocket, &client);
				if (INVALID_SOCKET != clientSocket)
				{
					char temp[256];
					snprintf(temp, 256, "Connected to %s", inet_ntoa(client.sin_addr));
					SendMessage(statusBarWin, SB_SETTEXT, 0, (LPARAM)temp);
					doc->clientSocket = ClientSocket(clientSocket);
					clientIndex = 0;
					doc->clientSocket.sendPauseCommand(true);
					doc->clientSocket.sendSetRowCommand(trackView->getEditRow());
					guiConnected = true;
				}
				else SendMessage(statusBarWin, SB_SETTEXT, 0, (LPARAM)"Not Connected.");
			}
		}

		if (doc->clientSocket.connected()) {
			ClientSocket &clientSocket = doc->clientSocket;

			// look for new commands
			while (clientSocket.pollRead())
				processCommand(clientSocket);
		}

		if (!doc->clientSocket.connected() && guiConnected) {
			doc->clientSocket.clientPaused = true;
			InvalidateRect(trackViewWin, NULL, FALSE);
			SendMessage(statusBarWin, SB_SETTEXT, 0, (LPARAM)"Not Connected.");
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

	UnregisterClassW(mainWindowClassName, hInstance);
	return int(msg.wParam);
}
