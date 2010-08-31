#include "mainwindow.h"

#include <QtGui/QMenuBar>
#include <QtGui/QStatusBar>

#include "trackview.h"

MainWindow::MainWindow(QWidget *parent, Qt::WFlags flags)
	: QMainWindow(parent, flags)
{
	ui.setupUi(this);

	trackView = new TrackView(this);
	setCentralWidget(trackView);
}

MainWindow::~MainWindow()
{

}
