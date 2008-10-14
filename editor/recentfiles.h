#pragma once
#include "stdafx.h"

class RecentFiles
{
public:
	RecentFiles(HMENU menu) : mruFileMenu(menu) { }

	void load(HKEY key);
	void save(HKEY key);
	void insert(const std::string &fileName);
	void update();

	size_t getEntryCount() const 
	{
		return mruList.size();
	}

	bool getEntry(size_t index, std::string &out) const;

private:
	static std::string getEntryName(size_t i)
	{
		std::string temp = std::string("RecentFile");
		temp += char('0' + i);
		return temp;
	}

	std::list<std::string> mruList;
	HMENU mruFileMenu;
};
