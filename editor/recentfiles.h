#pragma once
#include "stdafx.h"

static inline bool setRegString(HKEY key, const std::string &name, const std::string &value)
{
	return ERROR_SUCCESS == RegSetValueEx(key, name.c_str(), 0, REG_SZ, (BYTE *)value.c_str(), (DWORD)value.size());
}

static inline bool getRegString(HKEY key, const std::string &name, std::string &out)
{
	DWORD size = 0;
	DWORD type = 0;
	if (ERROR_SUCCESS != RegQueryValueEx(key, name.c_str(), 0, &type, (LPBYTE)NULL, &size)) return false;
	if (REG_SZ != type) return false;

	out.resize(size);
	DWORD ret = RegQueryValueEx(key, name.c_str(), 0, &type, (LPBYTE)&out[0], &size);
	while (out.size() > 0 && out[out.size() - 1] == '\0') out.resize(out.size() - 1);

	assert(ret == ERROR_SUCCESS);
	assert(REG_SZ == type);
	assert(size == out.size() + 1);

	return true;
}

class RecentFiles
{
public:
	RecentFiles(HMENU menu) : mruFileMenu(menu) { }

	void load(HKEY key)
	{
		for (size_t i = 0; i < 5; ++i)
		{
			std::string fileName;
			if (getRegString(key, getEntryName(i), fileName))
			{
				mruList.push_back(fileName);
			}
		}

		if (mruList.size() > 0) update();
	}

	void save(HKEY key)
	{
		std::list<std::string>::const_iterator it;
		size_t i;
		for (i = 0, it = mruList.begin(); it != mruList.end(); ++it, ++i)
		{
			assert(i <= 5);
			setRegString(key, getEntryName(i), *it);
		}
	}

	void insert(const std::string &fileName)
	{
		mruList.remove(fileName); // remove, if present
		mruList.push_front(fileName); // add to front
		while (mruList.size() > 5) mruList.pop_back(); // remove old entries
	}

	void update()
	{
		while (0 != RemoveMenu(mruFileMenu, 0, MF_BYPOSITION));

		std::list<std::string>::const_iterator it;
		size_t i;
		for (i = 0, it = mruList.begin(); it != mruList.end(); ++it, ++i)
		{
			assert(i <= 5);
			std::string menuEntry = std::string("&");
			menuEntry += char('1' + i);
			menuEntry += " ";
			menuEntry += *it;
			AppendMenu(mruFileMenu, MF_STRING, ID_RECENTFILES_FILE1 + i, menuEntry.c_str());
		}
	}

	size_t getEntryCount() const 
	{
		return mruList.size();
	}

	bool getEntry(size_t index, std::string &out) const
	{
		std::list<std::string>::const_iterator it;
		size_t i;
		for (i = 0, it = mruList.begin(); it != mruList.end(); ++it, ++i)
		{
			if (i == index)
			{
				out = *it;
				return true;
			}
		}
		return false;
	}

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
