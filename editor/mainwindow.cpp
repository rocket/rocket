#include "mainwindow.h"
#include "trackview.h"
#include "syncclient.h"
#include "syncdocument.h"

#include <QApplication>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QFileInfo>
#include <QFont>
#include <QFontDialog>
#include <QSettings>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QTabWidget>
#include <QTcpServer>
#include <QtEndian>

#ifdef QT_WEBSOCKETS_LIB
#include <QWebSocketServer>
#include <QWebSocket>
#endif

MainWindow::MainWindow() :
	QMainWindow(),
#ifdef Q_OS_WIN32
	settings("HKEY_CURRENT_USER\\Software\\GNU Rocket", QSettings::NativeFormat),
#endif
	syncClient(NULL),
	doc(NULL),
	currentTrackView(NULL)
{
#ifdef Q_OS_WIN
	trackViewFont = QFont("Consolas", 11);
#elif defined(Q_OS_OSX)
	trackViewFont = QFont("Monaco");
#else
	trackViewFont = QFont("Monospace");
	trackViewFont.setStyleHint(QFont::TypeWriter);
#endif

	QVariant fontSetting = settings.value("font");
	if (fontSetting.isValid() && fontSetting.type() == QVariant::String)
		trackViewFont.fromString(fontSetting.toString());

	tabWidget = new QTabWidget(this);
	tabWidget->setDocumentMode(true);
	connect(tabWidget, SIGNAL(currentChanged(int)),
	        this, SLOT(onTabChanged(int)));

	setCentralWidget(tabWidget);

	createMenuBar();
	updateRecentFiles();

	createStatusBar();

	tcpServer = new QTcpServer();
	connect(tcpServer, SIGNAL(newConnection()),
	        this, SLOT(onNewTcpConnection()));

	if (!tcpServer->listen(QHostAddress::Any, 1338))
		statusBar()->showMessage(QString("Could not start server: %1").arg(tcpServer->errorString()));

#ifdef QT_WEBSOCKETS_LIB
	wsServer = new QWebSocketServer("Rocket Editor", QWebSocketServer::NonSecureMode);
	connect(wsServer, SIGNAL(newConnection()),
	        this, SLOT(onNewWsConnection()));

	if (!wsServer->listen(QHostAddress::Any, 1339))
		statusBar()->showMessage(QString("Could not start server: %1").arg(wsServer->errorString()));
#endif
}

void MainWindow::showEvent(QShowEvent *event)
{
	QMainWindow::showEvent(event);

	// workaround for QTBUG-16507
	QString filePath = windowFilePath();
	setWindowFilePath(filePath + "foo");
	setWindowFilePath(filePath);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
	bool ctrlDown = (event->modifiers() & Qt::ControlModifier) != 0;

	switch (event->key()) {
	case Qt::Key_1: case Qt::Key_2: case Qt::Key_3:
	case Qt::Key_4: case Qt::Key_5: case Qt::Key_6:
	case Qt::Key_7: case Qt::Key_8:
		if (ctrlDown) {
			int tab = event->key() - Qt::Key_1;
			if (tab < tabWidget->count())
				tabWidget->setCurrentIndex(tab);
			else
				QApplication::beep();
			return;
		}
		break;

	case Qt::Key_9:
		if (ctrlDown) {
			if (tabWidget->count() > 0)
				tabWidget->setCurrentIndex(tabWidget->count() - 1);
			else
				QApplication::beep();
			return;
		}
		break;

	case Qt::Key_Space:
		if (syncClient) {
			setPaused(!syncClient->isPaused());
			return;
		}
		break;
	}
}

