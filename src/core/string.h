#pragma once

#include <string>

// https://codingtidbit.com/2020/02/09/c17-codecvt_utf8-is-deprecated/
static std::string wstringToString(const std::wstring_view& wstr)
{
	if (wstr.empty())
	{
		return std::string();
	}
	int size = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
	std::string result(size, 0);
	WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &result[0], size, NULL, NULL);
	return result;
}

static std::wstring stringToWstring(const std::string_view& str)
{
	if (str.empty())
	{
		return std::wstring();
	}
	int size = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
	std::wstring result(size, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &result[0], size);
	return result;
}

constexpr uint32 hashString32(const char* s)
{
	uint32 hash = 2166136261u;
	while (*s)
	{
		hash = 16777619u * (hash ^ (uint32)(*s++));
	}
	return hash;
}

constexpr uint64 hashString64(const char* s)
{
	uint64 hash = 14695981039346656037llu;
	while (*s)
	{
		hash = 1099511628211llu * (hash ^ (uint64)(*s++));
	}
	return hash;
}

static inline std::string getTimeString()
{
	time_t now = time(0);
	char nowString[100];
	ctime_s(nowString, 100, &now);
	std::string time = nowString;
	std::replace(time.begin(), time.end(), ' ', '_');
	std::replace(time.begin(), time.end(), ':', '.');
	time.pop_back(); // Pop last \n.

	return time;
}

static inline bool contains(const std::string_view& s, const char* sub)
{
	return s.find(sub) != std::string::npos;
}

static inline bool endsWith(const std::string_view& s, const std::string_view& sub)
{
	if (s.length() >= sub.length())
	{
		return s.compare(s.length() - sub.length(), sub.length(), sub) == 0;
	}
	return false;
}
