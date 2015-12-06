#include "syncdocument.h"
#include <QFile>
#include <QMessageBox>
#include <QDomDocument>
#include <QTextStream>


SyncDocument::~SyncDocument()
{
	for (int i = 0; i < tracks.size(); ++i)
		delete tracks[i];
}

SyncDocument *SyncDocument::load(const QString &fileName)
{
	SyncDocument *ret = new SyncDocument;
	ret->fileName = fileName;

	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly)) {
		QMessageBox::critical(NULL, "Error", file.errorString());
		return NULL;
	}

	QDomDocument doc;
	QString err;
	if (!doc.setContent(&file, &err)) {
		file.close();
		QMessageBox::critical(NULL, "Error", err);
		return NULL;
	}
	file.close();

	QDomNamedNodeMap attribs = doc.documentElement().attributes();
	QDomNode rowsParam = attribs.namedItem("rows");
	if (!rowsParam.isNull()) {
		QString rowsString = rowsParam.nodeValue();
		ret->setRows(rowsString.toInt());
	}

	QDomNodeList trackNodes =
	    doc.documentElement().elementsByTagName("track");
	for (int i = 0; i < int(trackNodes.length()); ++i) {
		QDomNode trackNode = trackNodes.item(i);
		QDomNamedNodeMap attribs = trackNode.attributes();

		QString name = attribs.namedItem("name").nodeValue();

		// look up track-name, create it if it doesn't exist
		SyncTrack *t = ret->findTrack(name.toUtf8());
		if (!t)
			t = ret->createTrack(name.toUtf8().constData());

		QDomNodeList rowNodes = trackNode.childNodes();
		for (int i = 0; i < int(rowNodes.length()); ++i) {
			QDomNode keyNode = rowNodes.item(i);
			QString baseName = keyNode.nodeName();
			if (baseName == "key") {
				QDomNamedNodeMap rowAttribs = keyNode.attributes();
				QString rowString = rowAttribs.namedItem("row").nodeValue();
				QString valueString = rowAttribs.namedItem("value").nodeValue();
				QString interpolationString = rowAttribs.namedItem("interpolation").nodeValue();

				SyncTrack::TrackKey k;
				k.row = rowString.toInt();
				k.value = valueString.toFloat();
				k.type = SyncTrack::TrackKey::KeyType(interpolationString.toInt());

				Q_ASSERT(!t->isKeyFrame(k.row));
				t->setKey(k);
			}
		}
	}

	// YUCK: gathers from entire document
	QDomNodeList bookmarkNodes =
	    doc.documentElement().elementsByTagName("bookmark");
	for (int i = 0; i < int(bookmarkNodes.length()); ++i) {
		QDomNode bookmarkNode =
		    bookmarkNodes.item(i);
		QDomNamedNodeMap bookmarkAttribs =
		    bookmarkNode.attributes();
		QString str =
		    bookmarkAttribs.namedItem("row").nodeValue();
		int row = str.toInt();
		ret->toggleRowBookmark(row);
	}

	return ret;
}

bool SyncDocument::save(const QString &fileName)
{
	QDomDocument doc;
	QDomElement rootNode = doc.createElement("sync");
	rootNode.setAttribute("rows", int(getRows()));
	doc.appendChild(rootNode);

	rootNode.appendChild(doc.createTextNode("\n\t"));
	QDomElement tracksNode =
	    doc.createElement("tracks");
	for (int i = 0; i < getTrackCount(); ++i) {
		const SyncTrack *t = getTrack(trackOrder[i]);

		QDomElement trackElem =
		    doc.createElement("track");
		trackElem.setAttribute("name", t->name);

		QMap<int, SyncTrack::TrackKey> keyMap = t->getKeyMap();
		QMap<int, SyncTrack::TrackKey>::const_iterator it;
		for (it = keyMap.constBegin(); it != keyMap.constEnd(); ++it) {
			int row = it.key();
			float value = it->value;
			char interpolationType = char(it->type);

			QDomElement keyElem =
			    doc.createElement("key");
				
			keyElem.setAttribute("row", row);
			keyElem.setAttribute("value", value);
			keyElem.setAttribute("interpolation",
			    (int)interpolationType);

			trackElem.appendChild(
			    doc.createTextNode("\n\t\t\t"));
			trackElem.appendChild(keyElem);
		}
		if (keyMap.size())
			trackElem.appendChild(
			    doc.createTextNode("\n\t\t"));

		tracksNode.appendChild(doc.createTextNode("\n\t\t"));
		tracksNode.appendChild(trackElem);
	}
	if (getTrackCount())
		tracksNode.appendChild(doc.createTextNode("\n\t"));
	rootNode.appendChild(tracksNode);
	rootNode.appendChild(doc.createTextNode("\n\t"));

	QDomElement bookmarksNode =
	    doc.createElement("bookmarks");
	QList<int>::const_iterator it;
	for (it = rowBookmarks.begin(); it != rowBookmarks.end(); ++it) {
		QDomElement bookmarkElem =
		    doc.createElement("bookmark");
		bookmarkElem.setAttribute("row", *it);

		bookmarksNode.appendChild(
		    doc.createTextNode("\n\t\t"));
		bookmarksNode.appendChild(bookmarkElem);
	}
	if (0 != rowBookmarks.size())
		bookmarksNode.appendChild(
		    doc.createTextNode("\n\t"));
	rootNode.appendChild(bookmarksNode);
	rootNode.appendChild(doc.createTextNode("\n"));

	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		QMessageBox::critical(NULL, "Error", file.errorString());
		return false;
	}
	QTextStream streamFileOut(&file);
	streamFileOut.setCodec("UTF-8");
	streamFileOut << doc.toString();
	streamFileOut.flush();
	file.close();

	undoStack.setClean();
	return true;
}