void MainWindow::createMenuBar()
{
	QMenu *fileMenu = menuBar()->addMenu("&File");
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
	fileMenu->addSeparator();
	fileMenu->addAction(QIcon::fromTheme("application-exit"), "E&xit", this, SLOT(fileQuit()), QKeySequence::Quit);

	QMenu *editMenu = menuBar()->addMenu("&Edit");
	editMenu->addAction(QIcon::fromTheme("edit-undo"), "Undo", this, SLOT(editUndo()), QKeySequence::Undo);
	editMenu->addAction(QIcon::fromTheme("edit-redo"), "Redo", this, SLOT(editRedo()), QKeySequence::Redo);
	editMenu->addSeparator();
	editMenu->addAction(QIcon::fromTheme("edit-copy"), "&Copy", this, SLOT(editCopy()), QKeySequence::Copy);
	editMenu->addAction(QIcon::fromTheme("edit-cut"), "Cu&t", this, SLOT(editCut()), QKeySequence::Cut);
	editMenu->addAction(QIcon::fromTheme("edit-paste"), "&Paste", this, SLOT(editPaste()), QKeySequence::Paste);
	editMenu->addAction(QIcon::fromTheme("edit-clear"), "Clear", this, SLOT(editClear()), QKeySequence::Delete);
	editMenu->addSeparator();
	editMenu->addAction(QIcon::fromTheme("edit-select-all"), "Select All", this, SLOT(editSelectAll()), QKeySequence::SelectAll);
	editMenu->addAction("Select Track", this, SLOT(editSelectTrack()), Qt::CTRL + Qt::Key_T);
	editMenu->addAction("Select Row", this, SLOT(editSelectRow()));
	editMenu->addSeparator();
	editMenu->addAction("Bias Selection", this, SLOT(editBiasSelection()), Qt::CTRL + Qt::Key_B);
	editMenu->addSeparator();
	editMenu->addAction("Set Rows", this, SLOT(editSetRows()), Qt::CTRL + Qt::Key_R);
	editMenu->addSeparator();
	editMenu->addAction("Set Font", this, SLOT(editSetFont()));
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

	statusBar()->showMessage("Not connected");
	onPosChanged(0, 0);
	onCurrValDirty();
}

QStringList MainWindow::getRecentFiles() const
{
	QStringList list;
	for (int i = 0; i < 5; ++i) {
		QVariant string = settings.value(QString("RecentFile%1").arg(i));
		if (string.isValid())
			list.push_back(string.toString());
	}
	return list;
}

void MainWindow::setRecentFiles(const QStringList &files)
{
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

void MainWindow::setDocument(SyncDocument *newDoc)
{
	if (doc) {
		QObject::disconnect(doc, SIGNAL(syncPageAdded(SyncPage *)),
		                    this, SLOT(onSyncPageAdded(SyncPage *)));
		QObject::disconnect(doc, SIGNAL(modifiedChanged(bool)),
		                    this, SLOT(setWindowModified(bool)));
	}

	if (doc && syncClient) {
		// delete old key frames
		for (int i = 0; i < doc->getTrackCount(); ++i) {
			SyncTrack *t = doc->getTrack(i);
			QMap<int, SyncTrack::TrackKey> keyMap = t->getKeyMap();
			QMap<int, SyncTrack::TrackKey>::const_iterator it;
			for (it = keyMap.constBegin(); it != keyMap.constEnd(); ++it)
				t->removeKey(it.key());

			QObject::disconnect(t, SIGNAL(keyFrameAdded(int)),
			                    syncClient, SLOT(onKeyFrameAdded(int)));
			QObject::disconnect(t, SIGNAL(keyFrameChanged(int, const SyncTrack::TrackKey &)),
			                    syncClient, SLOT(onKeyFrameChanged(int, const SyncTrack::TrackKey &)));
			QObject::disconnect(t, SIGNAL(keyFrameRemoved(int, const SyncTrack::TrackKey &)),
			                    syncClient, SLOT(onKeyFrameRemoved(int, const SyncTrack::TrackKey &)));
		}

		if (newDoc) {
			// add back missing client-tracks
			QStringList trackNames = syncClient->getTrackNames();
			for (int i = 0; i < trackNames.size(); ++i) {
				SyncTrack *t = newDoc->findTrack(trackNames[i]);
				if (!t)
					t = newDoc->createTrack(trackNames[i]);
				t->setActive(true);
			}

			for (int i = 0; i < newDoc->getTrackCount(); ++i) {
				SyncTrack *t = newDoc->getTrack(i);
				QMap<int, SyncTrack::TrackKey> keyMap = t->getKeyMap();
				QMap<int, SyncTrack::TrackKey>::const_iterator it;
				for (it = keyMap.constBegin(); it != keyMap.constEnd(); ++it)
					syncClient->sendSetKeyCommand(t->getName(), *it);

				QObject::connect(t, SIGNAL(keyFrameAdded(int)),
				                 syncClient, SLOT(onKeyFrameAdded(int)));
				QObject::connect(t, SIGNAL(keyFrameChanged(int, const SyncTrack::TrackKey &)),
				                 syncClient, SLOT(onKeyFrameChanged(int, const SyncTrack::TrackKey &)));
				QObject::connect(t, SIGNAL(keyFrameRemoved(int, const SyncTrack::TrackKey &)),
				                 syncClient, SLOT(onKeyFrameRemoved(int, const SyncTrack::TrackKey &)));
			}
		}
	}

	// recreate empty set of trackViews
	while (trackViews.count() > 0) {
		TrackView *trackView = trackViews.front();
		trackViews.removeFirst();
		delete trackView;
	}
	trackViews.clear();

	for (int i = 0; i < newDoc->getSyncPageCount(); ++i)
		addTrackView(newDoc->getSyncPage(i));

	if (doc)
		delete doc;
	doc = newDoc;

	QObject::connect(doc, SIGNAL(syncPageAdded(SyncPage *)),
	                 this, SLOT(onSyncPageAdded(SyncPage *)));
	QObject::connect(newDoc, SIGNAL(modifiedChanged(bool)),
	                 this, SLOT(setWindowModified(bool)));

	onCurrValDirty();
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
		setWindowModified(false);
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
		if (doc->save(fileName)) {
			if (syncClient)
				syncClient->sendSaveCommand();

			setCurrentFileName(fileName);
			doc->fileName = fileName;
		}
	}
}

