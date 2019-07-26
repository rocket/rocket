#include <QApplication>
#include <QMessageBox>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	app.setOrganizationName("GNU Rocket Foundation");
	app.setApplicationName("GNU Rocket Editor");
	app.setWindowIcon(QIcon(":appicon.ico"));

	MainWindow mainWindow;

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 0))
	qSetGlobalQHashSeed(0);
#endif

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