int SyncDocument::getTrackIndexFromPos(int track) const
{
	Q_ASSERT(track < trackOrder.size());
	return trackOrder[track];
}

void SyncDocument::swapTrackOrder(int t1, int t2)
{
	Q_ASSERT(t1 < trackOrder.size());
	Q_ASSERT(t2 < trackOrder.size());
	std::swap(trackOrder[t1], trackOrder[t2]);
}

bool SyncDocument::isRowBookmark(int row) const
{
	QList<int>::const_iterator it = qLowerBound(rowBookmarks.begin(), rowBookmarks.end(), row);
	return it != rowBookmarks.end() && *it == row;
}

void SyncDocument::toggleRowBookmark(int row)
{
	QList<int>::iterator it = qLowerBound(rowBookmarks.begin(), rowBookmarks.end(), row);
	if (it == rowBookmarks.end() || *it != row)
		rowBookmarks.insert(it, row);
	else
		rowBookmarks.erase(it);
}

int SyncDocument::nextRowBookmark(int row) const
{
	QList<int>::const_iterator it = qLowerBound(rowBookmarks.begin(), rowBookmarks.end(), row);
	if (it == rowBookmarks.end())
		return -1;
	return *it;
}

int SyncDocument::prevRowBookmark(int row) const
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

class InsertCommand : public QUndoCommand
{
public:
	InsertCommand(SyncTrack *track, const SyncTrack::TrackKey &key, QUndoCommand *parent = 0) :
	    QUndoCommand("insert", parent),
	    track(track),
	    key(key)
	{}

	void redo()
	{
		Q_ASSERT(!track->isKeyFrame(key.row));
		track->setKey(key);
	}

	void undo()
	{
		Q_ASSERT(track->isKeyFrame(key.row));
		track->removeKey(key.row);
	}

private:
	SyncTrack *track;
	SyncTrack::TrackKey key;
};

class DeleteCommand : public QUndoCommand
{
public:
	DeleteCommand(SyncTrack *track, int row, QUndoCommand *parent = 0) :
	    QUndoCommand("delete", parent),
	    track(track),
	    row(row)
	{}

	void redo()
	{
		Q_ASSERT(track->isKeyFrame(row));
		oldKey = track->getKeyFrame(row);
		Q_ASSERT(oldKey.row == row);
		track->removeKey(row);
	}

	void undo()
	{
		Q_ASSERT(!track->isKeyFrame(row));
		Q_ASSERT(oldKey.row == row);
		track->setKey(oldKey);
	}

private:
	SyncTrack *track;
	int row;
	SyncTrack::TrackKey oldKey;
};


class EditCommand : public QUndoCommand
{
public:
	EditCommand(SyncTrack *track, const SyncTrack::TrackKey &key, QUndoCommand *parent = 0) :
	        QUndoCommand("edit", parent),
	        track(track),
	        key(key)
	{}

	void redo()
	{
		Q_ASSERT(track->isKeyFrame(key.row));
		oldKey = track->getKeyFrame(key.row);
		Q_ASSERT(key.row == oldKey.row);
		track->setKey(key);
	}

	void undo()
	{
		Q_ASSERT(track->isKeyFrame(oldKey.row));
		Q_ASSERT(key.row == oldKey.row);
		track->setKey(oldKey);
	}

private:
	SyncTrack *track;
	SyncTrack::TrackKey oldKey, key;
};

void SyncDocument::setKeyFrame(SyncTrack *track, const SyncTrack::TrackKey &key)
{
	if (track->isKeyFrame(key.row))
		undoStack.push(new EditCommand(track, key));
	else
		undoStack.push(new InsertCommand(track, key));
}

void SyncDocument::deleteKeyFrame(SyncTrack *track, int row)
{
	undoStack.push(new DeleteCommand(track, row));
}
