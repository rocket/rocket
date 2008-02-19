/* Copyright (C) 2007-2008 Erik Faye-Lund and Egbert Teeselink
 * For conditions of distribution and use, see copyright notice in LICENSE.TXT
 */

#pragma once

#include "../sync/network.h"
#include "../sync/data.h"
#include <stack>
#include <list>


#import <msxml4.dll> named_guids

class SyncDocument : public sync::Data
{
public:
	SyncDocument() : sync::Data(), clientPaused(true) {}
	
	void sendSetKeyCommand(int track, int row, const sync::Track::KeyFrame &key)
	{
		if (INVALID_SOCKET == clientSocket) return;
		if (clientRemap.count(track) == 0) return;
		track = int(clientRemap[track]);
		
		unsigned char cmd = SET_KEY;
		send(clientSocket, (char*)&cmd, 1, 0);
		send(clientSocket, (char*)&track, sizeof(int), 0);
		send(clientSocket, (char*)&row,   sizeof(int), 0);
		send(clientSocket, (char*)&key.value, sizeof(float), 0);
		send(clientSocket, (char*)&key.interpolationType, 1, 0);
	}
	
	void sendDeleteKeyCommand(int track, int row)
	{
		if (INVALID_SOCKET == clientSocket) return;
		if (clientRemap.count(track) == 0) return;
		track = int(clientRemap[track]);
		
		unsigned char cmd = DELETE_KEY;
		send(clientSocket, (char*)&cmd, 1, 0);
		send(clientSocket, (char*)&track, sizeof(int), 0);
		send(clientSocket, (char*)&row,   sizeof(int), 0);
	}
	
	void sendSetRowCommand(int row)
	{
		if (INVALID_SOCKET == clientSocket) return;
		unsigned char cmd = SET_ROW;
		send(clientSocket, (char*)&cmd, 1, 0);
		send(clientSocket, (char*)&row,   sizeof(int), 0);
	}
	
	void sendPauseCommand(bool pause)
	{
		unsigned char cmd = PAUSE;
		send(clientSocket, (char*)&cmd, 1, 0);
		unsigned char flag = pause;
		send(clientSocket, (char*)&flag, 1, 0);
		clientPaused = pause;
	}
	
	void sendSaveCommand()
	{
		unsigned char cmd = SAVE_TRACKS;
		send(clientSocket, (char*)&cmd, 1, 0);
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
		InsertCommand(int track, int row, const sync::Track::KeyFrame &key) : track(track), row(row), key(key) {}
		~InsertCommand() {}
		
		virtual void exec(SyncDocument *data)
		{
			sync::Track &t = data->getTrack(this->track);
			assert(!t.isKeyFrame(row));
			t.setKeyFrame(row, key);
			data->sendSetKeyCommand(track, row, key); // update clients
		}
		
		virtual void undo(SyncDocument *data)
		{
			sync::Track &t = data->getTrack(this->track);
			assert(t.isKeyFrame(row));
			t.deleteKeyFrame(row);
			
			data->sendDeleteKeyCommand(track, row); // update clients
		}
		
	private:
		int track, row;
		sync::Track::KeyFrame key;
	};
	
	class DeleteCommand : public Command
	{
	public:
		DeleteCommand(int track, int row) : track(track), row(row) {}
		~DeleteCommand() {}
		
		virtual void exec(SyncDocument *data)
		{
			sync::Track &t = data->getTrack(this->track);
			assert(t.isKeyFrame(row));
			oldKey = *t.getKeyFrame(row);
			t.deleteKeyFrame(row);
			
			data->sendDeleteKeyCommand(track, row); // update clients
		}
		
		virtual void undo(SyncDocument *data)
		{
			sync::Track &t = data->getTrack(this->track);
			assert(!t.isKeyFrame(row));
			t.setKeyFrame(row, oldKey);
			
			data->sendSetKeyCommand(track, row, oldKey); // update clients
		}
		
	private:
		int track, row;
		sync::Track::KeyFrame oldKey;
	};

	
	class EditCommand : public Command
	{
	public:
		EditCommand(int track, int row, const sync::Track::KeyFrame &key) : track(track), row(row), key(key) {}
		~EditCommand() {}
		
