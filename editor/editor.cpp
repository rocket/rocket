/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in COPYING
 */

#include <QApplication>
#include <QMessageBox>
#include <QTcpServer>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	app.setOrganizationName("GNU Rocket Foundation");
	app.setApplicationName("GNU Rocket Editor");
	app.setWindowIcon(QIcon(":appicon.ico"));

	QTcpServer serverSocket;
	if (!serverSocket.listen(QHostAddress::Any, 1338)) {
		QMessageBox::critical(NULL, NULL, QString("Could not start server:\n%1").arg(serverSocket.errorString()), QMessageBox::Ok);
		exit(EXIT_FAILURE);
	}

	MainWindow mainWindow(&serverSocket);

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

	return 0;
}
