#ifndef SYNCDOCUMENT_H
#define SYNCDOCUMENT_H

#include <QStack>
#include <QList>
#include <QVector>
#include <QString>
#include <QUndoCommand>
#include <QUndoStack>
#include <QStringList>

#include "synctrack.h"
#include "syncpage.h"

class SyncDocument : public QObject {
	Q_OBJECT
public:
	SyncDocument() :
	    rows(128)
	{
		defaultSyncPage = createSyncPage("default");
		QObject::connect(&undoStack, SIGNAL(cleanChanged(bool)),
		                 this,       SLOT(onCleanChanged(bool)));
	}

	~SyncDocument();

	SyncTrack *createTrack(const QString &name);

	SyncTrack *getTrack(int index)
	{
		Q_ASSERT(index >= 0 && index < tracks.size());
		return tracks[index];
	}

	SyncTrack *findTrack(const QString &name)
	{
		for (int i = 0; i < tracks.size(); ++i)
			if (name == tracks[i]->getName())
				return tracks[i];
		return NULL;
	}

	int getTrackCount() const
	{
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

	static SyncDocument *load(const QString &fileName);
	bool save(const QString &fileName);

	bool isRowBookmark(int row) const;
	void toggleRowBookmark(int row);

	int getRows() const { return rows; }
	void setRows(int rows) { this->rows = rows; }

	QString fileName;

	int prevRowBookmark(int row) const;
	int nextRowBookmark(int row) const;

	SyncPage *findSyncPage(const QString &name)
	{
		for (int i = 0; i < syncPages.size(); ++i)
			if (name == syncPages[i]->getName())
				return syncPages[i];
		return NULL;
	}

	SyncPage *createSyncPage(const QString &name)
	{
		SyncPage *syncPage = new SyncPage(this, name);
		syncPages.append(syncPage);

		emit syncPageAdded(syncPage);
		return syncPage;
	}

	int getSyncPageCount() const
	{
		return syncPages.size();
	}

	SyncPage *getSyncPage(int index)
	{
		Q_ASSERT(index >= 0 && index < syncPages.size());
		return syncPages[index];
	}

private:
	QList<SyncTrack*> tracks;
	QList<int> rowBookmarks;
	QList<SyncPage*> syncPages;
	SyncPage *defaultSyncPage;
	int rows;

	QUndoStack undoStack;

signals:
	void syncPageAdded(SyncPage *page);
	void modifiedChanged(bool modified);
	void trackModified(int track);

private slots:
	void onCleanChanged(bool clean) { emit modifiedChanged(!clean); }
};

#endif // !defined(SYNCDOCUMENT_H)
