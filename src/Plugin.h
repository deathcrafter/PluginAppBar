#pragma once
#include <Windows.h>
#include "../rmapi/RainmeterAPI.h"
#include <shellapi.h>
#include <vector>
#include <string>
#include <unordered_map>
#include "String.h"

// from https://github.com/rainmeter/rainmeter/blob/master/Library/Skin.h
#define WM_METERWINDOW_DELAYED_MOVE WM_APP + 3
#define WM_SKINAPPBAR WM_APP + 12

class Measure;

enum WEdge {
	LEFT,
	TOP,
	RIGHT,
	BOTTOM
};

struct MonitorInfo
{
	bool isValid;
	std::wstring name;
	std::wstring id;
	int num;
	RECT rcMon;
	RECT rcWork;
	bool isPrimary;
	double scale;
	std::wstring scaleStr;

	MonitorInfo() :
		name(),
		id(),
		num(0),
		rcMon(RECT {0}),
		rcWork(RECT {0}),
		isPrimary(false),
		scale(1.0),
		scaleStr(L"1.0"),
		isValid(true)
	{}
	MonitorInfo(bool valid) :
		name(),
		id(),
		num(0),
		rcMon(RECT{ 0 }),
		rcWork(RECT{ 0 }),
		isPrimary(false),
		scale(1.0),
		scaleStr(L"1.0"),
		isValid(valid)
	{}
};

struct RectStr {
	std::wstring left;
	std::wstring top;
	std::wstring right;
	std::wstring bottom;
	RectStr(RECT rc)
	{
		left = std::to_wstring(rc.left);
		top = std::to_wstring(rc.top);
		right = std::to_wstring(rc.right);
		bottom = std::to_wstring(rc.bottom);
	}
	void operator = (RectStr rc)
	{
		left = rc.left;
		top = rc.top;
		right = rc.right;
		bottom = rc.bottom;
	}
};

void UpdateMonitorsInfo();
MonitorInfo GetMonitor(std::vector<std::wstring>& priorities, bool returnPrimary = true);