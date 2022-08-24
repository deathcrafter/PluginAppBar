#pragma once
class Measure
{
public:
	static std::unordered_map<HWND, Measure*> measureList;
	static HHOOK hook;
	static HHOOK hook2;
	static int ref;
	static HWND primaryHwnd;
	static UINT APPBAR_CALLBACK;

	void* rm; // measure instance
	void* skin; // skin instance
	HWND hwnd; // skin window handle

	WEdge edge;
	int requiredSpace;
	bool registeredAsAppbar;
	RECT assignedRect;
	RectStr assignedRectStr;
	MonitorInfo prevMonitor;
	std::wstring scale;
	std::vector<std::wstring> priorityList;

	bool usePrimaryMonitor;
	bool watchOnly;
	bool dpiAware;

	std::wstring appBarPosSetAction;
	std::wstring fullScreenActivateAction;
	std::wstring fullScreenDeactivatedAction;
	std::wstring monitorDetectedAction;
	std::wstring monitorNotDetectedAction;

	Measure(void* _rm);
	~Measure();

	void Initialize();
	void Finalize();
	void Update();
	void UpdateAppBarPos();
	
private:
	// Measure Management and Hooking
	void AddRef();
	static bool Hook(DWORD threadId);
	static LRESULT CALLBACK CallWndProc(int nCode, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK GetMsgProc(int nCode, WPARAM wParam, LPARAM lParam);
	static void Unhook();
	void DeleteRef();
	
	// AppBar Functions
	void RegisterAppBar(bool);
	void SetAppBarPos();
	void ProcessAppbarMessage(UINT message, LPARAM lParam);
	
	std::wstring ReplaceValuesInBang(std::wstring bang);
	void Execute(LPCWSTR bang);
};
