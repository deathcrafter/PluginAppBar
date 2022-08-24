#include "Plugin.h"
#include "Measure.h"

extern HINSTANCE MODULE_INSTANCE;
std::unordered_map<HWND, Measure*> Measure::measureList;
HHOOK Measure::hook = NULL;
HHOOK Measure::hook2 = NULL;
int Measure::ref = 0;
HWND Measure::primaryHwnd = NULL;
UINT Measure::APPBAR_CALLBACK = RegisterWindowMessage(L"RainmeterAppbarCallback");

Measure::Measure(void* _rm) :
	rm(_rm),
	skin(RmGetSkin(_rm)),
	hwnd(RmGetSkinWindow(_rm)),

	priorityList(),
	requiredSpace(0),
	edge(WEdge::TOP),
	registeredAsAppbar(false),

	assignedRect(RECT{ 0 }),
	assignedRectStr(RectStr{ assignedRect }),

	usePrimaryMonitor(true),
	watchOnly(false),
	dpiAware(false)
{
	prevMonitor.isValid = false;
}

Measure::~Measure()
{
}

#pragma region Plugin Functions

void Measure::Initialize()
{
	hwnd = RmGetSkinWindow(rm);
	requiredSpace = RmReadInt(rm, L"RequiredSpace", 10);
	std::wstring edge = RmReadString(rm, L"Edge", L"top");
	if (_wcsicmp(edge.c_str(), L"left") == 0)
		edge = LEFT;
	else if (_wcsicmp(edge.c_str(), L"right") == 0)
		edge = RIGHT;
	else if (_wcsicmp(edge.c_str(), L"bottom") == 0)
		edge = BOTTOM;
	else
		edge = TOP;
	priorityList = split(RmReadString(rm, L"PriorityList", L""), L";");
	if (priorityList.empty())
		usePrimaryMonitor = true;
	else
		usePrimaryMonitor = RmReadInt(rm, L"UsePrimaryMonitorByDefault", 0) == 1;
	dpiAware = RmReadInt(rm, L"DPIAware", 0) == 1;
	watchOnly = RmReadInt(rm, L"WatchOnly", 0) == 1;

	appBarPosSetAction = RmReadString(rm, L"AppBarPosSetAction", L"", FALSE);
	// RmLog(rm, LOG_NOTICE, appBarPosSetAction.c_str());
	fullScreenActivateAction = RmReadString(rm, L"FullScreenActivateAction", L"", FALSE);
	fullScreenDeactivatedAction = RmReadString(rm, L"FullScreenDeactivateAction", L"", FALSE);
	monitorDetectedAction = RmReadString(rm, L"MonitorDetectedAction", L"", FALSE);
	monitorNotDetectedAction = RmReadString(rm, L"MonitorNotDetectedAction", L"", FALSE);

	AddRef();
	if (watchOnly)
		return;
	RegisterAppBar(true);
	if (registeredAsAppbar)
		UpdateAppBarPos();
	else
		RmLog(rm, LOG_ERROR, L"Couldn't register appbar!");
}

void Measure::Update()
{
}

void Measure::Finalize()
{
	RegisterAppBar(false);
	DeleteRef();
}

#pragma endregion

#pragma region Measure Management and Hooking

void Measure::AddRef()
{
	auto found = measureList.find(hwnd);
	if (found != measureList.end())
		return;
	measureList.emplace(hwnd, this);
	primaryHwnd = hwnd;
	if (NULL == ref++) {
		if (!Hook(GetWindowThreadProcessId(hwnd, NULL)))
			RmLogF(rm, LOG_ERROR, L"Couldn't set message hook with error: %ld", GetLastError());
		else
			RmLog(rm, LOG_DEBUG, L"Hooked successfully!");
		UpdateMonitorsInfo();
	}
}

bool Measure::Hook(DWORD threadId)
{
	if (hook && hook2)
		return true;
	if (!hook)
		hook = SetWindowsHookEx(WH_CALLWNDPROC, Measure::CallWndProc, MODULE_INSTANCE, threadId);
	if (!hook2)
		hook2 = SetWindowsHookEx(WH_GETMESSAGE, Measure::GetMsgProc, MODULE_INSTANCE, threadId);
	return (NULL != hook) && (NULL != hook2);
}

