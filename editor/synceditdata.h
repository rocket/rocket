#pragma once

#include "../sync/network.h"
#include "../sync/data.h"
#include <stack>
#include <list>

class SyncEditData : public sync::Data
{
public:
	SyncEditData() : sync::Data(), clientPaused(true) {}
	
	void sendSetKeyCommand(int track, int row, const sync::Track::KeyFrame &key)
	{
		if (INVALID_SOCKET == clientSocket) return;
		if (clientRemap.count(track) == 0) return;
		track = int(clientRemap[track]);
		
		unsigned char cmd = SET_KEY;
		send(clientSocket, (char*)&cmd, 1, 0);
		send(clientSocket, (char*)&track, sizeof(int), 0);
		send(clientSocket, (char*)&row,   sizeof(int), 0);
		send(clientSocket, (char*)&key.value, sizeof(float), 0);
		send(clientSocket, (char*)&key.interpolationType, 1, 0);
	}
	
	void sendDeleteKeyCommand(int track, int row)
	{
		if (INVALID_SOCKET == clientSocket) return;
		
		unsigned char cmd = DELETE_KEY;
		send(clientSocket, (char*)&cmd, 1, 0);
		send(clientSocket, (char*)&track, sizeof(int), 0);
		send(clientSocket, (char*)&row,   sizeof(int), 0);
	}
	
	void sendSetRowCommand(int row)
	{
		if (INVALID_SOCKET == clientSocket) return;
		printf("sending row pos\n");
		
		unsigned char cmd = SET_ROW;
		send(clientSocket, (char*)&cmd, 1, 0);
		send(clientSocket, (char*)&row,   sizeof(int), 0);
	}

	void sendPauseCommand(bool pause)
	{
		unsigned char cmd = PAUSE;
		send(clientSocket, (char*)&cmd, 1, 0);
		unsigned char flag = pause;
		send(clientSocket, (char*)&flag, 1, 0);
		clientPaused = pause;
	}

	
	class Command
	{
	public:
		virtual ~Command() {}
		virtual void exec(SyncEditData *data) = 0;
		virtual void undo(SyncEditData *data) = 0;
	};
	
	class InsertCommand : public Command
	{
	public:
		InsertCommand(int track, int row, const sync::Track::KeyFrame &key) : track(track), row(row), key(key) {}
		~InsertCommand() {}
		
		virtual void exec(SyncEditData *data)
		{
			sync::Track &t = data->getTrack(this->track);
			assert(!t.isKeyFrame(row));
			t.setKeyFrame(row, key);
			
			data->sendSetKeyCommand(track, row, key); // update clients
		}
		
		virtual void undo(SyncEditData *data)
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
		
		virtual void exec(SyncEditData *data)
		{
			sync::Track &t = data->getTrack(this->track);
			assert(t.isKeyFrame(row));
			oldKey = *t.getKeyFrame(row);
			t.deleteKeyFrame(row);
			
			data->sendDeleteKeyCommand(track, row); // update clients
		}
		
		virtual void undo(SyncEditData *data)
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
		
		virtual void exec(SyncEditData *data)
		{
			sync::Track &t = data->getTrack(this->track);
			
			// store old key
			assert(t.isKeyFrame(row));
			oldKey = *t.getKeyFrame(row);
			
			// update
			t.setKeyFrame(row, key);
			
			data->sendSetKeyCommand(track, row, key); // update clients
		}
		
		virtual void undo(SyncEditData *data)
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
		
		virtual void exec(SyncEditData *data)
		{
			std::list<Command*>::iterator it;
			for (it = commands.begin(); it != commands.end(); ++it) (*it)->exec(data);
		}
		
		virtual void undo(SyncEditData *data)
		{
			std::list<Command*>::iterator it;
			for (it = commands.begin(); it != commands.end(); ++it) (*it)->undo(data);
		}
		
	private:
		std::list<Command*> commands;
	};
	
	void exec(Command *cmd)
	{
		undoStack.push(cmd);
		cmd->exec(this);
		clearRedoStack();
	}
	
	bool undo()
	{
		if (undoStack.size() == 0) return false;
		
		Command *cmd = undoStack.top();
		undoStack.pop();
		
		redoStack.push(cmd);
		cmd->undo(this);
		return true;
	}
	
	bool redo()
	{
		if (redoStack.size() == 0) return false;
		
		Command *cmd = redoStack.top();
		redoStack.pop();
		
		undoStack.push(cmd);
		cmd->exec(this);
		return true;
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
		SyncEditData::Command *cmd;
		if (t.isKeyFrame(row)) cmd = new EditCommand(track, row, key);
		else                   cmd = new InsertCommand(track, row, key);
		return cmd;
	}
	
	SOCKET clientSocket;
// private:
	std::map<size_t, size_t> clientRemap;
	bool clientPaused;
	
	std::stack<Command*> undoStack;
	std::stack<Command*> redoStack;
};
