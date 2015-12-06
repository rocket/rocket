#include "mainwindow.h"
#include "trackview.h"
#include "syncdocument.h"

#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QFileInfo>
#include <QSettings>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QTcpServer>
#include <QtEndian>

MainWindow::MainWindow(QTcpServer *serverSocket) :
	QMainWindow(),
	serverSocket(serverSocket),
	clientIndex(0)
{
	trackView = new TrackView(this);
	setCentralWidget(trackView);

	connect(trackView, SIGNAL(posChanged(int, int)),
	        this, SLOT(onPosChanged(int, int)));
	connect(trackView,     SIGNAL(pauseChanged(bool)),
	        &clientSocket, SLOT(onPauseChanged(bool)));
	connect(trackView, SIGNAL(currValDirty()),
	        this, SLOT(onCurrValDirty()));

	createMenuBar();
	createStatusBar();

	connect(serverSocket, SIGNAL(newConnection()),
	        this, SLOT(onNewConnection()));
}

void MainWindow::showEvent(QShowEvent *event)
{
	QMainWindow::showEvent(event);

	// workaround for QTBUG-16507
	QString filePath = windowFilePath();
	setWindowFilePath(filePath + "foo");
	setWindowFilePath(filePath);
}


void MainWindow::createMenuBar()
{
	fileMenu = menuBar()->addMenu("&File");
	fileMenu->addAction(QIcon::fromTheme("document-new"), "New", this, SLOT(fileNew()), QKeySequence::New);
	fileMenu->addAction(QIcon::fromTheme("document-open"), "&Open", this, SLOT(fileOpen()), QKeySequence::Open);
	fileMenu->addAction(QIcon::fromTheme("document-save"), "&Save", this, SLOT(fileSave()), QKeySequence::Save);
	fileMenu->addAction(QIcon::fromTheme("document-save-as"),"Save &As", this, SLOT(fileSaveAs()), QKeySequence::SaveAs);
	fileMenu->addSeparator();
	fileMenu->addAction("Remote &Export", this, SLOT(fileRemoteExport()), Qt::CTRL + Qt::Key_E);
	recentFilesMenu = fileMenu->addMenu(QIcon::fromTheme("document-open-recent"), "Recent &Files");
	for (int i = 0; i < 5; ++i) {
		recentFileActions[i] = recentFilesMenu->addAction(QIcon::fromTheme("document-open-recent"), "");
		recentFileActions[i]->setVisible(false);
		connect(recentFileActions[i], SIGNAL(triggered()),
		        this, SLOT(openRecentFile()));
	}
	updateRecentFiles();
	fileMenu->addSeparator();
	fileMenu->addAction(QIcon::fromTheme("application-exit"), "E&xit", this, SLOT(fileQuit()), QKeySequence::Quit);

	editMenu = menuBar()->addMenu("&Edit");
	editMenu->addAction(QIcon::fromTheme("edit-undo"), "Undo", trackView, SLOT(editUndo()), QKeySequence::Undo);
	editMenu->addAction(QIcon::fromTheme("edit-redo"), "Redo", trackView, SLOT(editRedo()), QKeySequence::Redo);
	editMenu->addSeparator();
	editMenu->addAction(QIcon::fromTheme("edit-copy"), "&Copy", trackView, SLOT(editCopy()), QKeySequence::Copy);
	editMenu->addAction(QIcon::fromTheme("edit-cut"), "Cu&t", trackView, SLOT(editCut()), QKeySequence::Cut);
	editMenu->addAction(QIcon::fromTheme("edit-paste"), "&Paste", trackView, SLOT(editPaste()), QKeySequence::Paste);
	editMenu->addAction(QIcon::fromTheme("edit-clear"), "Clear", trackView, SLOT(editClear()), QKeySequence::Delete);
	editMenu->addSeparator();
	editMenu->addAction(QIcon::fromTheme("edit-select-all"), "Select All", trackView, SLOT(selectAll()), QKeySequence::SelectAll);
	editMenu->addAction("Select Track", trackView, SLOT(selectTrack()), Qt::CTRL + Qt::Key_T);
	editMenu->addAction("Select Row", trackView, SLOT(selectRow()));
	editMenu->addSeparator();
	editMenu->addAction("Bias Selection", this, SLOT(editBiasSelection()), Qt::CTRL + Qt::Key_B);
	editMenu->addSeparator();
	editMenu->addAction("Set Rows", this, SLOT(editSetRows()), Qt::CTRL + Qt::Key_R);
	editMenu->addSeparator();
	editMenu->addAction("Previous Bookmark", this, SLOT(editPreviousBookmark()), Qt::ALT + Qt::Key_PageUp);
	editMenu->addAction("Next Bookmark", this, SLOT(editNextBookmark()), Qt::ALT + Qt::Key_PageDown);
}

