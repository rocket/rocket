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

		MSXML2::IXMLDOMNodeListPtr trackNodes = doc->documentElement->selectNodes("track");
		for (int i = 0; i < trackNodes->Getlength(); ++i)
		{
			MSXML2::IXMLDOMNodePtr trackNode = trackNodes->Getitem(i);
			MSXML2::IXMLDOMNamedNodeMapPtr attribs = trackNode->Getattributes();
			
			std::string name = attribs->getNamedItem("name")->Gettext();

			// look up track-name, create it if it doesn't exist
			int trackIndex = sync_find_track(ret, name.c_str());
			if (0 > trackIndex) trackIndex = int(ret->createTrack(name));

			MSXML2::IXMLDOMNodeListPtr rowNodes = trackNode->GetchildNodes();
			for (int i = 0; i < rowNodes->Getlength(); ++i)
			{
				MSXML2::IXMLDOMNodePtr keyNode = rowNodes->Getitem(i);
				std::string baseName = keyNode->GetbaseName();
				if (baseName == "key")
				{
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
	try
	{
		char temp[256];
		_variant_t varNodeType((short)MSXML2::NODE_ELEMENT);
		MSXML2::IXMLDOMElementPtr rootNode = doc->createElement("tracks");
		doc->appendChild(rootNode);

		_snprintf(temp, 256, "%d", getRows());
		rootNode->setAttribute("rows", temp);

		for (size_t i = 0; i < num_tracks; ++i) {
			const sync_track *t = tracks[i];

			MSXML2::IXMLDOMElementPtr trackElem = doc->createElement("track");
			trackElem->setAttribute("name", t->name);

			rootNode->appendChild(doc->createTextNode("\n\t"));
			rootNode->appendChild(trackElem);

			for (int i = 0; i < (int)t->num_keys; ++i) {
				size_t row = t->keys[i].row;
				float value = t->keys[i].value;
				char interpolationType = char(t->keys[i].type);

				MSXML2::IXMLDOMElementPtr keyElem = doc->createElement("key");
				
				_snprintf(temp, 256, "%d", row);
				keyElem->setAttribute("row", temp);

				_snprintf(temp, 256, "%f", value);
				keyElem->setAttribute("value", temp);

				_snprintf(temp, 256, "%d", interpolationType);
				keyElem->setAttribute("interpolation", temp);

				trackElem->appendChild(doc->createTextNode("\n\t\t"));
				trackElem->appendChild(keyElem);
			}
			if (t->num_keys)
				trackElem->appendChild(doc->createTextNode("\n\t"));
		}
		if (0 != num_tracks)
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

