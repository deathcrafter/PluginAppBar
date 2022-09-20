#include "Plugin.h"
#include "Measure.h"
#include "String.h"
#include <mutex>
#include <ShellScalingApi.h>

HINSTANCE MODULE_INSTANCE = NULL;

std::mutex mt;
static std::wstring primaryMonitorId;
static std::wstring availableMonitors;
static std::unordered_map<std::wstring, MonitorInfo> monitors;

BOOL WINAPI DllMain( HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)  // reserved
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		MODULE_INSTANCE = hinstDLL;
		DisableThreadLibraryCalls(hinstDLL); // disable thread library calls, for performance improvement
	default:
		break;
	}
	return TRUE;
}

MonitorInfo GetMonitor(std::vector<std::wstring>& priorities, bool returnPrimary)
{
	for (size_t i = 0; i < priorities.size(); i++) {
		auto iter = monitors.find(priorities[i]);
		if (iter != monitors.end())
			return iter->second;
	}
	if (returnPrimary) return monitors[primaryMonitorId];
	else return MonitorInfo{false};
}

double GetScaleForMonitor(HMONITOR hMon)
{
	UINT dpiX, dpiY;
	if (SUCCEEDED(GetDpiForMonitor(hMon, MDT_DEFAULT, &dpiX, &dpiY)))
		return (double)dpiY / 96.0;
	return 1.0;
}

std::wstring ExtractUniqueMonitorID(std::wstring rawId)
{
	rawId = rawId.substr(11, rawId.size() - 50);
	size_t j = rawId.find_last_of(L'D');
	return rawId.substr(j + 1);
}

BOOL MonitorEnumProc(HMONITOR hMon, HDC hDc, LPRECT rect, LPARAM lParam)
{
	std::pair<std::unordered_map<std::wstring, MonitorInfo>, std::wstring>* pair
		= (std::pair<std::unordered_map<std::wstring, MonitorInfo>, std::wstring>*)lParam;
	MonitorInfo monInfo;
	MONITORINFOEX monInfoEx;
	ZeroMemory(&monInfoEx, sizeof(monInfoEx));
	monInfoEx.cbSize = sizeof(MONITORINFOEX);
	if (GetMonitorInfo(hMon, &monInfoEx))
	{
		monInfo.hMon = hMon;
		monInfo.name = monInfoEx.szDevice;
		monInfo.num = _wtoi(monInfo.name.substr(monInfo.name.size() - 1, monInfo.name.size() - 1).c_str());
		monInfo.rcMon = monInfoEx.rcMonitor;
		monInfo.rcWork = monInfoEx.rcWork;
		if (monInfoEx.dwFlags & MONITORINFOF_PRIMARY)
			monInfo.isPrimary = true;
		DISPLAY_DEVICE dDevice;
		ZeroMemory(&dDevice, sizeof(dDevice));
		dDevice.cb = sizeof(DISPLAY_DEVICE);
		EnumDisplayDevices(monInfo.name.c_str(), 0, &dDevice, EDD_GET_DEVICE_INTERFACE_NAME);
		monInfo.id = dDevice.DeviceID;
		monInfo.id = ExtractUniqueMonitorID(monInfo.id);
		if (monInfo.isPrimary)
			primaryMonitorId = monInfo.id;
		pair->second.append(
			std::to_wstring(monInfo.rcMon.left) + L"|" + std::to_wstring(monInfo.rcMon.top) +
			L"|" +
			std::to_wstring(monInfo.rcMon.right - monInfo.rcMon.left) + L"|" + std::to_wstring(monInfo.rcMon.bottom - monInfo.rcMon.top) +
			L"|" +
			monInfo.id +
			L";"
		);
		monInfo.scale = GetScaleForMonitor(hMon);
		monInfo.scaleStr = std::to_wstring(monInfo.scale);
		pair->first.emplace(monInfo.id, monInfo);
	}
	return TRUE;
}

void UpdateMonitorsInfo()
{
	std::pair<std::unordered_map<std::wstring, MonitorInfo>, std::wstring> pair;
	EnumDisplayMonitors(NULL, NULL, (MONITORENUMPROC)MonitorEnumProc, (LPARAM)&pair);
	{
		std::unique_lock<std::mutex> lk(mt);
		monitors.clear();
		monitors = pair.first;
		availableMonitors.clear();
		availableMonitors = pair.second.substr(0, pair.second.size() - 1);
	}
	for (auto listItem : Measure::measureList)
	{
		listItem.second->Execute(listItem.second->displaySettingsUpdatedAction.c_str());
		listItem.second->UpdateAppBarPos();
	}
}

