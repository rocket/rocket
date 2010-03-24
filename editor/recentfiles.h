#pragma once
#include <string>
#include <list>

class RecentFiles
{
public:
	RecentFiles(HMENU menu) : mruFileMenu(menu) { }

	void load(HKEY key);
	void save(HKEY key);
	void insert(const std::wstring &fileName);
	void update();

	size_t getEntryCount() const 
	{
		return mruList.size();
	}

	bool getEntry(size_t index, std::wstring &out) const;

private:
	static std::wstring getEntryName(size_t i)
	{
		std::wstring temp = std::wstring(L"RecentFile");
		temp += char(L'0' + i);
		return temp;
	}

	std::list<std::wstring> mruList;
	HMENU mruFileMenu;
};
