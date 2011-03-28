#include "clientsocket.h"
#include "../sync/track.h"

#include <cassert>
#include <string>

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
	send((char *)&cmd, 1, 0);
	send((char *)&track, sizeof(track), 0);
	send((char *)&row, sizeof(row), 0);
	send((char *)&v.i, sizeof(v.i), 0);
	send((char *)&key.type, 1, 0);
}

void ClientSocket::sendDeleteKeyCommand(const std::string &trackName, int row)
{
	if (!connected() ||
	    clientTracks.count(trackName) == 0)
		return;

	uint32_t track = htonl(int(clientTracks[trackName]));
	row = htonl(row);

	unsigned char cmd = DELETE_KEY;
	send((char *)&cmd, 1, 0);
	send((char *)&track, sizeof(int), 0);
	send((char *)&row,   sizeof(int), 0);
}

void ClientSocket::sendSetRowCommand(int row)
{
	if (!connected())
		return;

	unsigned char cmd = SET_ROW;
	row = htonl(row);
	send((char *)&cmd, 1, 0);
	send((char *)&row, sizeof(int), 0);
}

void ClientSocket::sendPauseCommand(bool pause)
{
	if (!connected())
		return;

	unsigned char cmd = PAUSE, flag = pause;
	send((char *)&cmd, 1, 0);
	send((char *)&flag, 1, 0);
	clientPaused = pause;
}

void ClientSocket::sendSaveCommand()
{
	if (!connected())
		return;

	unsigned char cmd = SAVE_TRACKS;
	send((char *)&cmd, 1, 0);
}
