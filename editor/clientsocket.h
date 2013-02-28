#include "../sync/base.h"
#include <map>

class TcpSocket {
public:
	TcpSocket() : socket(INVALID_SOCKET) {}
	explicit TcpSocket(SOCKET socket) : socket(socket) {}

	bool connected() const
	{
		return INVALID_SOCKET != socket;
	}

	virtual void disconnect()
	{
		closesocket(socket);
		socket = INVALID_SOCKET;
	}

	virtual bool recv(char *buffer, size_t length)
	{
		if (!connected())
			return false;
		int ret = ::recv(socket, buffer, int(length), 0);
		if (ret != int(length)) {
			TcpSocket::disconnect();
			return false;
		}
		return true;
	}

	virtual bool send(const char *buffer, size_t length, bool endOfMessage)
	{
		(void)endOfMessage;
		if (!connected())
			return false;
		int ret = ::send(socket, buffer, int(length), 0);
		if (ret != int(length)) {
			TcpSocket::disconnect();
			return false;
		}
		return true;
	}

	virtual bool pollRead()
	{
		if (!connected())
			return false;
		return !!socket_poll(socket);
	}

private:
	SOCKET socket;
};

class WebSocket : public TcpSocket {
public:
	explicit WebSocket(SOCKET socket) : TcpSocket(socket), firstFrame(true) {}

	bool recv(char *buffer, size_t length);
	bool send(const char *buffer, size_t length, bool endOfMessage)
	{
		return sendFrame(firstFrame ? 2 : 0, buffer, length, endOfMessage);
	}


	virtual void disconnect()
	{
		sendFrame(8, NULL, 0, true);
		TcpSocket::disconnect();
	}

	bool pollRead()
	{
		if (buf.length() > 0)
			return true;
		return TcpSocket::pollRead();
	}

	// helpers
	bool readFrame(std::string &buf);
	bool sendFrame(int opcode, const char *payloadData, size_t payloadLength, bool endOfMessage);
	static WebSocket *upgradeFromHttp(SOCKET socket);

private:
	bool firstFrame;
	std::string buf;
};

class ClientSocket {
public:
	ClientSocket() : socket(NULL), clientPaused(true) {}

	bool connected() const
	{
		if (!socket)
			return false;
		return socket->connected();
	}

	void disconnect()
	{
		if (socket)
			socket->disconnect();
		clientTracks.clear();
	}

	bool recv(char *buffer, size_t length)
	{
		if (!socket)
			return false;
		return socket->recv(buffer, length);
	}

	bool send(const char *buffer, size_t length, bool endOfMessage)
	{
		if (!socket)
			return false;
		return socket->send(buffer, length, endOfMessage);
	}

	bool pollRead()
	{
		if (!connected())
			return false;
		return socket->pollRead();
	}

	void sendSetKeyCommand(const std::string &trackName, const struct track_key &key);
	void sendDeleteKeyCommand(const std::string &trackName, int row);
	void sendSetRowCommand(int row);
	void sendPauseCommand(bool pause);
	void sendSaveCommand();

	bool clientPaused;
	std::map<const std::string, size_t> clientTracks;
	TcpSocket *socket;
};
