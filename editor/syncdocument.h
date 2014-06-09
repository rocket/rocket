#ifndef SYNCDOCUMENT_H
#define SYNCDOCUMENT_H

#include <QStack>
#include <QList>
#include <QVector>
#include <QString>

#include "clientsocket.h"
#include "synctrack.h"

class SyncDocument {
public:
	SyncDocument() :
	    rows(128),
	    savePointDelta(0),
	    savePointUnreachable(false)
	{
	}

	~SyncDocument();

	int createTrack(const QString &name)
	{
		tracks.append(new SyncTrack(name));
		int index = tracks.size() - 1;
		trackOrder.push_back(index);
		Q_ASSERT(trackOrder.size() == tracks.size());
		return index;
	}

	SyncTrack *getTrack(int index)
	{
		Q_ASSERT(index >= 0 && index < tracks.size());
		return tracks[index];
	}

	const SyncTrack *getTrack(int index) const
	{
		Q_ASSERT(index >= 0 && index < tracks.size());
		return tracks[index];
	}

	SyncTrack *findTrack(const QString &name)
	{
		for (int i = 0; i < tracks.size(); ++i)
			if (name == tracks[i]->name)
				return tracks[i];
		return NULL;
	}

	size_t getTrackCount() const
	{
		Q_ASSERT(trackOrder.size() == tracks.size());
		return tracks.size();
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
		InsertCommand(int track, const SyncTrack::TrackKey &key) :
		    track(track),
		    key(key)
		{}

		~InsertCommand() {}
		
		void exec(SyncDocument *data)
		{
			SyncTrack *t = data->getTrack(track);
			Q_ASSERT(!t->isKeyFrame(key.row));
			t->setKey(key);
			data->clientSocket.sendSetKeyCommand(t->name.toUtf8().constData(), key); // update clients
		}
		
		void undo(SyncDocument *data)
		{
			SyncTrack *t = data->getTrack(track);
			Q_ASSERT(t->isKeyFrame(key.row));
			t->removeKey(key.row);
			data->clientSocket.sendDeleteKeyCommand(t->name.toUtf8().constData(), key.row); // update clients
		}

	private:
		int track;
		SyncTrack::TrackKey key;
	};
	
	class DeleteCommand : public Command
	{
	public:
		DeleteCommand(int track, int row) : track(track), row(row) {}
		~DeleteCommand() {}
		
		void exec(SyncDocument *data)
		{
			SyncTrack *t = data->getTrack(track);
			Q_ASSERT(t->isKeyFrame(row));
			oldKey = t->getKeyFrame(row);
			Q_ASSERT(oldKey.row == row);
			t->removeKey(row);
			data->clientSocket.sendDeleteKeyCommand(t->name.toUtf8().constData(), row); // update clients
		}
		
		void undo(SyncDocument *data)
		{
			SyncTrack *t = data->getTrack(track);
			Q_ASSERT(!t->isKeyFrame(row));
			Q_ASSERT(oldKey.row == row);
			t->setKey(oldKey);
			data->clientSocket.sendSetKeyCommand(t->name.toUtf8().constData(), oldKey); // update clients
		}

	private:
		int track, row;
		SyncTrack::TrackKey oldKey;
	};

	
	class EditCommand : public Command
	{
	public:
		EditCommand(int track, const SyncTrack::TrackKey &key) : track(track), key(key) {}
		~EditCommand() {}

		void exec(SyncDocument *data)
		{
			SyncTrack *t = data->getTrack(track);
			Q_ASSERT(t->isKeyFrame(key.row));
			oldKey = t->getKeyFrame(key.row);
			Q_ASSERT(key.row == oldKey.row);
			t->setKey(key);
			data->clientSocket.sendSetKeyCommand(t->name, key); // update clients
		}

		void undo(SyncDocument *data)
		{
			SyncTrack *t = data->getTrack(track);
			Q_ASSERT(t->isKeyFrame(oldKey.row));
			Q_ASSERT(key.row == oldKey.row);
			t->setKey(oldKey);
			data->clientSocket.sendSetKeyCommand(t->name, oldKey); // update clients
		}

	private:
		int track;
		SyncTrack::TrackKey oldKey, key;
	};
	
	class MultiCommand : public Command
	{
	public:
		~MultiCommand()
		{
			while (!commands.isEmpty())
				delete commands.takeFirst();
		}
		
		void addCommand(Command *cmd)
		{
			commands.push_back(cmd);
		}
		
		size_t getSize() const { return commands.size(); }
		
		void exec(SyncDocument *data)
		{
			QListIterator<Command *> i(commands);
			while (i.hasNext())
				i.next()->exec(data);
		}
		
		void undo(SyncDocument *data)
		{
			QListIterator<Command *> i(commands);
			i.toBack();
			while (i.hasPrevious())
				i.previous()->undo(data);
		}
		
	private:
		QList<Command*> commands;
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
	
	Command *getSetKeyFrameCommand(int track, const SyncTrack::TrackKey &key)
	{
		SyncTrack *t = getTrack(track);
		if (t->isKeyFrame(key.row))
			return new EditCommand(track, key);
		else
			return new InsertCommand(track, key);
	}

	size_t getTrackIndexFromPos(size_t track) const
	{
		Q_ASSERT(track < (size_t)trackOrder.size());
		return trackOrder[track];
	}

	void swapTrackOrder(size_t t1, size_t t2)
	{
		Q_ASSERT(t1 < (size_t)trackOrder.size());
		Q_ASSERT(t2 < (size_t)trackOrder.size());
		std::swap(trackOrder[t1], trackOrder[t2]);
	}

	static SyncDocument *load(const QString &fileName);
	bool save(const QString &fileName);

	bool modified() const
	{
		if (savePointUnreachable) return true;
		return 0 != savePointDelta;
	}

	bool isRowBookmark(int row) const
	{
		QList<int>::const_iterator it = qLowerBound(rowBookmarks.begin(), rowBookmarks.end(), row);
		return it != rowBookmarks.end() && *it == row;
	}

	void toggleRowBookmark(int row)
	{
		QList<int>::iterator it = qLowerBound(rowBookmarks.begin(), rowBookmarks.end(), row);
		if (it == rowBookmarks.end() || *it != row)
			rowBookmarks.insert(it, row);
		else
			rowBookmarks.erase(it);
	}

	ClientSocket clientSocket;

	size_t getRows() const { return rows; }
	void setRows(size_t rows) { this->rows = rows; }

	QString fileName;

	int nextRowBookmark(int row) const
	{
		QList<int>::const_iterator it = qLowerBound(rowBookmarks.begin(), rowBookmarks.end(), row);
		if (it == rowBookmarks.end())
			return -1;
		return *it;
	}

	int prevRowBookmark(int row) const
	{
		QList<int>::const_iterator it = qLowerBound(rowBookmarks.begin(), rowBookmarks.end(), row);
		if (it == rowBookmarks.end()) {

			// this can only really happen if the list is empty
			if (it == rowBookmarks.begin())
				return -1;

			// reached the end, pick the last bookmark if it's after the current row
			it--;
			return *it < row ? *it : -1;
		}

		// pick the previous key (if any)
		return it != rowBookmarks.begin() ? *(--it) : -1;
	}

private:
	QList<SyncTrack*> tracks;
	QList<int> rowBookmarks;
	QVector<size_t> trackOrder;
	size_t rows;

	// undo / redo functionality
	QStack<Command*> undoStack;
	QStack<Command*> redoStack;
	int savePointDelta;        // how many undos must be done to get to the last saved state
	bool savePointUnreachable; // is the save-point reachable?

};

#endif // !defined(SYNCDOCUMENT_H)
