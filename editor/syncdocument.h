/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#pragma once

#include "../sync/network.h"
#include "../sync/data.h"
#include "inifile.h"
#include <stack>
#include <list>
#include <vector>
#include <map>

class SyncDocument : public sync::Data
{
public:
	SyncDocument() : sync::Data(), clientPaused(true), rows(128), savePointDelta(0), savePointUnreachable(true) {}
	~SyncDocument();

	size_t createTrack(const std::basic_string<TCHAR> &name)
	{
		size_t index = sync::Data::createTrack(name);
		trackOrder.push_back(index);
		return index;
	}
	
	void sendSetKeyCommand(int track, int row, const sync::Track::KeyFrame &key)
	{
		if (!clientSocket.connected()) return;
		if (clientRemap.count(track) == 0) return;
		track = int(clientRemap[track]);
		
		unsigned char cmd = SET_KEY;
		clientSocket.send((char*)&cmd, 1, 0);
		clientSocket.send((char*)&track, sizeof(int), 0);
		clientSocket.send((char*)&row,   sizeof(int), 0);
		clientSocket.send((char*)&key.value, sizeof(float), 0);
		clientSocket.send((char*)&key.interpolationType, 1, 0);
	}
	
	void sendDeleteKeyCommand(int track, int row)
	{
		if (!clientSocket.connected()) return;
		if (clientRemap.count(track) == 0) return;
		track = int(clientRemap[track]);
		
		unsigned char cmd = DELETE_KEY;
		clientSocket.send((char*)&cmd, 1, 0);
		clientSocket.send((char*)&track, sizeof(int), 0);
		clientSocket.send((char*)&row,   sizeof(int), 0);
	}
	
	void sendSetRowCommand(int row)
	{
		if (!clientSocket.connected()) return;
		unsigned char cmd = SET_ROW;
		clientSocket.send((char*)&cmd, 1, 0);
		clientSocket.send((char*)&row,   sizeof(int), 0);
	}
	
	void sendPauseCommand(bool pause)
	{
		if (!clientSocket.connected()) return;
		unsigned char cmd = PAUSE;
		clientSocket.send((char*)&cmd, 1, 0);
		unsigned char flag = pause;
		clientSocket.send((char*)&flag, 1, 0);
		clientPaused = pause;
	}
	
	void sendSaveCommand()
	{
		if (!clientSocket.connected()) return;
		unsigned char cmd = SAVE_TRACKS;
		clientSocket.send((char*)&cmd, 1, 0);
	}
	
	class Command
	{
	public:
		virtual ~Command() {}
		virtual void exec(SyncDocument *data) = 0;
		virtual void undo(SyncDocument *data) = 0;
	};
	
	class InsertCommand : public Command
	{
	public:
		InsertCommand(int track, int row, const sync::Track::KeyFrame &key) : track(track), row(row), key(key) {}
		~InsertCommand() {}
		
		void exec(SyncDocument *data)
		{
			sync::Track &t = data->getTrack(this->track);
			assert(!t.isKeyFrame(row));
			t.setKeyFrame(row, key);
			data->sendSetKeyCommand(track, row, key); // update clients
		}
		
		void undo(SyncDocument *data)
		{
			sync::Track &t = data->getTrack(this->track);
			assert(t.isKeyFrame(row));
			t.deleteKeyFrame(row);
			
			data->sendDeleteKeyCommand(track, row); // update clients
		}
		
	private:
		int track, row;
		sync::Track::KeyFrame key;
	};
	
	class DeleteCommand : public Command
	{
	public:
		DeleteCommand(int track, int row) : track(track), row(row) {}
		~DeleteCommand() {}
		
		void exec(SyncDocument *data)
		{
			sync::Track &t = data->getTrack(this->track);
			assert(t.isKeyFrame(row));
			oldKey = *t.getKeyFrame(row);
			t.deleteKeyFrame(row);
			
			data->sendDeleteKeyCommand(track, row); // update clients
		}
		
		void undo(SyncDocument *data)
		{
			sync::Track &t = data->getTrack(this->track);
			assert(!t.isKeyFrame(row));
			t.setKeyFrame(row, oldKey);
			
			data->sendSetKeyCommand(track, row, oldKey); // update clients
		}
		
