#include "syncdocument.h"
#include <QFile>
#include <QMessageBox>
#include <QDomDocument>
#include <QTextStream>


SyncDocument::~SyncDocument()
{
	sync_data_deinit(this);
	clearUndoStack();
	clearRedoStack();
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
		int trackIndex = sync_find_track(ret, name.toUtf8());
		if (0 > trackIndex)
			trackIndex = int(ret->createTrack(name.toUtf8().constData()));

		QDomNodeList rowNodes = trackNode.childNodes();
		for (int i = 0; i < int(rowNodes.length()); ++i) {
			QDomNode keyNode = rowNodes.item(i);
			QString baseName = keyNode.nodeName();
			if (baseName == "key") {
				QDomNamedNodeMap rowAttribs = keyNode.attributes();
				QString rowString = rowAttribs.namedItem("row").nodeValue();
				QString valueString = rowAttribs.namedItem("value").nodeValue();
				QString interpolationString = rowAttribs.namedItem("interpolation").nodeValue();

				track_key k;
				k.row = rowString.toInt();
				k.value = valueString.toFloat();
				k.type = key_type(interpolationString.toInt());

				assert(!is_key_frame(ret->tracks[trackIndex], k.row));
				if (sync_set_key(ret->tracks[trackIndex], &k))
					qFatal("failed to insert key");
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
	for (size_t i = 0; i < num_tracks; ++i) {
		const sync_track *t = tracks[trackOrder[i]];

		QDomElement trackElem =
		    doc.createElement("track");
		trackElem.setAttribute("name", t->name);
		for (int i = 0; i < (int)t->num_keys; ++i) {
			int row = t->keys[i].row;
			float value = t->keys[i].value;
			char interpolationType = char(t->keys[i].type);

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
		if (t->num_keys)
			trackElem.appendChild(
			    doc.createTextNode("\n\t\t"));

		tracksNode.appendChild(doc.createTextNode("\n\t\t"));
		tracksNode.appendChild(trackElem);
	}
	if (0 != num_tracks)
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

	savePointDelta = 0;
	savePointUnreachable = false;
	return true;
}
