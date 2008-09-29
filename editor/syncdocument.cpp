#include "syncdocument.h"

SyncDocument::~SyncDocument()
{
	clearUndoStack();
	clearRedoStack();
}

#import <msxml3.dll> named_guids

bool SyncDocument::load(const std::string &fileName)
{
	MSXML2::IXMLDOMDocumentPtr doc(MSXML2::CLSID_DOMDocument);
	try
	{
		SyncDocument::MultiCommand *multiCmd = new SyncDocument::MultiCommand();
		
		doc->load(fileName.c_str());
		MSXML2::IXMLDOMNodeListPtr trackNodes = doc->documentElement->selectNodes("track");
		for (int i = 0; i < trackNodes->Getlength(); ++i)
		{
			MSXML2::IXMLDOMNodePtr trackNode = trackNodes->Getitem(i);
			MSXML2::IXMLDOMNamedNodeMapPtr attribs = trackNode->Getattributes();
			
			std::string name = attribs->getNamedItem("name")->Gettext();
			
			// look up track-name, create it if it doesn't exist
			int trackIndex = getTrackIndex(name);
			if (0 > trackIndex)
			{
				trackIndex = int(createTrack(name));
				trackOrder.push_back(trackIndex);
			}
			
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
					
					sync::Track::KeyFrame keyFrame(
						float(atof(valueString.c_str())),
						sync::Track::KeyFrame::InterpolationType(
							atoi(interpolationString.c_str())
						)
					);
					multiCmd->addCommand(
						this->getSetKeyFrameCommand(
							int(trackIndex),
							atoi(rowString.c_str()),
							keyFrame
						)
					);
				}
			}
		}
		this->exec(multiCmd);
	}
	catch(_com_error &e)
	{
		char temp[256];
		_snprintf(temp, 256, "Error loading: %s\n", (const char*)_bstr_t(e.Description()));
		MessageBox(NULL, temp, NULL, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
		return false;
	}
	
	return true;
}

bool SyncDocument::save(const std::string &fileName)
{
	MSXML2::IXMLDOMDocumentPtr doc(MSXML2::CLSID_DOMDocument);
	try
	{
		_variant_t varNodeType((short)MSXML2::NODE_ELEMENT);
		MSXML2::IXMLDOMNodePtr rootNode = doc->createNode(varNodeType, _T("tracks"), _T(""));
		doc->appendChild(rootNode);
		
		for (size_t i = 0; i < getTrackCount(); ++i)
		{
			const sync::Track &track = getTrack(i);
			
			MSXML2::IXMLDOMElementPtr trackElem = doc->createElement(_T("track"));
			trackElem->setAttribute(_T("name"), track.getName().c_str());
			rootNode->appendChild(trackElem);
			
			sync::Track::KeyFrameContainer::const_iterator it;
			for (it = track.keyFrames.begin(); it != track.keyFrames.end(); ++it)
			{
				char temp[256];
				size_t row = it->first;
				float value = it->second.value;
				char interpolationType = char(it->second.interpolationType);
				
				MSXML2::IXMLDOMElementPtr keyElem = doc->createElement(_T("key"));
				
				_snprintf(temp, 256, "%d", row);
				keyElem->setAttribute(_T("row"), temp);
				
				_snprintf(temp, 256, "%f", value);
				keyElem->setAttribute(_T("value"), temp);
				
				_snprintf(temp, 256, "%d", interpolationType);
				keyElem->setAttribute(_T("interpolation"), temp);
				
				trackElem->appendChild(keyElem);
			}
		}
		
		doc->save(fileName.c_str());
	}
	catch(_com_error &e)
	{
		char temp[256];
		_snprintf(temp, 256, "Error loading: %s\n", (const char*)_bstr_t(e.Description()));
		MessageBox(NULL, temp, NULL, MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
		return false;
	}
	return true;
}

