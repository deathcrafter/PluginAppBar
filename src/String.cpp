#include "String.h"

// trim from start (in place)
std::wstring ltrim(std::wstring s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
		return !std::isspace(ch);
		}));
	return s;
}

// trim from end (in place)
std::wstring rtrim(std::wstring s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
		return !std::isspace(ch);
		}).base(), s.end());
	return s;
}

// trim from both ends (in place)
std::wstring trim(std::wstring s) {
	ltrim(s);
	rtrim(s);
	return s;
}

std::vector<std::wstring> split(std::wstring str, wchar_t* delim)
{
	std::vector<std::wstring> strings;
	size_t start;
	size_t end = 0;
	while ((start = str.find_first_not_of(delim, end)) != std::wstring::npos) {
		end = str.find(delim, start);
		strings.push_back(trim(str.substr(start, end - start)));
	}
	return strings;
}