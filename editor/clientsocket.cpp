#include "clientsocket.h"
extern "C" {
#include "../lib/track.h"
}
#include <QCryptographicHash>
#include <QtEndian>

#include <cassert>
#include <string>

bool WebSocket::readFrame(QByteArray &buf)
{
	unsigned char header[2];
	if (!TcpSocket::recv((char *)header, 2))
		return false;

	// int flags = header[0] >> 4;
	int opcode = header[0] & 0xF;
	int masked = header[1] >> 7;
	int payload_len = header[1] & 0x7f;

	if (payload_len == 126) {
		quint16 tmp;
		if (!TcpSocket::recv((char *)&tmp, 2))
			return false;
		payload_len = qFromBigEndian(tmp);
	} else if (payload_len == 127) {
		// dude, that's one crazy big payload! let's bail!
		return false;
	}

	unsigned char mask[4] = { 0 };
	if (masked) {
		if (!TcpSocket::recv((char *)mask, sizeof(mask)))
			return false;
	}

	buf.resize(payload_len);
	if (payload_len > 0) {
		if (!TcpSocket::recv(buf.data(), payload_len))
			return false;
	}

	for (int i = 0; i < payload_len; ++i)
		buf[i] = buf[i] ^ mask[i & 3];

	switch (opcode) {
	case 9:
		// got ping, send pong!
		sendFrame(10, buf.data(), buf.length(), true);
		buf.clear();
		return true;

	case 8:
		// close
		disconnect();
		buf.clear();
		return false;
	}

	return true;
}

bool WebSocket::recv(char *buffer, int length)
{
	if (!connected())
		return false;
	while (length) {
		while (!buf.length() && !readFrame(buf))
			return false;

		int bytes = qMin(buf.length(), length);
		memcpy(buffer, buf.data(), bytes);
		buf.remove(0, bytes);
		buffer += bytes;
		length -= bytes;
	}
	return true;
}

bool WebSocket::sendFrame(int opcode, const char *payloadData, size_t payloadLength, bool endOfMessage)
{
	unsigned char header[2];
	header[0] = (endOfMessage ? 0x80 : 0) | (unsigned char)opcode;
	header[1] = payloadLength < 126 ? (unsigned char)(payloadLength) : 126;
	if (!TcpSocket::send((const char *)header, 2, false))
		return false;

	if (payloadLength >= 126) {
		assert(payloadLength < 0xffff);
		quint16 tmp = qToBigEndian((quint16)(payloadLength));
		if (!TcpSocket::send((const char *)&tmp, 2, false))
			return false;
	}

	firstFrame = endOfMessage;
	return TcpSocket::send(payloadData, payloadLength, endOfMessage);
}

WebSocket *WebSocket::upgradeFromHttp(QTcpSocket *socket)
{
	std::string key;
	for (;;) {
		std::string line;
		for (;;) {
			char ch;
			if (socket->read(&ch, 1) != 1)
				return NULL;

			if (ch == '\n')
				break;
			if (ch != '\r')
				line.push_back(ch);
		}

		if (!line.compare(0, 19, "Sec-WebSocket-Key: "))
			key = line.substr(19);
		else if (!line.length())
			break;
	}

	if (!key.length())
		return NULL;

	std::string accept = key;
	accept.append("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

	QCryptographicHash hash(QCryptographicHash::Sha1);
	hash.addData(&accept[0], accept.length());

	std::string response = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ";
	response.append(hash.result().toBase64().constData());
	response.append("\r\n\r\n");

	socket->write(&response[0], response.length());

	return new WebSocket(socket);
}


void ClientSocket::sendSetKeyCommand(const std::string &trackName, const struct track_key &key)
{
	if (!connected() ||
	    clientTracks.count(trackName) == 0)
		return;
	quint32 track = qToBigEndian((quint32)clientTracks[trackName]);
	quint32 row = qToBigEndian((quint32)key.row);

	union {
		float f;
		quint32 i;
	} v;
	v.f = key.value;
	v.i = qToBigEndian(v.i);

	assert(key.type < KEY_TYPE_COUNT);

	unsigned char cmd = SET_KEY;
	send((char *)&cmd, 1, false);
	send((char *)&track, sizeof(track), false);
	send((char *)&row, sizeof(row), false);
	send((char *)&v.i, sizeof(v.i), false);
	send((char *)&key.type, 1, true);
}

void ClientSocket::sendDeleteKeyCommand(const std::string &trackName, int row)
{
	if (!connected() ||
	    clientTracks.count(trackName) == 0)
		return;

	quint32 track = qToBigEndian(int(clientTracks[trackName]));
	row = qToBigEndian((quint32)row);

	unsigned char cmd = DELETE_KEY;
	send((char *)&cmd, 1, false);
	send((char *)&track, sizeof(int), false);
	send((char *)&row,   sizeof(int), true);
}

void ClientSocket::sendSetRowCommand(int row)
{
	if (!connected())
		return;

	unsigned char cmd = SET_ROW;
	row = qToBigEndian((quint32)row);
	send((char *)&cmd, 1, false);
	send((char *)&row, sizeof(int), true);
}

void ClientSocket::sendPauseCommand(bool pause)
{
	if (!connected())
		return;

	unsigned char cmd = PAUSE, flag = pause;
	send((char *)&cmd, 1, false);
	send((char *)&flag, 1, true);
	clientPaused = pause;
}

void ClientSocket::sendSaveCommand()
{
	if (!connected())
		return;

	unsigned char cmd = SAVE_TRACKS;
	send((char *)&cmd, 1, true);
}
