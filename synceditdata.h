#pragma once

#include "syncdata.h"
#include <stack>
#include <list>

class SyncEditData : public SyncData
{
public:
	SyncEditData() : SyncData() {}
	
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
		InsertCommand(size_t track, size_t row, const SyncTrack::KeyFrame &key) : track(track), row(row), key(key) {}
		~InsertCommand() {}
		
		virtual void exec(SyncEditData *data)
		{
			SyncTrack &track = data->getTrack(this->track);
			assert(!track.isKeyFrame(row));
			track.setKeyFrame(row, key);
		}
		
		virtual void undo(SyncEditData *data)
		{
			SyncTrack &track = data->getTrack(this->track);
			assert(track.isKeyFrame(row));
			track.deleteKeyFrame(row);
		}
		
	private:
		size_t track, row;
		SyncTrack::KeyFrame key;
	};
	
	class DeleteCommand : public Command
	{
	public:
		DeleteCommand(size_t track, size_t row) : track(track), row(row) {}
		~DeleteCommand() {}
		
		virtual void exec(SyncEditData *data)
		{
			SyncTrack &track = data->getTrack(this->track);
			assert(track.isKeyFrame(row));
			oldKey = *track.getKeyFrame(row);
			track.deleteKeyFrame(row);
		}
		
		virtual void undo(SyncEditData *data)
		{
			SyncTrack &track = data->getTrack(this->track);
			assert(!track.isKeyFrame(row));
			track.setKeyFrame(row, oldKey);
		}
		
	private:
		size_t track, row;
		SyncTrack::KeyFrame oldKey;
	};

	
	class EditCommand : public Command
	{
	public:
		EditCommand(size_t track, size_t row, const SyncTrack::KeyFrame &key) : track(track), row(row), key(key) {}
		~EditCommand() {}
		
		virtual void exec(SyncEditData *data)
		{
			SyncTrack &track = data->getTrack(this->track);
			
			// store old key
			assert(track.isKeyFrame(row));
			oldKey = *track.getKeyFrame(row);
			
			// update
			track.setKeyFrame(row, key);
		}
		
		virtual void undo(SyncEditData *data)
		{
			SyncTrack &track = data->getTrack(this->track);
			
			assert(track.isKeyFrame(row));
			track.setKeyFrame(row, oldKey);
		}
		
	private:
		size_t track, row;
		SyncTrack::KeyFrame oldKey, key;
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
	
	void setKeyFrame(int track, int row, const SyncTrack::KeyFrame &key)
	{
		SyncTrack &t = getTrack(track);
		SyncEditData::Command *cmd;
		if (t.isKeyFrame(row))
		{
			cmd = new EditCommand(track, row, key);
		}
		else
		{
			cmd = new InsertCommand(track, row, key);
		}
		exec(cmd);
	}
	
	void deleteKeyFrame(int track, int row)
	{
		assert(getTrack(track).isKeyFrame(row));
		Command *cmd = new DeleteCommand(track, row);
		exec(cmd);
	}
	
private:
	
	std::stack<Command*> undoStack;
	std::stack<Command*> redoStack;
};
