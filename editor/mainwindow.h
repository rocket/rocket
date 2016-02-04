#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include <QStringList>
#include "synctrack.h"
#include "clientsocket.h"

class QLabel;
class QAction;
class QTcpServer;
class QWebSocketServer;

class SyncDocument;
class TrackView;
class ClientSocket;

class MainWindow : public QMainWindow {
	Q_OBJECT

public:
	MainWindow();
	void showEvent(QShowEvent *event);
	void keyPressEvent(QKeyEvent *event);

	void createMenuBar();
	void createStatusBar();
	void updateRecentFiles();
	QStringList getRecentFiles() const;
	void setRecentFiles(const QStringList &files);
	void setCurrentFileName(const QString &fileName);
	bool loadDocument(const QString &path);
	void setDocument(SyncDocument *newDoc);

	void setStatusPosition(int row, int col);
	void setStatusText(const QString &text);
	void setStatusValue(double val, bool valid);
	void setStatusKeyType(const SyncTrack::TrackKey::KeyType keyType);

	QSettings settings;
	QTcpServer *tcpServer;
	QWebSocketServer *wsServer;

	ClientSocket *clientSocket;

	SyncDocument *doc;

	TrackView *trackView;

	QLabel *statusPos, *statusValue, *statusKeyType;
	QMenu *fileMenu, *recentFilesMenu, *editMenu;
	QAction *recentFileActions[5];

private:
	void setPaused(bool pause);

public slots:
	void fileNew();
	void fileOpen();
	void fileSave();
	void fileSaveAs();
	void fileRemoteExport();
	void openRecentFile();
	void fileQuit();

	void editBiasSelection();

	void editUndo();
	void editRedo();
	void editCopy();
	void editCut();
	void editPaste();
	void editClear();
	void editSelectAll();
	void editSelectTrack();
	void editSelectRow();

	void editSetRows();
	void editSetFont();

	void editPreviousBookmark();
	void editNextBookmark();

	void onPosChanged(int col, int row);
	void onCurrValDirty();

private slots:
	void onTrackRequested(const QString &trackName);
	void onRowChanged(int row);
	void onNewTcpConnection();
#ifdef QT_WEBSOCKETS_LIB
	void onNewWsConnection();
#endif
	void onConnected();
	void onDisconnected();
};

#endif // MAINWINDOW_H
