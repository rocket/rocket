/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in COPYING
 */

#pragma once

extern "C" {
#include "../sync/data.h"
}

#include <stack>
#include <list>
#include <vector>
#include <set>
#include <string>
#include <cassert>

#include "clientsocket.h"

class SyncDocument : public sync_data {
public:
	SyncDocument() :
	    rows(128), savePointDelta(0), savePointUnreachable(false)
	{
		this->tracks = NULL;
		this->num_tracks = 0;
	}

	~SyncDocument();

	size_t createTrack(const std::string &name)
	{
		size_t index = sync_create_track(this, name.c_str());
		trackOrder.push_back(index);
		return index;
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
		InsertCommand(int track, const track_key &key) : track(track), key(key) {}
		~InsertCommand() {}
		
		void exec(SyncDocument *data)
		{
			sync_track *t = data->tracks[track];
			assert(!is_key_frame(t, key.row));
			if (sync_set_key(t, &key))
				throw std::bad_alloc("sync_set_key");
			data->clientSocket.sendSetKeyCommand(t->name, key); // update clients
		}
		
		void undo(SyncDocument *data)
		{
			sync_track *t = data->tracks[track];
			assert(is_key_frame(t, key.row));
			if (sync_del_key(t, key.row))
				throw std::bad_alloc("sync_del_key");
			data->clientSocket.sendDeleteKeyCommand(t->name, key.row); // update clients
		}

	private:
		int track;
		track_key key;
	};
	
	class DeleteCommand : public Command
	{
	public:
		DeleteCommand(int track, int row) : track(track), row(row) {}
		~DeleteCommand() {}
		
		void exec(SyncDocument *data)
		{
			sync_track *t = data->tracks[track];
			int idx = sync_find_key(t, row);
			assert(idx >= 0);
			oldKey = t->keys[idx];
			if (sync_del_key(t, row))
				throw std::bad_alloc("sync_del_key");
			data->clientSocket.sendDeleteKeyCommand(t->name, row); // update clients
		}
		
		void undo(SyncDocument *data)
		{
			sync_track *t = data->tracks[track];
			assert(!is_key_frame(t, row));
			if (sync_set_key(t, &oldKey))
				throw std::bad_alloc("sync_set_key");
			data->clientSocket.sendSetKeyCommand(t->name, oldKey); // update clients
		}

	private:
		int track, row;
		struct track_key oldKey;
	};

	
	class EditCommand : public Command
	{
	public:
		EditCommand(int track, const track_key &key) : track(track), key(key) {}
		~EditCommand() {}

		void exec(SyncDocument *data)
		{
			sync_track *t = data->tracks[track];
			int idx = sync_find_key(t, key.row);
			assert(idx >= 0);
			oldKey = t->keys[idx];
			if (sync_set_key(t, &key))
				throw std::bad_alloc("sync_set_key");
			data->clientSocket.sendSetKeyCommand(t->name, key); // update clients
		}

		void undo(SyncDocument *data)
		{
			sync_track *t = data->tracks[track];
			assert(is_key_frame(t, key.row));
			if (sync_set_key(t, &oldKey))
				throw std::bad_alloc("sync_set_key");
			data->clientSocket.sendSetKeyCommand(t->name, oldKey); // update clients
		}
		
	private:
		int track;
		track_key oldKey, key;
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
	
	Command *getSetKeyFrameCommand(int track, const track_key &key)
	{
		sync_track *t = tracks[track];
		SyncDocument::Command *cmd;
		if (is_key_frame(t, key.row)) cmd = new EditCommand(track, key);
		else                          cmd = new InsertCommand(track, key);
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

	static SyncDocument *load(const std::wstring &fileName);
	bool save(const std::wstring &fileName);

	bool modified() const
	{
		if (savePointUnreachable) return true;
		return 0 != savePointDelta;
	}

	bool isRowBookmark(int row) const
	{
		return !!rowBookmarks.count(row);
	}

	void toggleRowBookmark(int row)
	{
		if (isRowBookmark(row))
			rowBookmarks.erase(row);
		else
			rowBookmarks.insert(row);
	}

	ClientSocket clientSocket;

	size_t getRows() const { return rows; }
	void setRows(size_t rows) { this->rows = rows; }

	std::wstring fileName;

private:
	std::set<int> rowBookmarks;
	std::vector<size_t> trackOrder;
	size_t rows;

	// undo / redo functionality
	std::stack<Command*> undoStack;
	std::stack<Command*> redoStack;
	int savePointDelta;        // how many undos must be done to get to the last saved state
	bool savePointUnreachable; // is the save-point reachable?

};
