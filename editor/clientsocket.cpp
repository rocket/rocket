#include "clientsocket.h"
#include "syncdocument.h"

#include <QDataStream>
#include <QtEndian>

void ClientSocket::sendSetKeyCommand(const QString &trackName, const SyncTrack::TrackKey &key)
{
	int trackIndex = trackNames.indexOf(trackName);
	if (trackIndex < 0)
		return;

	union {
		float f;
		quint32 i;
	} v;
	v.f = key.value;

	Q_ASSERT(key.type < SyncTrack::TrackKey::KEY_TYPE_COUNT);

	QByteArray data;
	QDataStream ds(&data, QIODevice::WriteOnly);
	ds << (unsigned char)SET_KEY;
	ds << (quint32)trackIndex;
	ds << (quint32)key.row;
	ds << v.i;
	ds << (unsigned char)key.type;
	sendData(data);
}

void ClientSocket::sendDeleteKeyCommand(const QString &trackName, int row)
{
	int trackIndex = trackNames.indexOf(trackName);
	if (trackIndex < 0)
		return;

	QByteArray data;
	QDataStream ds(&data, QIODevice::WriteOnly);
	ds << (unsigned char)DELETE_KEY;
	ds << (quint32)trackIndex;
	ds << (quint32)row;
	sendData(data);
}

void ClientSocket::sendSetRowCommand(int row)
{
	QByteArray data;
	QDataStream ds(&data, QIODevice::WriteOnly);
	ds << (unsigned char)SET_ROW;
	ds << (quint32)row;
	sendData(data);
}

void ClientSocket::sendPauseCommand(bool pause)
{
	QByteArray data;
	QDataStream ds(&data, QIODevice::WriteOnly);
	ds << (unsigned char)PAUSE;
	ds << (unsigned char)pause;
	sendData(data);
}

void ClientSocket::sendSaveCommand()
{
	QByteArray data;
	data.append(SAVE_TRACKS);
	sendData(data);
}

void ClientSocket::setPaused(bool pause)
{
	if (pause != paused) {
		sendPauseCommand(pause);
		paused = pause;
	}
}

void ClientSocket::requestTrack(const QString &trackName)
{
	trackNames.append(trackName);
	emit trackRequested(trackName);
}

bool AbstractSocketClient::recv(char *buffer, qint64 length)
{
	// wait for enough data to arrive
	while (socket->bytesAvailable() < length) {
		if (!socket->waitForReadyRead(-1))
			return false;
	}

	qint64 ret = socket->read(buffer, length);
	Q_ASSERT(ret == length);
	Q_UNUSED(ret);
	return true;
}

void AbstractSocketClient::processCommand()
{
	unsigned char cmd = 0;
	if (recv((char*)&cmd, 1)) {
		switch (cmd) {
		case GET_TRACK:
			processGetTrack();
			break;

		case SET_ROW:
			processSetRow();
			break;
		}
	}
}

void AbstractSocketClient::processGetTrack()
{
	// read data
	quint32 strLen;
	if (!recv((char *)&strLen, sizeof(strLen))) {
		close();
		return;
	}

	strLen = qFromBigEndian(strLen);

	if (!strLen) {
		close();
		return;
	}

	QByteArray trackNameBuffer;
	trackNameBuffer.resize(strLen);
	if (!recv(trackNameBuffer.data(), strLen) ||
	    trackNameBuffer.contains('\0')) {
		close();
		return;
	}

	requestTrack(QString::fromUtf8(trackNameBuffer));
}

void AbstractSocketClient::processSetRow()
{
	quint32 newRow;
	if (recv((char *)&newRow, sizeof(newRow)))
		emit rowChanged(qFromBigEndian(newRow));
}

void AbstractSocketClient::onReadyRead()
{
	while (socket->bytesAvailable() > 0)
		processCommand();
}

#ifdef QT_WEBSOCKETS_LIB
#include <QWebSocket>

WebSocketClient::WebSocketClient(QWebSocket *socket) :
    socket(socket)
{
	connect(socket, SIGNAL(textMessageReceived(const QString &)), this, SLOT(processTextMessage(const QString &)));
	connect(socket, SIGNAL(disconnected()), this, SLOT(onDisconnected()));
	if (!socket->isValid())
		emit disconnected();
}

void WebSocketClient::close()
{
	socket->close();
}

qint64 WebSocketClient::sendData(const QByteArray &data)
{
	return socket->sendBinaryMessage(data);
}

void WebSocketClient::processTextMessage(const QString &message)
{
	QObject::disconnect(socket, SIGNAL(textMessageReceived(const QString &)), this, SLOT(processTextMessage(const QString &)));

	QByteArray response = QString(SERVER_GREET).toUtf8();
	if (message != CLIENT_GREET ||
		sendData(response) != response.length()) {
		socket->close();
	} else {
		connect(socket, SIGNAL(binaryMessageReceived(const QByteArray &)), this, SLOT(onMessageReceived(const QByteArray &)));
		emit connected();
	}
}

void WebSocketClient::onMessageReceived(const QByteArray &data)
{
	QDataStream ds(data);
	quint8 cmd;
	ds >> cmd;

	switch (cmd) {
	case GET_TRACK:
	{
		quint32 length;
		ds >> length;
		Q_ASSERT(1 + sizeof(length) + length == size_t(data.length()));
		QByteArray nameData(data.constData() + 1 + sizeof(length), length);
		requestTrack(QString::fromUtf8(nameData));
	}
	break;

	case SET_ROW:
	{
		quint32 row;
		ds >> row;
		emit rowChanged(row);
	}
	break;
	}
}

#endif // defined(QT_WEBSOCKETS_LIB)
