#ifndef SYNCDOCUMENT_H
#define SYNCDOCUMENT_H

#include <QStack>
#include <QList>
#include <QVector>
#include <QString>
#include <QUndoCommand>
#include <QUndoStack>

#include "clientsocket.h"
#include "synctrack.h"

class SyncDocument : public QObject {
	Q_OBJECT
public:
	SyncDocument() :
	    rows(128)
	{
		QObject::connect(&undoStack, SIGNAL(cleanChanged(bool)),
		                 this, SLOT(cleanChanged(bool)));
	}

	~SyncDocument();

	SyncTrack *createTrack(const QString &name)
	{
		SyncTrack *t = new SyncTrack(name);
		tracks.append(t);

		int index = tracks.size() - 1;
		trackOrder.push_back(index);
		Q_ASSERT(trackOrder.size() == tracks.size());
		return t;
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

	void undo() { undoStack.undo(); }
	void redo() { undoStack.redo(); }
	bool isModified() const { return !undoStack.isClean(); }
	bool canUndo () const { return undoStack.canUndo();  }
	bool canRedo () const { return undoStack.canRedo();  }

	void beginMacro(const QString &text) { undoStack.beginMacro(text); }
	void setKeyFrame(SyncTrack *track, const SyncTrack::TrackKey &key);
	void deleteKeyFrame(SyncTrack *track, int row);
	void endMacro() { undoStack.endMacro(); }

	size_t getTrackIndexFromPos(size_t track) const;
	void swapTrackOrder(size_t t1, size_t t2);

	static SyncDocument *load(const QString &fileName);
	bool save(const QString &fileName);

	bool isRowBookmark(int row) const;
	void toggleRowBookmark(int row);

	size_t getRows() const { return rows; }
	void setRows(size_t rows) { this->rows = rows; }

	QString fileName;

	int nextRowBookmark(int row) const;
	int prevRowBookmark(int row) const;

private:
	QList<SyncTrack*> tracks;
	QList<int> rowBookmarks;
	QVector<size_t> trackOrder;
	size_t rows;

	QUndoStack undoStack;

signals:
	void modifiedChanged(bool modified);

private slots:
	void cleanChanged(bool clean) { emit modifiedChanged(!clean); }
};

#endif // !defined(SYNCDOCUMENT_H)