		virtual void exec(SyncDocument *data)
		{
			sync::Track &t = data->getTrack(this->track);
			
			// store old key
			assert(t.isKeyFrame(row));
			oldKey = *t.getKeyFrame(row);
			
			// update
			t.setKeyFrame(row, key);
			
			data->sendSetKeyCommand(track, row, key); // update clients
		}
		
		virtual void undo(SyncDocument *data)
		{
			sync::Track &t = data->getTrack(this->track);
			
			assert(t.isKeyFrame(row));
			t.setKeyFrame(row, oldKey);
			
			data->sendSetKeyCommand(track, row, oldKey); // update clients
		}
		
	private:
		int track, row;
		sync::Track::KeyFrame oldKey, key;
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
		
		virtual void exec(SyncDocument *data)
		{
			std::list<Command*>::iterator it;
			for (it = commands.begin(); it != commands.end(); ++it) (*it)->exec(data);
		}
		
		virtual void undo(SyncDocument *data)
		{
			std::list<Command*>::iterator it;
			for (it = commands.begin(); it != commands.end(); ++it) (*it)->undo(data);
		}
		
	private:
		std::list<Command*> commands;
	};
	
	void exec(Command *cmd)
	{
		undoStack.push(cmd);
		cmd->exec(this);
		clearRedoStack();
	}
	
	bool undo()
	{
		if (undoStack.size() == 0) return false;
		
		Command *cmd = undoStack.top();
		undoStack.pop();
		
		redoStack.push(cmd);
		cmd->undo(this);
		return true;
	}
	
	bool redo()
	{
		if (redoStack.size() == 0) return false;
		
		Command *cmd = redoStack.top();
		redoStack.pop();
		
		undoStack.push(cmd);
		cmd->exec(this);
		return true;
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
	
	Command *getSetKeyFrameCommand(int track, int row, const sync::Track::KeyFrame &key)
	{
		sync::Track &t = getTrack(track);
		SyncDocument::Command *cmd;
		if (t.isKeyFrame(row)) cmd = new EditCommand(track, row, key);
		else                   cmd = new InsertCommand(track, row, key);
		return cmd;
	}

	bool load(const std::string &fileName)
	{
		MSXML2::IXMLDOMDocumentPtr doc(MSXML2::CLSID_DOMDocument);
		try
		{
			doc->load(fileName.c_str());
			MSXML2::IXMLDOMNodeListPtr trackNodes = doc->documentElement->selectNodes("track");
			for (int i = 0; i < trackNodes->Getlength(); ++i)
			{
				MSXML2::IXMLDOMNodePtr trackNode = trackNodes->Getitem(i);
				MSXML2::IXMLDOMNamedNodeMapPtr attribs = trackNode->Getattributes();
				
				std::string name = attribs->getNamedItem("name")->Gettext();
				sync::Track &t = getTrack(name);
				
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
						t.setKeyFrame(atoi(rowString.c_str()), keyFrame);
					}
				}
			}
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

	bool save(const std::string &fileName)
	{
		MSXML2::IXMLDOMDocumentPtr doc(MSXML2::CLSID_DOMDocument);
		try
		{
			_variant_t varNodeType((short)MSXML2::NODE_ELEMENT);
			MSXML2::IXMLDOMNodePtr rootNode = doc->createNode(varNodeType, _T("tracks"), _T(""));
			doc->appendChild(rootNode);
			
			sync::Data::TrackContainer::iterator iter;
			for (iter = tracks.begin(); iter != tracks.end(); ++iter)
			{
				size_t index = iter->second;
				const sync::Track &track = getTrack(index);
				
				MSXML2::IXMLDOMElementPtr trackElem = doc->createElement(_T("track"));
				trackElem->setAttribute(_T("name"), iter->first.c_str());
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
	
	SOCKET clientSocket;
	std::map<size_t, size_t> clientRemap;
	bool clientPaused;
	
private:
	std::stack<Command*> undoStack;
	std::stack<Command*> redoStack;
};