LRESULT Measure::CallWndProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode < 0)
		return CallNextHookEx(NULL, nCode, wParam, lParam);

	CWPSTRUCT* st = (CWPSTRUCT*)lParam;
	bool exists = measureList.find(st->hwnd) != measureList.end();
	if (!exists)
		return CallNextHookEx(NULL, nCode, wParam, lParam);

	switch (st->message) {
	case WM_WINDOWPOSCHANGED:
		APPBARDATA data;
		ZeroMemory(&data, sizeof(data));
		data.cbSize = sizeof(APPBARDATA);
		data.hWnd = st->hwnd;
		SHAppBarMessage(ABM_WINDOWPOSCHANGED, &data);
		break;
	case WM_DISPLAYCHANGE: // not handling because wm_settingchange is always sent after wm_displaychange
		break;
	case WM_DPICHANGED: // not handling because wm_settingchange is always sent after wm_dpichange
		break;
	case WM_SETTINGCHANGE:
		if (st->hwnd == primaryHwnd && st->wParam == SPI_SETWORKAREA)
			UpdateMonitorsInfo();
		break;
	default:
		break;
	}
	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

LRESULT Measure::GetMsgProc(int nCode, WPARAM wParam, LPARAM lParam)
{
	if (nCode < 0)
		return CallNextHookEx(NULL, nCode, wParam, lParam);

	MSG* msg = (MSG*)lParam;
	bool found = measureList.find(msg->hwnd) != measureList.end();
	if (!found)
		return CallNextHookEx(NULL, nCode, wParam, lParam);

	if (msg->message == APPBAR_CALLBACK)
		measureList[msg->hwnd]->ProcessAppbarMessage((UINT)msg->wParam, msg->lParam);

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void Measure::Unhook()
{
	if (hook)
		while (!UnhookWindowsHookEx(hook));
	if (hook2)
		while (!UnhookWindowsHookEx(hook2));
	hook = NULL;
	hook2 = NULL;
}

void Measure::DeleteRef()
{
	auto found = measureList.find(hwnd);
	if (found == measureList.end())
		return;
	measureList.erase(found);
	if (NULL == --ref)
		Unhook();
	else if (primaryHwnd == hwnd)
		for (auto listItem : measureList) {
			primaryHwnd = listItem.second->hwnd;
			RmLogF(
				rm, LOG_NOTICE,
				L"PluginAppBar: Primary measure changed to: %s [%s]",
				RmGetSkinName(listItem.second->rm),
				RmGetMeasureName(listItem.second->rm)
			);
			break;
		}
}

#pragma endregion

#pragma region AppBar Functions

void Measure::RegisterAppBar(bool add)
{
	APPBARDATA data;
	ZeroMemory(&data, sizeof(APPBARDATA));
	data.cbSize = sizeof(APPBARDATA);
	data.hWnd = hwnd;
	if (add) {
		data.uCallbackMessage = APPBAR_CALLBACK;
		registeredAsAppbar = SHAppBarMessage(ABM_NEW, &data);
	}
	else {
		SHAppBarMessage(ABM_REMOVE, &data);
		SHAppBarMessage(ABM_REMOVE, &data); // called twice because calling once doesn't seem to work
		registeredAsAppbar = false;
	}
}

void Measure::UpdateAppBarPos()
{
	MonitorInfo monitor = GetMonitor(priorityList, usePrimaryMonitor);
	if (!monitor.isValid) {
		Execute(monitorNotDetectedAction.c_str());
		return;
	}
	else {
		Execute(monitorDetectedAction.c_str());
	}
	if (!registeredAsAppbar)
		return;
	if (
		EqualRect(&monitor.rcMon, &prevMonitor.rcMon)
		&& monitor.id == prevMonitor.id
		&& monitor.scale == prevMonitor.scale
		)
	{
		return;
	}
	prevMonitor = monitor;
	SetAppBarPos();
	assignedRectStr = RectStr{ assignedRect };
	// RmLog(rm, LOG_NOTICE, ReplaceValuesInBang(appBarPosSetAction).c_str());
	Execute(ReplaceValuesInBang(appBarPosSetAction).c_str());
}

void Measure::SetAppBarPos()
{
	APPBARDATA data;
	ZeroMemory(&data, sizeof(APPBARDATA));
	data.cbSize = sizeof(APPBARDATA);
	data.hWnd = hwnd;
	data.uEdge = edge;
	int actualRequiredSpace = ceil(requiredSpace * (dpiAware ? prevMonitor.scale : 1.0f));
	switch (edge)
	{
	case LEFT:
		data.rc.left = prevMonitor.rcMon.left;
		data.rc.top = prevMonitor.rcMon.top;
		data.rc.right = prevMonitor.rcMon.left + actualRequiredSpace;
		data.rc.bottom = prevMonitor.rcMon.bottom;
		break;
	case TOP:
		data.rc.left = prevMonitor.rcMon.left;
		data.rc.top = prevMonitor.rcMon.top;
		data.rc.right = prevMonitor.rcMon.right;
		data.rc.bottom = prevMonitor.rcMon.top + actualRequiredSpace;
		break;
	case RIGHT:
		data.rc.left = prevMonitor.rcMon.right - actualRequiredSpace;
		data.rc.top = prevMonitor.rcMon.top;
		data.rc.right = prevMonitor.rcMon.right;
		data.rc.bottom = prevMonitor.rcMon.bottom;
		break;
	case BOTTOM:
		data.rc.left = prevMonitor.rcMon.left;
		data.rc.top = prevMonitor.rcMon.bottom - actualRequiredSpace;
		data.rc.right = prevMonitor.rcMon.right;
		data.rc.bottom = prevMonitor.rcMon.bottom;
		break;
	}
	SHAppBarMessage(ABM_QUERYPOS, &data);
	assignedRect = data.rc;
	RmLogF(rm, LOG_NOTICE, L"Ask H: %d, Asgn H: %d", actualRequiredSpace, data.rc.bottom - data.rc.top);
	SHAppBarMessage(ABM_SETPOS, &data);
}

void Measure::ProcessAppbarMessage(UINT message, LPARAM lParam)
{
	switch (message)
	{
	case ABN_FULLSCREENAPP:
		Execute(
			(BOOL)lParam ?
			fullScreenActivateAction.c_str() :
			fullScreenDeactivatedAction.c_str()
		);
		break;
	case ABN_POSCHANGED:
		UpdateAppBarPos();
		break;
	// https://docs.microsoft.com/en-us/windows/win32/shell/application-desktop-toolbars#appbar-notification-messages	
	/*
	* An appbar can use ABN_WINDOWARRANGE messages to exclude itself from the cascade or tile operation. 
	* To exclude itself, the appbar should hide itself when lParam is TRUE and show itself when lParam is FALSE. 
	* If an appbar hides itself in response to this message, it does not need to send 
	* the ABM_QUERYPOS and ABM_SETPOS messages in response to the cascade or tile operation.
	*/
	case ABN_WINDOWARRANGE:
		if ((BOOL)lParam)
			ShowWindow(hwnd, SW_HIDE);
		else
			ShowWindow(hwnd, SW_SHOW);
		break;
	// do not process any other messages
	case ABN_STATECHANGE: // notifies about taskbar state changes, e.g. autohide/position
	default:
		break;
	}
}

#pragma endregion

std::wstring Measure::ReplaceValuesInBang(std::wstring bang)
{
	std::vector<std::wstring> tokens = split(bang, L"$");
	std::wstring ret;
	for (size_t i = 0; i < tokens.size(); i++)
	{
		if (_wcsicmp(tokens[i].c_str(), L"left") == 0) tokens[i] = std::to_wstring(assignedRect.left);
		else if (_wcsicmp(tokens[i].c_str(), L"top") == 0) tokens[i] = std::to_wstring(assignedRect.top);
		else if (_wcsicmp(tokens[i].c_str(), L"right") == 0) tokens[i] = std::to_wstring(assignedRect.right);
		else if (_wcsicmp(tokens[i].c_str(), L"bottom") == 0) tokens[i] = std::to_wstring(assignedRect.bottom);
		ret.append(tokens[i]);
	}
	return ret;
}

void Measure::Execute(LPCWSTR bang)
{
	RmExecute(skin, RmReplaceVariables(rm, bang));
}