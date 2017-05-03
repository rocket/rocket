#include "syncdocument.h"
#include <QFile>
#include <QMessageBox>
#include <QDomDocument>
#include <QTextStream>

#if (QT_VERSION >= QT_VERSION_CHECK(5, 1, 0))
#include <QSaveFile>
#define USE_QSAVEFILE
#endif

SyncDocument::~SyncDocument()
{
	for (int i = 0; i < tracks.size(); ++i)
		delete tracks[i];
}

SyncTrack *SyncDocument::createTrack(const QString &name)
{
	QStringList parts = name.split(':');
	SyncPage *page = defaultSyncPage;
	QString visibleName = name;
	if (parts.size() > 1) {
		page = findSyncPage(parts[0]);
		if (!page)
			page = createSyncPage(parts[0]);
		visibleName = name.right(name.length() - parts[0].length() - 1);
	}

	SyncTrack *t = new SyncTrack(name, visibleName);
	tracks.append(t);
	page->addTrack(t);
	return t;
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
	for (int i = 0; i < trackNodes.count(); ++i) {
		QDomNode trackNode = trackNodes.item(i);
		QDomNamedNodeMap attribs = trackNode.attributes();

		QString name = attribs.namedItem("name").nodeValue();

		// look up track-name, create it if it doesn't exist
		SyncTrack *t = ret->findTrack(name.toUtf8());
		if (!t)
			t = ret->createTrack(name.toUtf8().constData());

		QDomNodeList rowNodes = trackNode.childNodes();
		for (int i = 0; i < rowNodes.count(); ++i) {
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
	for (int i = 0; i < bookmarkNodes.count(); ++i) {
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

static QDomElement serializeTrack(QDomDocument &doc, const SyncTrack *t)
{
	QDomElement trackElem = doc.createElement("track");
	trackElem.setAttribute("name", t->getName());

	QMap<int, SyncTrack::TrackKey> keyMap = t->getKeyMap();
	QMap<int, SyncTrack::TrackKey>::const_iterator it;
	for (it = keyMap.constBegin(); it != keyMap.constEnd(); ++it) {
		int row = it.key();
		float value = it->value;
		char interpolationType = char(it->type);

		QDomElement keyElem = doc.createElement("key");

		keyElem.setAttribute("row", row);
		keyElem.setAttribute("value", value);
		keyElem.setAttribute("interpolation", (int)interpolationType);

		trackElem.appendChild(doc.createTextNode("\n\t\t\t"));
		trackElem.appendChild(keyElem);
	}

	if (keyMap.size())
		trackElem.appendChild(doc.createTextNode("\n\t\t"));

	return trackElem;
}

bool SyncDocument::save(const QString &fileName)
{
	QDomDocument doc;
	QDomElement rootNode = doc.createElement("sync");
	rootNode.setAttribute("rows", getRows());
	doc.appendChild(rootNode);

	rootNode.appendChild(doc.createTextNode("\n\t"));
	QDomElement tracksNode =
	    doc.createElement("tracks");
	for (int i = 0; i < getSyncPageCount(); i++) {
		SyncPage *page = getSyncPage(i);
		for (int j = 0; j < page->getTrackCount(); ++j) {
			const SyncTrack *t = page->getTrack(j);
			QDomElement trackElem = serializeTrack(doc, t);

			tracksNode.appendChild(doc.createTextNode("\n\t\t"));
			tracksNode.appendChild(trackElem);
		}
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

#ifdef USE_QSAVEFILE
	QSaveFile file(fileName);
#else
	QFile file(fileName);
#endif

	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		QMessageBox::critical(NULL, "Error", file.errorString());
		return false;
	}
	QTextStream streamFileOut(&file);
	streamFileOut.setCodec("UTF-8");
	streamFileOut << doc.toString();
	streamFileOut.flush();

#ifdef USE_QSAVEFILE
	file.commit();
#else
	file.close();
#endif

	undoStack.setClean();
	return true;
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

int SyncDocument::nextRowBookmark(int row) const
{
	QList<int>::const_iterator it = qLowerBound(rowBookmarks.begin(), rowBookmarks.end(), row);
	if (it == rowBookmarks.end())
		return -1;
	return *it;
}

class InsertCommand : public QUndoCommand
{
public:
	InsertCommand(SyncTrack *track, const SyncTrack::TrackKey &key, QUndoCommand *parent = 0) :
	    QUndoCommand("insert", parent),
	    track(track),
	    key(key)
	{}

	virtual void redo()
	{
		Q_ASSERT(!track->isKeyFrame(key.row));
		track->setKey(key);
	}

	virtual void undo()
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
	    row(row),
	    oldKey(track->getKeyFrame(row))
	{}

	virtual void redo()
	{
		Q_ASSERT(track->isKeyFrame(row));
		Q_ASSERT(oldKey.row == row);
		Q_ASSERT(track->getKeyFrame(row) == oldKey);
		track->removeKey(row);
	}

	virtual void undo()
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
	    oldKey(track->getKeyFrame(key.row)),
	    key(key)
	{}

	virtual void redo()
	{
		Q_ASSERT(track->isKeyFrame(key.row));
		Q_ASSERT(key.row == oldKey.row);
		Q_ASSERT(track->getKeyFrame(key.row) == oldKey);
		track->setKey(key);
	}

	virtual void undo()
	{
		Q_ASSERT(track->isKeyFrame(oldKey.row));
		Q_ASSERT(key.row == oldKey.row);
		Q_ASSERT(track->getKeyFrame(key.row) == key);
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
