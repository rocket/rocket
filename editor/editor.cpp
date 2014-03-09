/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in COPYING
 */

extern "C" {
#include "../lib/base.h"
}

#include <QApplication>
#include <QMessageBox>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	app.setOrganizationName("GNU Rocket Foundation");
	app.setApplicationName("GNU Rocket Editor");
	app.setWindowIcon(QIcon(":appicon.ico"));

#ifdef WIN32
	WSADATA wsa;
	if (0 != WSAStartup(MAKEWORD(2, 0), &wsa)) {
		QMessageBox::critical(NULL, NULL, "failed to init network", QMessageBox::Ok);
		exit(EXIT_FAILURE);
	}
#endif

	SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in sin;
	memset(&sin, 0, sizeof sin);

	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(1338);

	if (bind(serverSocket, (struct sockaddr *)&sin,
	    sizeof(sin)) < 0) {
		QMessageBox::critical(NULL, NULL, "Could not start server", QMessageBox::Ok);
		exit(EXIT_FAILURE);
	}

	while (listen(serverSocket, SOMAXCONN) < 0)
		; /* nothing */

	MainWindow mainWindow(serverSocket);

	if (argc > 1) {
		if (argc > 2) {
			QMessageBox::critical(&mainWindow, NULL, QString("usage: %1 [filename.rocket]").arg(argv[0]), QMessageBox::Ok);
			exit(EXIT_FAILURE);
		}
		mainWindow.loadDocument(argv[1]);
	} else
		mainWindow.fileNew();
	
	mainWindow.show();
	app.exec();
	closesocket(serverSocket);

#ifdef WIN32
	WSACleanup();
#endif

	return 0;
}
