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

	void disconnect()
	{
		closesocket(socket);
		socket = INVALID_SOCKET;
	}

	bool recv(char *buffer, size_t length, int flags)
	{
		if (!connected())
			return false;
		int ret = ::recv(socket, buffer, int(length), flags);
		if (ret != int(length)) {
			disconnect();
			return false;
		}
		return true;
	}

	bool send(const char *buffer, size_t length, int flags)
	{
		if (!connected())
			return false;
		int ret = ::send(socket, buffer, int(length), flags);
		if (ret != int(length)) {
			disconnect();
			return false;
		}
		return true;
	}

	bool pollRead()
	{
		if (!connected())
			return false;
		return !!socket_poll(socket);
	}

private:
	SOCKET socket;
};

class ClientSocket {
public:
	ClientSocket() : socket(INVALID_SOCKET) {}
	explicit ClientSocket(SOCKET socket) : socket(socket), clientPaused(true) {}

	bool connected() const
	{
		return socket.connected();
	}

	void disconnect()
	{
		socket.disconnect();
		clientTracks.clear();
	}

	bool recv(char *buffer, size_t length, int flags)
	{
		return socket.recv(buffer, length, flags);
	}

	bool send(const char *buffer, size_t length, int flags)
	{
		return socket.send(buffer, length, flags);
	}

	bool pollRead()
	{
		if (!connected())
			return false;
		return socket.pollRead();
	}

	void sendSetKeyCommand(const std::string &trackName, const struct track_key &key);
	void sendDeleteKeyCommand(const std::string &trackName, int row);
	void sendSetRowCommand(int row);
	void sendPauseCommand(bool pause);
	void sendSaveCommand();

	bool clientPaused;
	std::map<const std::string, size_t> clientTracks;

private:
	TcpSocket socket;
};
