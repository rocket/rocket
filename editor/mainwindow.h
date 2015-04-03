#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "synctrack.h"
#include "clientsocket.h"

class QLabel;
class QAction;
class QTcpServer;

class SyncDocument;
class TrackView;
class ClientSocket;

class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	MainWindow(QTcpServer *serverSocket);
	void showEvent(QShowEvent *event);

	void createMenuBar();
	void createStatusBar();
	void updateRecentFiles();
	void setCurrentFileName(const QString &fileName);
	bool loadDocument(const QString &path);
	void setDocument(SyncDocument *newDoc);

	void processCommand(ClientSocket &sock);
	void processGetTrack(ClientSocket &sock);
	void processSetRow(ClientSocket &sock);

	void setStatusPosition(int row, int col);
	void setStatusText(const QString &text);
	void setStatusValue(double val, bool valid);
	void setStatusKeyType(SyncTrack::TrackKey::KeyType keyType, bool valid);

	QTcpServer *serverSocket;
	ClientSocket clientSocket;
	size_t clientIndex;

	TrackView *trackView;
	QLabel *statusPos, *statusValue, *statusKeyType;
	QMenu *fileMenu, *recentFilesMenu, *editMenu;
	QAction *recentFileActions[5];

public slots:
	void fileNew();
	void fileOpen();
	void fileSave();
	void fileSaveAs();
	void fileRemoteExport();
	void openRecentFile();
	void fileQuit();

	void editBiasSelection();

	void editSetRows();

	void editPreviousBookmark();
	void editNextBookmark();

	void onPosChanged(int col, int row);
	void onCurrValDirty();

private slots:
	void onReadyRead();
	void onNewConnection();
	void onDisconnected();
};

#endif // MAINWINDOW_H
