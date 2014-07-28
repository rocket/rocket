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

	if (app.arguments().size() > 1) {
		if (app.arguments().size() > 2) {
			QMessageBox::critical(&mainWindow, NULL, QString("usage: %1 [filename.rocket]").arg(argv[0]), QMessageBox::Ok);
			exit(EXIT_FAILURE);
		}
		mainWindow.loadDocument(app.arguments()[1]);
	} else
		mainWindow.fileNew();
	
	mainWindow.show();
	app.exec();

	return 0;
}