void MainWindow::createStatusBar()
{
	statusPos = new QLabel;
	statusValue = new QLabel;
	statusKeyType = new QLabel;

	statusBar()->addPermanentWidget(statusPos);
	statusBar()->addPermanentWidget(statusValue);
	statusBar()->addPermanentWidget(statusKeyType);

	statusBar()->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

	setStatusText("Not connected");
	setStatusPosition(0, 0);
	setStatusValue(0.0f, false);
	setStatusKeyType(SyncTrack::TrackKey::STEP, false);
}

static QStringList getRecentFiles()
{
#ifdef Q_OS_WIN32
	QSettings settings("HKEY_CURRENT_USER\\Software\\GNU Rocket",
	                   QSettings::NativeFormat);
#else
	QSettings settings;
#endif
	QStringList list;
	for (int i = 0; i < 5; ++i) {
		QVariant string = settings.value(QString("RecentFile%1").arg(i));
		if (string.isValid())
			list.push_back(string.toString());
	}
	return list;
}

static void setRecentFiles(const QStringList &files)
{
#ifdef Q_OS_WIN32
	QSettings settings("HKEY_CURRENT_USER\\Software\\GNU Rocket",
	                   QSettings::NativeFormat);
#else
	QSettings settings;
#endif

	for (int i = 0; i < files.size(); ++i)
		settings.setValue(QString("RecentFile%1").arg(i), files[i]);

	// remove keys not in the list
	for (int i = files.size(); ;++i) {
		QString key = QString("RecentFile%1").arg(i);

		if (!settings.contains(key))
			break;

		settings.remove(key);
	}
}

void MainWindow::updateRecentFiles()
{
	QStringList files = getRecentFiles();

	if (!files.size()) {
		recentFilesMenu->setEnabled(false);
		return;
	}

	Q_ASSERT(files.size() <= 5);
	for (int i = 0; i < files.size(); ++i) {
		QFileInfo info(files[i]);
		QString text = QString("&%1 %2").arg(i + 1).arg(info.fileName());

		recentFileActions[i]->setText(text);
		recentFileActions[i]->setData(info.absoluteFilePath());
		recentFileActions[i]->setVisible(true);
	}
	for (int i = files.size(); i < 5; ++i)
		recentFileActions[i]->setVisible(false);
	recentFilesMenu->setEnabled(true);
}

void MainWindow::setCurrentFileName(const QString &fileName)
{
	QFileInfo info(fileName);

	QStringList files = getRecentFiles();
	files.removeAll(info.absoluteFilePath());
	files.prepend(info.absoluteFilePath());
	while (files.size() > 5)
		files.removeLast();
	setRecentFiles(files);

	updateRecentFiles();
	setWindowFilePath(fileName);
}

void MainWindow::setStatusText(const QString &text)
{
	statusBar()->showMessage(text);
}

void MainWindow::setStatusPosition(int col, int row)
{
	statusPos->setText(QString("Row %1, Col %2").arg(row).arg(col));
}

void MainWindow::setStatusValue(double val, bool valid)
{
	if (valid)
		statusValue->setText(QString::number(val, 'f', 3));
	else
		statusValue->setText("---");
}

