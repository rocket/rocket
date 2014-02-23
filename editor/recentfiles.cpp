#include "../lib/base.h"
#include "recentfiles.h"
#include "resource.h"
#include <assert.h>

#define MAX_DIR_LEN 64

static bool setRegString(HKEY key, const std::wstring &name, const std::wstring &value)
{
	return ERROR_SUCCESS == RegSetValueExW(key, name.c_str(), 0, REG_SZ, (BYTE *)value.c_str(), (DWORD)(value.size() + 1) * 2);
}

static bool getRegString(HKEY key, const std::wstring &name, std::wstring &out)
{
	DWORD size = 0;
	DWORD type = 0;
	if (ERROR_SUCCESS != RegQueryValueExW(key, name.c_str(), 0, &type, (LPBYTE)NULL, &size)) return false;
	if (REG_SZ != type) return false;

	assert(!(size % 1));
	out.resize(size / 2);
	DWORD ret = RegQueryValueExW(key, name.c_str(), 0, &type, (LPBYTE)&out[0], &size);
	while (out.size() > 0 && out[out.size() - 1] == L'\0') out.resize(out.size() - 1);

	assert(ret == ERROR_SUCCESS);
	assert(REG_SZ == type);
	assert(size == (out.size() + 1) * 2);

	return true;
}

void RecentFiles::load(HKEY key)
{
	for (size_t i = 0; i < 5; ++i)
	{
		std::wstring fileName;
		if (getRegString(key, getEntryName(i), fileName))
		{
			mruList.push_back(fileName);
		}
	}

	if (mruList.size() > 0) update();
}

void RecentFiles::save(HKEY key)
{
	std::list<std::wstring>::const_iterator it;
	size_t i;
	for (i = 0, it = mruList.begin(); it != mruList.end(); ++it, ++i)
	{
		assert(i <= 5);
		setRegString(key, getEntryName(i), *it);
	}
}

void RecentFiles::insert(const std::wstring &fileName)
{
	mruList.remove(fileName); // remove, if present
	mruList.push_front(fileName); // add to front
	while (mruList.size() > 5) mruList.pop_back(); // remove old entries
}

void RecentFiles::update()
{
	while (0 != RemoveMenu(mruFileMenu, 0, MF_BYPOSITION));
	std::list<std::wstring>::const_iterator it;
	size_t i;
	for (i = 0, it = mruList.begin(); it != mruList.end(); ++it, ++i)
	{
		assert(i <= 5);
		std::wstring menuEntry = std::wstring(L"&");
		menuEntry += wchar_t(L'1' + i);
		menuEntry += L" ";
		
		wchar_t path[_MAX_PATH], drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];
		_wsplitpath(it->c_str(), drive, dir, fname, ext);
		if (wcslen(dir) > MAX_DIR_LEN) wcscpy(dir, L"\\...");
		_wmakepath(path, drive, dir, fname, ext);
		menuEntry += std::wstring(path);

		AppendMenuW(mruFileMenu, MF_STRING, ID_RECENTFILES_FILE1 + i, menuEntry.c_str());
	}
}

bool RecentFiles::getEntry(size_t index, std::wstring &out) const
{
	std::list<std::wstring>::const_iterator it;
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
