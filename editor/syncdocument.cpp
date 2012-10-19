#include "syncdocument.h"
#include <string>

SyncDocument::~SyncDocument()
{
	sync_data_deinit(this);
	clearUndoStack();
	clearRedoStack();
}

#import <msxml3.dll> named_guids

SyncDocument *SyncDocument::load(const std::wstring &fileName)
{
	SyncDocument *ret = new SyncDocument;
	ret->fileName = fileName;

	MSXML2::IXMLDOMDocumentPtr doc(MSXML2::CLSID_DOMDocument);
	try {
		doc->load(fileName.c_str());
		MSXML2::IXMLDOMNamedNodeMapPtr attribs = doc->documentElement->Getattributes();

		MSXML2::IXMLDOMNodePtr rowsParam = attribs->getNamedItem("rows");
		if (rowsParam) {
			std::string rowsString = rowsParam->Gettext();
			ret->setRows(atoi(rowsString.c_str()));
		}

		MSXML2::IXMLDOMNodeListPtr trackNodes =
		    doc->documentElement->selectNodes("//track");
		for (int i = 0; i < trackNodes->Getlength(); ++i) {
			MSXML2::IXMLDOMNodePtr trackNode = trackNodes->Getitem(i);
			MSXML2::IXMLDOMNamedNodeMapPtr attribs = trackNode->Getattributes();
			
			std::string name = attribs->getNamedItem("name")->Gettext();

			// look up track-name, create it if it doesn't exist
			int trackIndex = sync_find_track(ret, name.c_str());
			if (0 > trackIndex) trackIndex = int(ret->createTrack(name));

			MSXML2::IXMLDOMNodeListPtr rowNodes = trackNode->GetchildNodes();
			for (int i = 0; i < rowNodes->Getlength(); ++i) {
				MSXML2::IXMLDOMNodePtr keyNode = rowNodes->Getitem(i);
				std::string baseName = keyNode->GetbaseName();
				if (baseName == "key") {
					MSXML2::IXMLDOMNamedNodeMapPtr rowAttribs = keyNode->Getattributes();
					std::string rowString = rowAttribs->getNamedItem("row")->Gettext();
					std::string valueString = rowAttribs->getNamedItem("value")->Gettext();
					std::string interpolationString = rowAttribs->getNamedItem("interpolation")->Gettext();

					track_key k;
					k.row = atoi(rowString.c_str());
					k.value = float(atof(valueString.c_str()));
					k.type = key_type(atoi(interpolationString.c_str()));

					assert(!is_key_frame(ret->tracks[trackIndex], k.row));
					if (sync_set_key(ret->tracks[trackIndex], &k))
						throw std::bad_alloc("sync_set_key");
				}
			}
		}

		MSXML2::IXMLDOMNodeListPtr bookmarkNodes =
		    doc->documentElement->selectNodes(
		    "/sync/bookmarks/bookmark");
		for (int i = 0; i < bookmarkNodes->Getlength(); ++i) {
			MSXML2::IXMLDOMNodePtr bookmarkNode =
			    bookmarkNodes->Getitem(i);
			MSXML2::IXMLDOMNamedNodeMapPtr bookmarkAttribs =
			    bookmarkNode->Getattributes();
			std::string str =
			    bookmarkAttribs->getNamedItem("row")->Gettext();
			int row = atoi(str.c_str());
			ret->toggleRowBookmark(row);
		}
	}
	catch(_com_error &e)
	{
		char temp[256];
		_snprintf(temp, 256, "Error loading: %s\n", (const char*)_bstr_t(e.Description()));
		MessageBox(NULL, temp, NULL, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
		return NULL;
	}
	
	return ret;
}

bool SyncDocument::save(const std::wstring &fileName)
{
	MSXML2::IXMLDOMDocumentPtr doc(MSXML2::CLSID_DOMDocument);
	try {
		MSXML2::IXMLDOMElementPtr rootNode = doc->createElement("sync");
		rootNode->setAttribute("rows", getRows());
		doc->appendChild(rootNode);
		rootNode->appendChild(doc->createTextNode("\n\t"));

		MSXML2::IXMLDOMElementPtr tracksNode =
		    doc->createElement("tracks");
		for (size_t i = 0; i < num_tracks; ++i) {
			const sync_track *t = tracks[trackOrder[i]];

			MSXML2::IXMLDOMElementPtr trackElem =
			    doc->createElement("track");
			trackElem->setAttribute("name", t->name);

			for (int i = 0; i < (int)t->num_keys; ++i) {
				size_t row = t->keys[i].row;
				float value = t->keys[i].value;
				char interpolationType = char(t->keys[i].type);

				MSXML2::IXMLDOMElementPtr keyElem =
				    doc->createElement("key");
				
				keyElem->setAttribute("row", row);
				keyElem->setAttribute("value", value);
				keyElem->setAttribute("interpolation",
				    (int)interpolationType);

				trackElem->appendChild(
				    doc->createTextNode("\n\t\t\t"));
				trackElem->appendChild(keyElem);
			}
			if (t->num_keys)
				trackElem->appendChild(
				    doc->createTextNode("\n\t\t"));

			tracksNode->appendChild(doc->createTextNode("\n\t\t"));
			tracksNode->appendChild(trackElem);
		}
		if (0 != num_tracks)
			tracksNode->appendChild(doc->createTextNode("\n\t"));
		rootNode->appendChild(tracksNode);
		rootNode->appendChild(doc->createTextNode("\n\t"));

		MSXML2::IXMLDOMElementPtr bookmarksNode =
		    doc->createElement("bookmarks");
		std::set<int>::const_iterator it;
		for (it = rowBookmarks.begin(); it != rowBookmarks.end(); ++it) {
			MSXML2::IXMLDOMElementPtr bookmarkElem =
			    doc->createElement("bookmark");
			bookmarkElem->setAttribute("row", *it);

			bookmarksNode->appendChild(
			    doc->createTextNode("\n\t\t"));
			bookmarksNode->appendChild(bookmarkElem);
		}
		if (0 != rowBookmarks.size())
			bookmarksNode->appendChild(
			    doc->createTextNode("\n\t"));
		rootNode->appendChild(bookmarksNode);
		rootNode->appendChild(doc->createTextNode("\n"));

		doc->save(fileName.c_str());
		
		savePointDelta = 0;
		savePointUnreachable = false;
	}
	catch(_com_error &e)
	{
		char temp[256];
		_snprintf(temp, 256, "Error saving: %s\n", (const char*)_bstr_t(e.Description()));
		MessageBox(NULL, temp, NULL, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
		return false;
	}
	return true;
}