	private:
		int track, row;
		sync::Track::KeyFrame oldKey;
	};

	
	class EditCommand : public Command
	{
	public:
		EditCommand(int track, int row, const sync::Track::KeyFrame &key) : track(track), row(row), key(key) {}
		~EditCommand() {}
		
		void exec(SyncDocument *data)
		{
			sync::Track &t = data->getTrack(this->track);
			
			// store old key
			assert(t.isKeyFrame(row));
			oldKey = *t.getKeyFrame(row);
			
			// update
			t.setKeyFrame(row, key);
			
			data->sendSetKeyCommand(track, row, key); // update clients
		}
		
		void undo(SyncDocument *data)
		{
			sync::Track &t = data->getTrack(this->track);
			
			assert(t.isKeyFrame(row));
			t.setKeyFrame(row, oldKey);
			
			data->sendSetKeyCommand(track, row, oldKey); // update clients
		}
		
	private:
		int track, row;
		sync::Track::KeyFrame oldKey, key;
	};
	
	class MultiCommand : public Command
	{
	public:
		~MultiCommand()
		{
			std::list<Command*>::iterator it;
			for (it = commands.begin(); it != commands.end(); ++it)
			{
				delete *it;
			}
			commands.clear();
		}
		
		void addCommand(Command *cmd)
		{
			commands.push_back(cmd);
		}
		
		size_t getSize() const { return commands.size(); }
		
		void exec(SyncDocument *data)
		{
			std::list<Command*>::iterator it;
			for (it = commands.begin(); it != commands.end(); ++it) (*it)->exec(data);
		}
		
		void undo(SyncDocument *data)
		{
			std::list<Command*>::reverse_iterator it;
			for (it = commands.rbegin(); it != commands.rend(); ++it) (*it)->undo(data);
		}
		
	private:
		std::list<Command*> commands;
	};
	
	void exec(Command *cmd)
	{
		undoStack.push(cmd);
		cmd->exec(this);
		clearRedoStack();

		if (savePointDelta < 0) savePointUnreachable = true;
		savePointDelta++;
	}
	
	bool undo()
	{
		if (undoStack.size() == 0) return false;
		
		Command *cmd = undoStack.top();
		undoStack.pop();
		
		redoStack.push(cmd);
		cmd->undo(this);
		
		savePointDelta--;
		
		return true;
	}
	
	bool redo()
	{
		if (redoStack.size() == 0) return false;
		
		Command *cmd = redoStack.top();
		redoStack.pop();
		
		undoStack.push(cmd);
		cmd->exec(this);

		savePointDelta++;

		return true;
	}
	
	void clearUndoStack()
	{
		while (!undoStack.empty())
		{
			Command *cmd = undoStack.top();
			undoStack.pop();
			delete cmd;
		}
	}
	
	void clearRedoStack()
	{
		while (!redoStack.empty())
		{
			Command *cmd = redoStack.top();
			redoStack.pop();
			delete cmd;
		}
	}
	
	Command *getSetKeyFrameCommand(int track, int row, const sync::Track::KeyFrame &key)
	{
		sync::Track &t = getTrack(track);
		SyncDocument::Command *cmd;
		if (t.isKeyFrame(row)) cmd = new EditCommand(track, row, key);
		else                   cmd = new InsertCommand(track, row, key);
		return cmd;
	}

	size_t getTrackOrderCount() const
	{
		return trackOrder.size();
	}
	
	size_t getTrackIndexFromPos(size_t track) const
	{
		assert(track < trackOrder.size());
		return trackOrder[track];
	}

	void swapTrackOrder(size_t t1, size_t t2)
	{
		assert(t1 < trackOrder.size());
		assert(t2 < trackOrder.size());
		std::swap(trackOrder[t1], trackOrder[t2]);
	}
	
	bool load(const std::string &fileName);
	bool save(const std::string &fileName);

	bool modified()
	{
		if (savePointUnreachable) return true;
		return 0 != savePointDelta;
	}
	
	NetworkSocket clientSocket;
	std::map<size_t, size_t> clientRemap;
	bool clientPaused;

	size_t getRows() const { return rows; }
	void setRows(size_t rows) { this->rows = rows; }
	
private:
	std::vector<size_t> trackOrder;
	size_t rows;
	
	// undo / redo functionality
	std::stack<Command*> undoStack;
	std::stack<Command*> redoStack;
	int savePointDelta;        // how many undos must be done to get to the last saved state
	bool savePointUnreachable; // is the save-point reachable?

};
