#pragma once

#include <string>
#include <windows.h>

class IniFile
{
protected:
	static std::string filename;
	static std::string section;
public:

	static std::string getFullPath(std::string filename)
	{
		char fullpath[100];
		char* dummy;
		GetFullPathName(filename.c_str(), 100, fullpath, &dummy);
		return std::string(fullpath);
	}
	static void load(std::string filename)
	{ 
		IniFile::filename = getFullPath(filename);
	}
	static void load(std::string filename, std::string section)
	{ 
		load(filename);
		setSection(section);
	}
	static void setSection(std::string section)
	{
		IniFile::section = section;
	}
	static bool check()
	{
		char buf[10];
		return (GetPrivateProfileSectionNames( buf, 10, filename.c_str() )) ? true : false;
	}
	static int get(std::string filename, std::string section, std::string key, int defaultValue=INT_MAX)
	{
		return GetPrivateProfileInt(section.c_str(), key.c_str(), defaultValue, filename.c_str());
	}
	static int get(std::string section, std::string key, int defaultValue=INT_MAX)
	{
		return get(filename, section, key, defaultValue);
	}
	static int get(std::string key, int defaultValue=INT_MAX)
	{
		return get(filename, section, key, defaultValue);
	}	
	static void read(int &var, std::string filename, std::string section, std::string key)
	{
		var = get(filename, section, key, var);
	}
	static void read(int &var, std::string section, std::string key)
	{
		var = get(filename, section, key, var);
	}
	static void read(int &var, std::string key)
	{
		var = get(filename, section, key, var);
	}
};
