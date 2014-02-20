#include "clientsocket.h"
#include "../lib/track.h"
#include <wincrypt.h>

#include <cassert>
#include <string>

bool WebSocket::readFrame(std::string &buf)
{
	unsigned char header[2];
	if (!TcpSocket::recv((char *)header, 2))
		return false;

	// int flags = header[0] >> 4;
	int opcode = header[0] & 0xF;
	int masked = header[1] >> 7;
	int payload_len = header[1] & 0x7f;

	if (payload_len == 126) {
		unsigned short tmp;
		if (!TcpSocket::recv((char *)&tmp, 2))
			return false;
		payload_len = ntohs(tmp);
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
		if (!TcpSocket::recv(&buf[0], payload_len))
			return false;
	}

	for (int i = 0; i < payload_len; ++i)
		buf[i] = buf[i] ^ mask[i & 3];

	switch (opcode) {
	case 9:
		// got ping, send pong!
		sendFrame(10, &buf[0], buf.length(), true);
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

bool WebSocket::recv(char *buffer, size_t length)
{
	if (!connected())
		return false;
	while (length) {
		while (!buf.length() && !readFrame(buf))
			return false;

		int bytes = std::min(buf.length(), length);
		memcpy(buffer, &buf[0], bytes);
		buf = buf.substr(bytes);
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
		unsigned short tmp = htons(unsigned short(payloadLength));
		if (!TcpSocket::send((const char *)&tmp, 2, false))
			return false;
	}

	firstFrame = endOfMessage;
	return TcpSocket::send(payloadData, payloadLength, endOfMessage);
}

static BOOL MyCryptBinaryToStringA(const BYTE* pbBinary, DWORD cbBinary, DWORD dwFlags,
    LPTSTR pszString, DWORD* pcchString)
{
	typedef BOOL (WINAPI *T)(const BYTE* pbBinary, DWORD cbBinary, DWORD dwFlags, LPTSTR pszString, DWORD* pcchString);
	static T CryptBinaryToStringA;
	if (!CryptBinaryToStringA)
		CryptBinaryToStringA = (T)GetProcAddress(LoadLibrary("crypt32"),
		    "CryptBinaryToStringA");
	return CryptBinaryToStringA(pbBinary, cbBinary, dwFlags, pszString, pcchString);
}

WebSocket *WebSocket::upgradeFromHttp(SOCKET socket)
{
	std::string key;
	for (;;) {
		std::string line;
		for (;;) {
			char ch;
			if (::recv(socket, &ch, 1, 0) != 1)
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

	HCRYPTPROV prov;
	HCRYPTHASH hash;
	unsigned char sha1_buf[21];
	DWORD sha1_len = 20;
	char base64_buf[31];
	DWORD base64_len = 31;
	if ((!CryptAcquireContext(&prov, NULL, NULL, PROV_RSA_FULL, 0) &&
	    !CryptAcquireContext(&prov, NULL, NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET)) ||
	    !CryptCreateHash(prov, CALG_SHA1, 0, 0, &hash) ||
	    !CryptHashData(hash, (const BYTE *)&accept[0], accept.length(), 0) ||
	    !CryptGetHashParam(hash, HP_HASHVAL, sha1_buf, &sha1_len, 0) ||
	    !MyCryptBinaryToStringA(sha1_buf, sha1_len, CRYPT_STRING_BASE64, base64_buf, &base64_len))
		return NULL;

	std::string response = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ";
	response.append(base64_buf);
	response.append("\r\n");

	::send(socket, &response[0], response.length(), 0);

	return new WebSocket(socket);
}


void ClientSocket::sendSetKeyCommand(const std::string &trackName, const struct track_key &key)
{
	if (!connected() ||
	    clientTracks.count(trackName) == 0)
		return;
	uint32_t track = htonl(clientTracks[trackName]);
	uint32_t row = htonl(key.row);

	union {
		float f;
		uint32_t i;
	} v;
	v.f = key.value;
	v.i = htonl(v.i);

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

	uint32_t track = htonl(int(clientTracks[trackName]));
	row = htonl(row);

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
	row = htonl(row);
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