void UpdateMonitorInfo(MonitorInfo& monInfo)
{
	MONITORINFOEX monInfoEx;
	ZeroMemory(&monInfoEx, sizeof(monInfoEx));
	monInfoEx.cbSize = sizeof(MONITORINFOEX);
	if (GetMonitorInfo(monInfo.hMon, &monInfoEx))
	{
		monInfo.rcMon = monInfoEx.rcMonitor;
		monInfo.rcWork = monInfoEx.rcWork;
		availableMonitors.append(
			std::to_wstring(monInfo.rcMon.left) + L"|" + std::to_wstring(monInfo.rcMon.top) +
			L"|" +
			std::to_wstring(monInfo.rcMon.right - monInfo.rcMon.left) + L"|" + std::to_wstring(monInfo.rcMon.bottom - monInfo.rcMon.top) +
			L"|" +
			monInfo.id +
			L";"
		);
		monInfo.scale = GetScaleForMonitor(monInfo.hMon);
		monInfo.scaleStr = std::to_wstring(monInfo.scale);
	}
}

void UpdateExistingMonitorsInfo()
{
	availableMonitors.clear();
	for (auto monitor : monitors)
	{
		UpdateMonitorInfo(monitor.second);
	}
	for (auto listItem : Measure::measureList)
	{
		listItem.second->Execute(listItem.second->displaySettingsUpdatedAction.c_str());
		listItem.second->UpdateAppBarPos();
	}
}

PLUGIN_EXPORT void Initialize(void** data, void* rm)
{
	Measure* measure = new Measure(rm);
	*data = measure;
	measure->Initialize();
}

PLUGIN_EXPORT void Reload(void* data, void* rm, double* maxValue)
{
	Measure* measure = (Measure*)data;
}

PLUGIN_EXPORT double Update(void* data)
{
	Measure* measure = (Measure*)data;
	measure->Update();
	if (measure->prevMonitor.isValid) {
		return measure->prevMonitor.num;
	}
	return 0.0;
}

PLUGIN_EXPORT void ExecuteBang(void* data, LPCWSTR args)
{
	Measure* measure = (Measure*)data;
	if (measure->watchOnly)
	{
		RmLog(measure->rm, LOG_WARNING, L"Commands not supported in watch only mode!");
		return;
	}
	if (_wcsicmp(args, L"UpdatePosition") == 0)
	{
		measure->Update();
		measure->UpdateAppBarPos();
	}
	else
	{
		RmLogF(measure->rm, LOG_ERROR, L"Invalid command: %s", args);
	}
}

PLUGIN_EXPORT void Finalize(void* data)
{
	Measure* measure = (Measure*)data;
	measure->Finalize();
	delete measure;
}

PLUGIN_EXPORT LPCWSTR AvailableMonitors(void* data, const int argc, WCHAR* argv[])
{
	return availableMonitors.c_str();
}

PLUGIN_EXPORT LPCWSTR RectValue(void* data, const int argc, WCHAR* argv[])
{
	if (argc <= 0)
		return L"0";
	Measure* measure = (Measure*)data;
	int type = _wtoi(argv[0]);
	switch (type)
	{
	case LEFT:
		return measure->assignedRectStr.left.c_str();
	case TOP:
		return measure->assignedRectStr.top.c_str();
	case RIGHT:
		return measure->assignedRectStr.right.c_str();
	case BOTTOM:
		return measure->assignedRectStr.bottom.c_str();
	default:
		RmLogF(
			measure->rm, LOG_ERROR, 
			L"Invalid edge %d. Values: LEFT = 0 | TOP = 1 | RIGHT = 2 | BOTTOM = 3",
			type
		);
		break;
	}
	return L"0";
}

PLUGIN_EXPORT LPCWSTR MonitorScale(void* data, const int argc, WCHAR* argv[])
{
	Measure* measure = (Measure*)data;
	return measure->dpiAware ? measure->prevMonitor.scaleStr.c_str() : L"1.0";
}