void MainWindow::fileSave()
{
	if (doc->fileName.isEmpty())
		return fileSaveAs();

	if (!doc->save(doc->fileName))
		fileRemoteExport();
}

void MainWindow::fileRemoteExport()
{
	if (syncClient)
		syncClient->sendSaveCommand();
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
	if (doc->isModified()) {
		QMessageBox::StandardButton res = QMessageBox::question(
		    this, "Rocket", "Save before exit?",
		    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
		if (res == QMessageBox::Yes) {
			fileSave();
			QApplication::quit();
		} else if (res == QMessageBox::No)
			QApplication::quit();
	}
	else QApplication::quit();
}

void MainWindow::editUndo()
{
	currentTrackView->editUndo();
}

void MainWindow::editRedo()
{
	currentTrackView->editRedo();
}

void MainWindow::editCopy()
{
	currentTrackView->editCopy();
}

void MainWindow::editCut()
{
	currentTrackView->editCut();
}

void MainWindow::editPaste()
{
	currentTrackView->editPaste();
}

void MainWindow::editClear()
{
	currentTrackView->editClear();
}

void MainWindow::editSelectAll()
{
	currentTrackView->selectAll();
}

void MainWindow::editSelectTrack()
{
	currentTrackView->selectTrack();
}

void MainWindow::editSelectRow()
{
	currentTrackView->selectRow();
}

void MainWindow::editBiasSelection()
{
	bool ok = false;
	float bias = QInputDialog::getDouble(this, "Bias Selection", "", 0, INT_MIN, INT_MAX, 1, &ok);
	if (ok)
		currentTrackView->editBiasValue(bias);
}

void MainWindow::editSetRows()
{
	bool ok = false;
	int rows = QInputDialog::getInt(this, "Set Rows", "", currentTrackView->getRows(), 0, INT_MAX, 1, &ok);
	if (ok) {
		for (int i = 0; i < trackViews.size(); ++i)
			trackViews[i]->setRows(rows);
		doc->setRows(rows);
	}
}

void MainWindow::editSetFont()
{
	bool ok = false;
	QFont font = QFontDialog::getFont(&ok, trackViewFont, this);
	if (ok) {
		for (int i = 0; i < trackViews.size(); ++i)
			trackViews[i]->setFont(font);
		settings.setValue("font", font.toString());
	}
}

void MainWindow::editPreviousBookmark()
{
	currentTrackView->editPreviousBookmark();
}

void MainWindow::editNextBookmark()
{
	currentTrackView->editNextBookmark();
}

void MainWindow::onPosChanged(int col, int row)
{
	statusPos->setText(QString("Row %1, Col %2").arg(row).arg(col));
}

void MainWindow::onEditRowChanged(int row)
{
	if (syncClient)
		syncClient->sendSetRowCommand(row);

	for (int i = 0; i < trackViews.size(); ++i) {
		if (trackViews[i] != QObject::sender())
			trackViews[i]->updateRow(row);
	}
}

void MainWindow::onCurrValDirty()
{
	if (doc && currentTrackView->getTrackCount() > 0) {
		const SyncTrack *t = currentTrackView->getTrack(currentTrackView->getEditTrack());
		int row = currentTrackView->getEditRow();

		statusValue->setText(QString::number(t->getValue(row), 'f', 3));

		const SyncTrack::TrackKey *k = t->getPrevKeyFrame(row);
		if (k) {
			switch (k->type) {
			case SyncTrack::TrackKey::STEP:   statusKeyType->setText("step"); break;
			case SyncTrack::TrackKey::LINEAR: statusKeyType->setText("linear"); break;
			case SyncTrack::TrackKey::SMOOTH: statusKeyType->setText("smooth"); break;
			case SyncTrack::TrackKey::RAMP:   statusKeyType->setText("ramp"); break;
			default: Q_ASSERT(false);
			}
		} else
			statusKeyType->setText("---");
	} else {
		statusValue->setText("---");
		statusKeyType->setText("---");
	}
}

TrackView *MainWindow::addTrackView(SyncPage *page)
{
	TrackView *trackView = new TrackView(page, NULL);
	trackView->setFont(trackViewFont);

	trackViews.append(trackView);
	tabWidget->addTab(trackView, page->getName());

	return trackView;
}

void MainWindow::onSyncPageAdded(SyncPage *page)
{
	addTrackView(page);
}

void MainWindow::onTabChanged(int index)
{
	Q_ASSERT(trackViews.size() == tabWidget->count());

	if (currentTrackView != NULL) {
		disconnect(posChangedConnection);
		disconnect(editRowChangedConnection);
		disconnect(editRowChangedConnection);
		currentTrackView = NULL;
	}

	if (index >= 0) {
		currentTrackView = trackViews[index];

		posChangedConnection = connect(
			currentTrackView, SIGNAL(posChanged(int, int)),
			this,             SLOT(onPosChanged(int, int)));
		editRowChangedConnection = connect(
			currentTrackView, SIGNAL(editRowChanged(int)),
			this,             SLOT(onEditRowChanged(int)));
		currValDirtyConnection = connect(
			currentTrackView, SIGNAL(currValDirty()),
			this,             SLOT(onCurrValDirty()));

		currentTrackView->setFocus();
	}
}

void MainWindow::onTrackRequested(const QString &trackName)
{
	// find track
	SyncTrack *t = doc->findTrack(trackName.toUtf8());
	if (!t)
		t = doc->createTrack(trackName);

	// hook up signals to slots
	QObject::connect(t, SIGNAL(keyFrameAdded(int)),
	                 syncClient, SLOT(onKeyFrameAdded(int)));
	QObject::connect(t, SIGNAL(keyFrameChanged(int, const SyncTrack::TrackKey &)),
	                 syncClient, SLOT(onKeyFrameChanged(int, const SyncTrack::TrackKey &)));
	QObject::connect(t, SIGNAL(keyFrameRemoved(int, const SyncTrack::TrackKey &)),
	                 syncClient, SLOT(onKeyFrameRemoved(int, const SyncTrack::TrackKey &)));

	// send key frames
	QMap<int, SyncTrack::TrackKey> keyMap = t->getKeyMap();
	QMap<int, SyncTrack::TrackKey>::const_iterator it;
	for (it = keyMap.constBegin(); it != keyMap.constEnd(); ++it)
		syncClient->sendSetKeyCommand(t->getName(), *it);

	t->setActive(true);
}

void MainWindow::onClientRowChanged(int row)
{
	for (int i = 0; i < trackViews.count(); ++i)
		trackViews[i]->updateRow(row);
}

void MainWindow::onClientKeyChanged(int track, int row, float value, SyncTrack::TrackKey::KeyType type)
{
	if (track < doc->getTrackCount()) {
		SyncTrack::TrackKey newKey;
		SyncTrack *t = doc->getTrack(track);

		/* ensure the editor is on the row being changed by the demo */
		onClientRowChanged(row);

		newKey.type = type;
		newKey.row = row;
		if (t->isKeyFrame(row))
			newKey = t->getKeyFrame(row);

		newKey.value = value;

		doc->setKeyFrame(t, newKey);
		onCurrValDirty();
	} else
		QApplication::beep();
}

void MainWindow::onClientPauseToggled()
{
	if (syncClient)
		setPaused(!syncClient->isPaused());
}

void MainWindow::setPaused(bool pause)
{
	if (syncClient)
		syncClient->setPaused(pause);

	for (int i = 0; i < trackViews.count(); ++i)
		trackViews[i]->setReadOnly(!pause);
}

void MainWindow::setSyncClient(SyncClient *client)
{
	Q_ASSERT(syncClient == NULL);
	Q_ASSERT(client != NULL);

	connect(client, SIGNAL(trackRequested(const QString &)), this, SLOT(onTrackRequested(const QString &)));
	connect(client, SIGNAL(rowChanged(int)), this, SLOT(onClientRowChanged(int)));
	connect(client, SIGNAL(keyChanged(int, int, float, SyncTrack::TrackKey::KeyType)), this, SLOT(onClientKeyChanged(int, int, float, SyncTrack::TrackKey::KeyType)));
	connect(client, SIGNAL(pauseToggled()), this, SLOT(onClientPauseToggled()));
	connect(client, SIGNAL(connected()), this, SLOT(onConnected()));
	connect(client, SIGNAL(disconnected(const QString &)), this, SLOT(onDisconnected(const QString &)));
	syncClient = client;
}

void MainWindow::onNewTcpConnection()
{
	QTcpSocket *pendingSocket = tcpServer->nextPendingConnection();
	if (!syncClient) {
		statusBar()->showMessage("Accepting...");

		QByteArray greeting = QString(CLIENT_GREET).toUtf8();
		QByteArray response = QString(SERVER_GREET).toUtf8();

		while (pendingSocket->bytesAvailable() < greeting.length() &&
				pendingSocket->waitForReadyRead())
			; // wait until we have the message or got an error
		QByteArray line = pendingSocket->read(greeting.length());
		if (line != greeting ||
		    pendingSocket->write(response) != response.length()) {
			pendingSocket->close();

			statusBar()->showMessage(QString("Not Connected: %1").arg(tcpServer->errorString()));
			return;
		}

		AbstractSocketClient *client = new AbstractSocketClient(pendingSocket);
		statusBar()->showMessage(QString("Connected to %1").arg(pendingSocket->peerAddress().toString()));

		setSyncClient(client);

		client->notifyConnected(); // we already performed the hand-shake, unlike the WebSocket client
	} else
		pendingSocket->close();
}

#ifdef QT_WEBSOCKETS_LIB

void MainWindow::onNewWsConnection()
{
	QWebSocket *pendingSocket = wsServer->nextPendingConnection();

	if (!syncClient) {
		statusBar()->showMessage("Accepting...");

		SyncClient *client = new WebSocketClient(pendingSocket);
		statusBar()->showMessage(QString("Connected to %1").arg(pendingSocket->peerAddress().toString()));

		setSyncClient(client);
	} else
		pendingSocket->close();
}

#endif

void MainWindow::onConnected()
{
	setPaused(true);
	syncClient->sendSetRowCommand(currentTrackView->getEditRow());
}

void MainWindow::onDisconnected(const QString &error)
{
	setPaused(true);

	// disconnect track-signals
	for (int i = 0; i < doc->getTrackCount(); ++i) {
		SyncTrack *t = doc->getTrack(i);

		QObject::disconnect(t, SIGNAL(keyFrameAdded(int)),
		                    syncClient, SLOT(onKeyFrameAdded(int)));
		QObject::disconnect(t, SIGNAL(keyFrameChanged(int, const SyncTrack::TrackKey &)),
		                    syncClient, SLOT(onKeyFrameChanged(int, const SyncTrack::TrackKey &)));
		QObject::disconnect(t, SIGNAL(keyFrameRemoved(int, const SyncTrack::TrackKey &)),
		                    syncClient, SLOT(onKeyFrameRemoved(int, const SyncTrack::TrackKey &)));

		t->setActive(false);
	}

	if (syncClient) {
		delete syncClient;
		syncClient = NULL;
	}

	statusBar()->showMessage("Disconnected: " + error);
}