void MainWindow::setStatusKeyType(SyncTrack::TrackKey::KeyType keyType, bool valid)
{
	if (!valid) {
		statusKeyType->setText("---");
		return;
	}

	switch (keyType) {
	case SyncTrack::TrackKey::STEP:   statusKeyType->setText("step"); break;
	case SyncTrack::TrackKey::LINEAR: statusKeyType->setText("linear"); break;
	case SyncTrack::TrackKey::SMOOTH: statusKeyType->setText("smooth"); break;
	case SyncTrack::TrackKey::RAMP:   statusKeyType->setText("ramp"); break;
	default: Q_ASSERT(false);
	}
}

void MainWindow::setDocument(SyncDocument *newDoc)
{
	SyncDocument *oldDoc = trackView->getDocument();

	if (oldDoc)
		QObject::disconnect(oldDoc, SIGNAL(modifiedChanged(bool)),
		                    this, SLOT(setWindowModified(bool)));

	if (oldDoc && clientSocket.connected()) {
		// delete old key frames
		for (int i = 0; i < oldDoc->getTrackCount(); ++i) {
			SyncTrack *t = oldDoc->getTrack(i);
			QMap<int, SyncTrack::TrackKey> keyMap = t->getKeyMap();
			QMap<int, SyncTrack::TrackKey>::const_iterator it;
			for (it = keyMap.constBegin(); it != keyMap.constEnd(); ++it)
				t->removeKey(it.key());
			QObject::disconnect(t, SIGNAL(keyFrameChanged(const SyncTrack &, int)),
			        &clientSocket, SLOT(onKeyFrameChanged(const SyncTrack &, int)));
		}

		if (newDoc) {
			// add back missing client-tracks
			QMap<QString, size_t>::const_iterator it;
			for (it = clientSocket.clientTracks.begin(); it != clientSocket.clientTracks.end(); ++it) {
				SyncTrack *t = newDoc->findTrack(it.key());
				if (!t)
					newDoc->createTrack(it.key());
			}

			for (int i = 0; i < newDoc->getTrackCount(); ++i) {
				SyncTrack *t = newDoc->getTrack(i);
				QMap<int, SyncTrack::TrackKey> keyMap = t->getKeyMap();
				QMap<int, SyncTrack::TrackKey>::const_iterator it;
				for (it = keyMap.constBegin(); it != keyMap.constEnd(); ++it)
					clientSocket.sendSetKeyCommand(t->name.toUtf8().constData(), *it);
				QObject::connect(t,             SIGNAL(keyFrameChanged(const SyncTrack &, int)),
						 &clientSocket, SLOT(onKeyFrameChanged(const SyncTrack &, int)));
			}
		}
	}

	trackView->setDocument(newDoc);
	trackView->dirtyCurrentValue();
	trackView->viewport()->update();

	QObject::connect(newDoc, SIGNAL(modifiedChanged(bool)),
	                 this, SLOT(setWindowModified(bool)));

	if (oldDoc)
		delete oldDoc;
}

void MainWindow::fileNew()
{
	setDocument(new SyncDocument);
	setWindowFilePath("Untitled");
}

bool MainWindow::loadDocument(const QString &path)
{
	SyncDocument *newDoc = SyncDocument::load(path);
	if (newDoc) {
		// set new document
		setDocument(newDoc);
		setCurrentFileName(path);
		return true;
	}
	return false;
}

void MainWindow::fileOpen()
{
	QString fileName = QFileDialog::getOpenFileName(this, "Open File", "", "ROCKET File (*.rocket);;All Files (*.*)");
	if (fileName.length()) {
		loadDocument(fileName);
	}
}

void MainWindow::fileSaveAs()
{
	QString fileName = QFileDialog::getSaveFileName(this, "Save File", "", "ROCKET File (*.rocket);;All Files (*.*)");
	if (fileName.length()) {
		SyncDocument *doc = trackView->getDocument();
		if (doc->save(fileName)) {
			clientSocket.sendSaveCommand();
			setCurrentFileName(fileName);
			doc->fileName = fileName;
		}
	}
}

