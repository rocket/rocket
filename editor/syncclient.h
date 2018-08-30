#ifndef CLIENTSOCKET_H
#define CLIENTSOCKET_H

#include <QTcpSocket>
#include <QByteArray>
#include <QObject>
#include <QStringList>

#include "synctrack.h"

#define CLIENT_GREET "hello, synctracker!"
#define SERVER_GREET "hello, demo!"

enum {
	SET_KEY = 0,
	DELETE_KEY = 1,
	GET_TRACK = 2,
	SET_ROW = 3,
	PAUSE = 4,
	SAVE_TRACKS = 5
};

class SyncClient : public QObject {
	Q_OBJECT

public:
	SyncClient() : paused(false) { }

	virtual void close() = 0;
	virtual qint64 sendData(const QByteArray &data) = 0;

	void sendSetKeyCommand(const QString &trackName, const SyncTrack::TrackKey &key);
	void sendDeleteKeyCommand(const QString &trackName, int row);
	void sendSetRowCommand(int row);
	void sendSaveCommand();

	const QStringList getTrackNames() { return trackNames; }
	bool isPaused() { return paused; }
	void setPaused(bool);

signals:
	void connected();
	void disconnected(const QString &reason);
	void trackRequested(const QString &trackName);
	void rowChanged(int row);

public slots:
	void onKeyFrameAdded(int row)
	{
		const SyncTrack *track = qobject_cast<SyncTrack *>(sender());
		sendSetKeyCommand(track->getName(), track->getKeyFrame(row));
	}

	void onKeyFrameChanged(int row, const SyncTrack::TrackKey &)
	{
		const SyncTrack *track = qobject_cast<SyncTrack *>(sender());
		sendSetKeyCommand(track->getName(), track->getKeyFrame(row));
	}

	void onKeyFrameRemoved(int row, const SyncTrack::TrackKey &)
	{
		const SyncTrack *track = qobject_cast<SyncTrack *>(sender());
		sendDeleteKeyCommand(track->getName(), row);
	}

protected:
	void requestTrack(const QString &trackName);
	void sendPauseCommand(bool pause);

	QList<QString> trackNames;
	bool paused;
};

class AbstractSocketClient : public SyncClient {
	Q_OBJECT
public:
	explicit AbstractSocketClient(QAbstractSocket *socket) : socket(socket)
	{
		connect(socket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
		connect(socket, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
	}

	virtual void close()
	{
		disconnect(socket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
		disconnect(socket, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
		socket->close();
	}

	qint64 sendData(const QByteArray &data)
	{
		qint64 ret = socket->write(data);
		socket->flush();
		return ret;
	}

	void notifyConnected()
	{
		emit connected();
	}

private:
	QAbstractSocket *socket;
	bool recv(char *buffer, qint64 length);

	void processCommand();
	void processGetTrack();
	void processSetRow();

private slots:
	void onReadyRead();
	void onDisconnected();
};

#ifdef QT_WEBSOCKETS_LIB

class QWebSocket;

class WebSocketClient : public SyncClient {
	Q_OBJECT
public:
	explicit WebSocketClient(QWebSocket *socket);
	void close();
	qint64 sendData(const QByteArray &data);

private slots:
	void processTextMessage(const QString &message);
	void onMessageReceived(const QByteArray &data);
	void onDisconnected();

private:
	QWebSocket *socket;
};

#endif

#endif // !defined(CLIENTSOCKET_H)
