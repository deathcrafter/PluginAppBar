#pragma once
#include <algorithm> 
#include <cctype>
#include <locale>
#include <string>
#include <vector>

std::wstring ltrim(std::wstring s);
std::wstring rtrim(std::wstring s);
std::wstring trim(std::wstring s);
std::vector<std::wstring> split(std::wstring str, wchar_t* delim);