void MainWindow::fileSave()
{
	SyncDocument *doc = trackView->getDocument();
	if (doc->fileName.isEmpty())
		return fileSaveAs();

	if (!doc->save(doc->fileName))
		clientSocket.sendSaveCommand();
}

void MainWindow::fileRemoteExport()
{
	clientSocket.sendSaveCommand();
}

void MainWindow::openRecentFile()
{
	QAction *action = qobject_cast<QAction *>(sender());
	if (action) {
		QString fileName = action->data().toString();
		if (!loadDocument(fileName)) {
			QStringList files = getRecentFiles();
			files.removeAll(fileName);
			setRecentFiles(files);
			updateRecentFiles();
		}
	}
}

void MainWindow::fileQuit()
{
	SyncDocument *doc = trackView->getDocument();
	if (doc->isModified()) {
		QMessageBox::StandardButton res = QMessageBox::question(
		    this, "GNU Rocket", "Save before exit?",
		    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
		if (res == QMessageBox::Yes) {
			fileSave();
			QApplication::quit();
		} else if (res == QMessageBox::No)
			QApplication::quit();
	}
	else QApplication::quit();
}

void MainWindow::editBiasSelection()
{
	bool ok = false;
	float bias = QInputDialog::getDouble(this, "Bias Selection", "", 0, INT_MIN, INT_MAX, 1, &ok);
	if (ok)
		trackView->editBiasValue(bias);
}

void MainWindow::editSetRows()
{
	bool ok = false;
	int rows = QInputDialog::getInt(this, "Set Rows", "", trackView->getRows(), 0, INT_MAX, 1, &ok);
	if (ok)
		trackView->setRows(rows);
}

void MainWindow::editPreviousBookmark()
{
	int row = trackView->getDocument()->prevRowBookmark(trackView->getEditRow());
	if (row >= 0)
		trackView->setEditRow(row);
}

void MainWindow::editNextBookmark()
{
	int row = trackView->getDocument()->nextRowBookmark(trackView->getEditRow());
	if (row >= 0)
		trackView->setEditRow(row);
}

void MainWindow::onPosChanged(int col, int row)
{
	setStatusPosition(col, row);
	if (trackView->paused && clientSocket.connected())
		clientSocket.sendSetRowCommand(row);
}

void MainWindow::onCurrValDirty()
{
	SyncDocument *doc = trackView->getDocument();
	if (doc && doc->getTrackCount() > 0) {
		const SyncTrack *t = doc->getTrack(doc->getTrackIndexFromPos(trackView->getEditTrack()));
		int row = trackView->getEditRow();

		setStatusValue(t->getValue(row), true);

		const SyncTrack::TrackKey *k = t->getPrevKeyFrame(row);
		if (k)
			setStatusKeyType(k->type, true);
		else
			setStatusKeyType(SyncTrack::TrackKey::STEP, false);
	} else {
		setStatusValue(0.0f, false);
		setStatusKeyType(SyncTrack::TrackKey::STEP, false);
	}
}

void MainWindow::processCommand(ClientSocket &sock)
{
	unsigned char cmd = 0;
	if (sock.recv((char*)&cmd, 1)) {
		switch (cmd) {
		case GET_TRACK:
			processGetTrack(sock);
			break;

		case SET_ROW:
			processSetRow(sock);
			break;
		}
	}
}

void MainWindow::processGetTrack(ClientSocket &sock)
{
	SyncDocument *doc = trackView->getDocument();

	// read data
	int strLen;
	sock.recv((char *)&strLen, sizeof(int));
	strLen = qFromBigEndian((quint32)strLen);
	if (!sock.connected())
		return;

	if (!strLen) {
		sock.disconnect();
		trackView->update();
		return;
	}

	QByteArray trackNameBuffer;
	trackNameBuffer.resize(strLen);
	if (!sock.recv(trackNameBuffer.data(), strLen))
		return;

	if (trackNameBuffer.contains('\0')) {
		sock.disconnect();
		trackView->update();
		return;
	}

	QString trackName = QString::fromUtf8(trackNameBuffer);

	// find track
	const SyncTrack *t = doc->findTrack(trackName.toUtf8());
	if (!t)
		t = doc->createTrack(trackName);

	// hook up signals to slots
	QObject::connect(t,             SIGNAL(keyFrameChanged(const SyncTrack &, int)),
			 &clientSocket, SLOT(onKeyFrameChanged(const SyncTrack &, int)));

	// setup remap
	clientSocket.clientTracks[trackName] = clientIndex++;

	// send key frames
	QMap<int, SyncTrack::TrackKey> keyMap = t->getKeyMap();
	QMap<int, SyncTrack::TrackKey>::const_iterator it;
	for (it = keyMap.constBegin(); it != keyMap.constEnd(); ++it)
		clientSocket.sendSetKeyCommand(t->name.toUtf8().constData(), *it);

	trackView->update();
}

void MainWindow::processSetRow(ClientSocket &sock)
{
	int newRow;
	sock.recv((char*)&newRow, sizeof(int));
	trackView->setEditRow(qToBigEndian((quint32)newRow));
}

static TcpSocket *clientConnect(QTcpServer *serverSocket, QHostAddress *host)
{
	QTcpSocket *clientSocket = serverSocket->nextPendingConnection();
	Q_ASSERT(clientSocket != NULL);

	QByteArray line;

	// Read greetings or WebSocket upgrade
	// command from the socket
	for (;;) {
		char ch;
		if (!clientSocket->getChar(&ch)) {
			// Read failed; wait for data and try again
			clientSocket->waitForReadyRead();
			if(!clientSocket->getChar(&ch)) {
				clientSocket->close();
				return NULL;
			}
		}

		if (ch == '\n')
			break;
		if (ch != '\r')
			line.push_back(ch);
		if (ch == '!')
			break;
	}

	TcpSocket *ret = NULL;
	if (line.startsWith("GET ")) {
		ret = WebSocket::upgradeFromHttp(clientSocket);
		line.resize(int(strlen(CLIENT_GREET)));
		if (!ret || !ret->recv(line.data(), line.size())) {
			clientSocket->close();
			return NULL;
		}
	} else
		ret = new TcpSocket(clientSocket);

	if (!line.startsWith(CLIENT_GREET) ||
	    !ret->send(SERVER_GREET, strlen(SERVER_GREET), true)) {
		ret->disconnect();
		return NULL;
	}

	if (NULL != host)
		*host = clientSocket->peerAddress();
	return ret;
}

void MainWindow::onReadyRead()
{
	while (clientSocket.pollRead())
		processCommand(clientSocket);
}

void MainWindow::onNewConnection()
{
	if (!clientSocket.connected()) {
		setStatusText("Accepting...");
		QHostAddress client;
		TcpSocket *socket = clientConnect(serverSocket, &client);
		if (socket) {
			setStatusText(QString("Connected to %1").arg(client.toString()));
			clientSocket.socket = socket;
			connect(socket->socket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
			connect(socket->socket, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
			clientIndex = 0;
			clientSocket.sendPauseCommand(trackView->paused);
			clientSocket.sendSetRowCommand(trackView->getEditRow());
			trackView->connected = true;
		} else
			setStatusText(QString("Not Connected: %1").arg(serverSocket->errorString()));
	}
}

void MainWindow::onDisconnected()
{
	trackView->paused = true;
	clientSocket.disconnect();

	// disconnect track-signals
	SyncDocument *doc = trackView->getDocument();
	for (int i = 0; i < doc->getTrackCount(); ++i)
		QObject::disconnect(doc->getTrack(i), SIGNAL(keyFrameChanged(const SyncTrack &, int)),
		                      &clientSocket, SLOT(onKeyFrameChanged(const SyncTrack &, int)));

	trackView->update();
	setStatusText("Not Connected.");
	trackView->connected = false;
}
