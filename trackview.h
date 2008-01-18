#pragma once

#include "syncdata.h"

#include <list>
#include <string>
#include <stack>

class SyncData;

class SyncDataEdit
{
public:
	SyncDataEdit() : syncData(NULL) {}
	
	void setSyncData(SyncData *syncData) { this->syncData = syncData; }
	SyncData *getSyncData() { return syncData; }
	
	class Command
	{
	public:
		virtual ~Command() {}
		virtual void exec(SyncDataEdit *data) = 0;
		virtual void undo(SyncDataEdit *data) = 0;
	};
	
	class EditCommand : public Command
	{
	public:
		EditCommand(int track, int row, bool existing, float value) : track(track), row(row), newValExisting(existing), newVal(value) {}
		~EditCommand() {}
		
		virtual void exec(SyncDataEdit *data)
		{
			SyncTrack &track = data->getSyncData()->getTrack(this->track);

			// store old state
			oldValExisting = track.isKeyFrame(row);
			if (oldValExisting) oldVal = track.getKeyFrame(row)->value;

			// update
			if (!newValExisting) track.deleteKeyFrame(row);
			else track.setKeyFrame(row, newVal);
		}
		
		virtual void undo(SyncDataEdit *data)
		{
			SyncTrack &track = data->getSyncData()->getTrack(this->track);

			// un-update
			if (!oldValExisting) track.deleteKeyFrame(row);
			else track.setKeyFrame(row, oldVal);
		}
		
	private:
		int track, row;
		float newVal, oldVal;
		bool newValExisting, oldValExisting;
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

private:
	
	std::stack<Command*> undoStack;
	std::stack<Command*> redoStack;
	
	SyncData *syncData;
};


// custom messages
#define WM_REDO (WM_USER + 1)

class TrackView
{
public:
	TrackView();
	~TrackView();
	
	HWND create(HINSTANCE hInstance, HWND hwndParent);
	HWND getWin(){ return hwnd; }
	
	void setSyncData(SyncData *syncData) { this->syncDataEdit.setSyncData(syncData); }
	SyncData *getSyncData() { return syncDataEdit.getSyncData(); }
	
private:
	// some nasty hackery to forward the window messages
	friend static LRESULT CALLBACK trackViewWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT windowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	
	// events
	LRESULT onCreate();
	LRESULT onPaint();
	LRESULT onVScroll(UINT sbCode, int newPos);
	LRESULT onHScroll(UINT sbCode, int newPos);
	LRESULT onSize(int width, int height);
	LRESULT onKeyDown(UINT keyCode, UINT flags);
	LRESULT onChar(UINT keyCode, UINT flags);
	
	// the window procedure
	
	void paintTracks(HDC hdc, RECT rcTracks);
	void paintTopMargin(HDC hdc, RECT rcTracks);
	
	void setupScrollBars();
	void setScrollPos(int newScrollPosX, int newScrollPosY);
	void scrollWindow(int newScrollPosX, int newScrollPosY);
	
	void invalidatePos(int track, int row)
	{
		RECT rect;
		rect.left  = getScreenX(track);
		rect.right = getScreenX(track + 1);
		rect.top    = getScreenY(row);
		rect.bottom = getScreenY(row + 1);
		InvalidateRect(hwnd, &rect, TRUE);
	}
	
	void setEditRow(int newEditRow);
	void setEditTrack(int newEditTrack);
	
	int getScreenY(int row);
	int getScreenX(int track);
	
	int getTrackCount()
	{
		SyncData *syncData = getSyncData();
		if (NULL == syncData) return 0;
		return int(syncData->getTrackCount());
	};
	
	/* cursor position */
	int editRow, editTrack;
	
	int scrollPosX,  scrollPosY;
	int windowWidth, windowHeight;
	int windowRows,  windowTracks;
	
	SyncDataEdit syncDataEdit;
	
	std::string editString;
	
	HWND hwnd;
	HBRUSH editBrush;
};

ATOM registerTrackViewWindowClass(HINSTANCE hInstance);
