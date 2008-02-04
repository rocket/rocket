#pragma once

#include "syncdata.h"
#include <stack>

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
	
	class EditCommand : public Command
	{
	public:
		EditCommand(int track, int row, bool existing, float value) : track(track), row(row), newValExisting(existing), newVal(value) {}
		~EditCommand() {}
		
		virtual void exec(SyncEditData *data)
		{
			SyncTrack &track = data->getTrack(this->track);

			// store old state
			oldValExisting = track.isKeyFrame(row);
			if (oldValExisting) oldVal = track.getKeyFrame(row)->value;

			// update
			if (!newValExisting) track.deleteKeyFrame(row);
			else track.setKeyFrame(row, newVal);
		}
		
		virtual void undo(SyncEditData *data)
		{
			SyncTrack &track = data->getTrack(this->track);

			// un-update
			if (!oldValExisting) track.deleteKeyFrame(row);
			else track.setKeyFrame(row, oldVal);
		}
		
	private:
		int track, row;
		float newVal, oldVal;
		bool newValExisting, oldValExisting;
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

private:
	
	std::stack<Command*> undoStack;
	std::stack<Command*> redoStack;
};
