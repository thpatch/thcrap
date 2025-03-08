#include "thcrap_wrapper.h"
#include <stdbool.h>

// TODO: Get these dynamically from the bundled installer
#define SuppliedCRTVersionMajor		14u
#define SuppliedCRTVersionMinor		22u
#define SuppliedCRTVersionBuild		27821u
#define SuppliedCRTVersionRebuild	0u

typedef enum {
	NotInstalled,
	NeedsUpdate,
	IsCurrent,
	IsNotInstallable
} InstallStatus_t;

static InstallStatus_t CheckCRTStatus()
{
	// Look in the registry
	HKEY Key;
	LSTATUS status;

	status = RegOpenKeyW(
		HKEY_LOCAL_MACHINE,
		L"SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\X86",
		&Key
	);

	InstallStatus_t CrtStatus;

	if (status != ERROR_SUCCESS) {
		CrtStatus = NotInstalled;
	}
	else {
		DWORD key_data;
		DWORD data_size;
		DWORD reg_type;

		data_size = 4;
		status = RegQueryValueExW(Key, L"Installed", NULL, &reg_type, (LPBYTE)&key_data, &data_size);
		if (status != ERROR_SUCCESS || reg_type != REG_DWORD || key_data == 0) {
			CrtStatus = NotInstalled;
			goto CloseKeyAndReturn;
		}

		data_size = 4;
		status = RegQueryValueExW(Key, L"Major", NULL, &reg_type, (LPBYTE)&key_data, &data_size);
		if (status != ERROR_SUCCESS || reg_type != REG_DWORD || key_data < SuppliedCRTVersionMajor) {
			CrtStatus = NeedsUpdate;
			goto CloseKeyAndReturn;
		}

		data_size = 4;
		status = RegQueryValueExW(Key, L"Minor", NULL, &reg_type, (LPBYTE)&key_data, &data_size);
		if (status != ERROR_SUCCESS || reg_type != REG_DWORD || key_data < SuppliedCRTVersionMinor) {
			CrtStatus = NeedsUpdate;
			goto CloseKeyAndReturn;
		}

		data_size = 4;
		status = RegQueryValueExW(Key, L"Bld", NULL, &reg_type, (LPBYTE)&key_data, &data_size);
		if (status != ERROR_SUCCESS || reg_type != REG_DWORD || key_data < SuppliedCRTVersionBuild) {
			CrtStatus = NeedsUpdate;
			goto CloseKeyAndReturn;
		}

		data_size = 4;
		status = RegQueryValueExW(Key, L"Rbld", NULL, &reg_type, (LPBYTE)&key_data, &data_size);
		if (status != ERROR_SUCCESS || reg_type != REG_DWORD || key_data < SuppliedCRTVersionRebuild) {
			CrtStatus = NeedsUpdate;
			goto CloseKeyAndReturn;
		}

		CrtStatus = IsCurrent;
			
	CloseKeyAndReturn:
		RegCloseKey(Key);
	}
	return CrtStatus;
}

typedef enum {
	n_NotInstalled,
	n_MonoInstalled,
	n_OutOfDate
} WineNetError_t;

static void NETShowWineError(WineNetError_t reason) {
	LPCWSTR wineMessage = L"";

	switch (reason) {
	case n_MonoInstalled:
		wineMessage =
			L"It seems you are using Wine and Wine Mono is installed\n"
			L"Unfortunately, Wine Mono does not work with thcrap\n"
			L"Please use the bundled script to install .NET Framework 4.8.0\n"
			L"(the script is located in the \"bin/scripts\" folder and called \"install_dotnet480.sh\")";
		break;
	case n_NotInstalled:
		wineMessage =
			L"It seems you are using Wine and .NET Framework 4.8.0 is not installed\n"
			L".NET Framework 4.8.0 or higher is required to setup thcrap\n"
			L"Please use the bundled script to install .NET Framework 4.8.0\n"
			L"(the script is located in the \"bin/scripts\" folder and called \"install_dotnet480.sh\")";
		break;
	case n_OutOfDate:
		wineMessage =
			L"It seems you are using Wine and .NET Framework is installed\n"
			L"Unfortunately, the installed version is older than 4.8.0\n"
			L"Please create a new wineprefix and use the bundled script\n"
			L"to install .NET Framework 4.8.0\n"
			L"(the script is located in the \"bin/scripts\" folder and called \"install_dotnet480.sh\")";
		break;
	}

	MessageBoxW(NULL, wineMessage, L"Touhou Community Reliant Automatic Patcher", MB_ICONERROR | MB_OK);
}

static InstallStatus_t CheckDotNETStatus(bool isWine) {
	HKEY key;
	LSTATUS status;

	if (isWine) { // Check for Wine Mono
		// Registry functions are easier for this than SetupAPI functions
		status = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
			L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{7E8D2753-698F-5E0D-98AF-5AAD5D228984}",
			0, KEY_QUERY_VALUE | KEY_WOW64_64KEY,
			&key);
		if (status == ERROR_SUCCESS) {
			NETShowWineError(n_MonoInstalled);
			RegCloseKey(key);
			return IsNotInstallable;
		}
		status = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
			L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{EF53CA78-D197-5C46-8003-A0002819B1D7}",
			0, KEY_QUERY_VALUE | KEY_WOW64_64KEY,
			&key);
		if (status == ERROR_SUCCESS) {
			NETShowWineError(n_MonoInstalled);
			RegCloseKey(key);
			return IsNotInstallable;
		}
	}

	status = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
		// NOTE(32th): key "v4" has 2 subkeys: "Client" and "Full" that seem identical.
		//             What are the differences? Which should be used?
		L"SOFTWARE\\Microsoft\\NET Framework Setup\\NDP\\v4\\Client",
		0, KEY_QUERY_VALUE | KEY_WOW64_64KEY,
		&key
	);

	InstallStatus_t NETStatus;

	if (status != ERROR_SUCCESS) {
		NETStatus = NotInstalled;
	}
	else {
		DWORD key_data;
		DWORD data_size = 4;
		DWORD reg_type;

		status = RegQueryValueExW(key, L"Release", NULL, &reg_type, (LPBYTE)&key_data, &data_size);

		if (status != ERROR_SUCCESS || reg_type != REG_DWORD)
			NETStatus = NotInstalled;
		else if (key_data < 0x80EB1)
			NETStatus = NeedsUpdate;
		else
			NETStatus = IsCurrent;
	}
	RegCloseKey(key);
	return NETStatus;
}

// From thcrap_update/src/http_status.h
// Slightly modified since this is C
typedef enum HttpStatus {
	// 200 - success
	HttpOk,
	// Download cancelled by the progress callback, or another client
	// declared the server as dead
	HttpCancelled,
	// 3XX and 4XX - file not found, not accessible, moved, etc.
	HttpClientError,
	// 5XX errors - server errors, further requests are likely to fail.
	// Also covers weird error codes like 1XX and 2XX which we shouldn't see.
	HttpServerError,
	// Error returned by the download library or by the write callback
	HttpSystemError,
	// Error encountered before loading thcrap_update.dll
	HttpLibLoadError
} HttpStatus;

DWORD WINAPI NETDownloadThread(LPVOID lpParam) {
	LPCWSTR ApplicationPath = lpParam;

	wchar_t current_dir[MAX_PATH];
	GetCurrentDirectoryW(MAX_PATH, current_dir);
	SetCurrentDirectoryW(ApplicationPath);
	HMODULE hUpdate = LoadLibraryW(L"thcrap_update" DEBUG_OR_RELEASE L".dll");
	SetCurrentDirectoryW(current_dir);
	if (!hUpdate)
		ExitThread(HttpLibLoadError);

	typedef HttpStatus download_single_file_t(const char* url, const char* fn);
	download_single_file_t* download_single_file = (download_single_file_t*)GetProcAddress(hUpdate, "download_single_file");
	if (!download_single_file)
		ExitThread(HttpLibLoadError);

	HttpStatus res = download_single_file("https://download.visualstudio.microsoft.com/download/pr/7afca223-55d2-470a-8edc-6a1739ae3252/abd170b4b0ec15ad0222a809b761a036/ndp48-x86-x64-allos-enu.exe", "NDP480-Installer.exe");

	FreeLibrary(hUpdate);

	ExitThread(res);
}

int NETDownloadCheckError(HttpStatus reason) {
	wchar_t* errorMessage = L"";
	switch (reason) {
	case HttpOk:
		return 0;
	case HttpLibLoadError:
		errorMessage =
			L"Failed to download .NET Framework 4.8.0\n"
			L"Corrupt or outdated thcrap installation";
		break;
	case HttpCancelled:
		errorMessage =
			L"Failed to download .NET Framework 4.8.0\n"
			L"An error occurred with the download process";
		break;
	case HttpClientError:
		errorMessage =
			L"Failed to download .NET Framework 4.8.0\n"
			L"The file wasn't found on Microsoft's server";
		break;
	case HttpServerError:
		errorMessage =
			L"Failed to download .NET Framework 4.8.0\n"
			L"An unknown error ocurred with Microsoft's server";
		break;
	case HttpSystemError:
		errorMessage =
			L"Failed to download .NET Framework 4.8.0\n"
			L"No internet or not enough system memory";
		break;
	}
	MessageBoxW(NULL, errorMessage, L"Touhou Community Reliant Automatic Patcher", MB_ICONERROR | MB_OK);
	return 1;
}

int installDotNET(LPWSTR ApplicationPath) {
	LPWSTR net_install_message = L"";

	HMODULE hNTDLL = GetModuleHandleW(L"ntdll.dll");
	bool isWine = GetProcAddress(hNTDLL, "wine_get_version") != NULL;

	switch (CheckDotNETStatus(isWine)) {
	case IsCurrent:
		return 0;
	case NotInstalled:
		if (isWine) {
			NETShowWineError(n_NotInstalled);
			return 1;
		}
		net_install_message = L"Installing some required files, please wait...";
		break;
	case NeedsUpdate:
		if (isWine) {
			NETShowWineError(n_OutOfDate);
			return 1;
		}
		net_install_message = L"Updating some required files, please wait...";
		break;
	case IsNotInstallable:
		return 1;
	}

	typedef NTSTATUS NTAPI RtlGetVersion_t(
		PRTL_OSVERSIONINFOW lpVersionInformation
	);
	RtlGetVersion_t* RtlGetVersion = (RtlGetVersion_t*)GetProcAddress(hNTDLL, "RtlGetVersion");

	if (RtlGetVersion) {
		RTL_OSVERSIONINFOW winver;
		my_memset(&winver, 0, sizeof(winver));
		winver.dwOSVersionInfoSize = sizeof(winver);
		RtlGetVersion(&winver);
		if (winver.dwMajorVersion < 6) {
			MessageBoxW(NULL, L"Your Windows version is too old for .NET 4.8.0", L".Touhou Community Reliant Automatic Patcher", MB_ICONERROR | MB_OK);
			return 1;
		}
	}
	else {
		if (MessageBoxW(NULL, L"Couldn't determine Windows version. Proceed anyways?", L"Touhou Community Reliant Automatic Patcher", MB_ICONWARNING | MB_YESNO) != IDYES)
			return 1;
	}

	MessageBoxW(NULL,
		L"We will now download some required files\n"
		L"Should your internet go out, the download\n"
		L"will pause and resume once you have internet.\n\n"

		L"If your internet goes out and you are certain,\n"
		L"you won't reconnect for a long period of time, you\n"
		L"can use Task Manager to force close this process",
	L"Touhou Community Reliant Automatic Patcher", MB_ICONINFORMATION | MB_OK); 

	HANDLE hThread = CreateThread(NULL, 0, NETDownloadThread, ApplicationPath, 0, NULL);
	HWND hwnd = createInstallPopup(L"Downloading some required files");

	MSG msg;
	my_memset(&msg, 0, sizeof(msg));
	DWORD threadExitCode;
	DWORD waitStatus;

	while (1) {
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) && msg.message != WM_QUIT) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		if (msg.message == WM_QUIT) {
			PostQuitMessage((int)msg.wParam);
			break;
		}
		waitStatus = MsgWaitForMultipleObjects(1, &hThread, FALSE, INFINITE, QS_ALLEVENTS);
		if (waitStatus == WAIT_OBJECT_0 + 0) { // Thread finished
			DestroyWindow(hwnd);
			BOOL ret = GetExitCodeThread(hThread, &threadExitCode);
			if (!ret) {
				errorCodeMsg(L"An unknown error with the download thread occurred\n", NULL);
			}
			CloseHandle(hThread);
			if (NETDownloadCheckError(threadExitCode)) {
				return 1;
			}
			break;
		}
	}

	wchar_t current_dir[MAX_PATH];
	GetCurrentDirectoryW(MAX_PATH, current_dir);

	LPWSTR NETInstaller = my_alloc(my_wcslen(current_dir) + my_wcslen(L"\\NDP480-Installer.exe") + 1, sizeof(wchar_t));
	LPWSTR p = NETInstaller;

	p = my_strcpy(p, current_dir);
	p = my_strcpy(p, L"\\NDP480-Installer.exe");

	hwnd = createInstallPopup(net_install_message);

	SHELLEXECUTEINFOW shellex;
	my_memset(&shellex, 0, sizeof(shellex));
	shellex.cbSize = sizeof(shellex);
	shellex.fMask = SEE_MASK_NOCLOSEPROCESS;
	shellex.hwnd = hwnd;
	shellex.lpVerb = L"runas";
	shellex.lpFile = NETInstaller;
	shellex.lpParameters = L"/passive /promptrestart";
	shellex.nShow = SW_SHOW;

	BOOL ret = ShellExecuteExW(&shellex);

	my_free(NETInstaller);

	if (!ret) {
		DestroyWindow(hwnd);
		return 1;
	}

	my_memset(&msg, 0, sizeof(msg));
	while (1) {
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) && msg.message != WM_QUIT) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		if (msg.message == WM_QUIT) {
			PostQuitMessage((int)msg.wParam);
			break;
		}
		waitStatus = MsgWaitForMultipleObjects(1, &shellex.hProcess, FALSE, INFINITE, QS_ALLEVENTS);
		if (waitStatus == WAIT_OBJECT_0 + 0) { // Process finished
			DestroyWindow(hwnd);
			break;
		}
	}

	DeleteFileW(L"NDP480-Installer.exe");

	return 0;
}

void installCrt(LPWSTR ApplicationPath)
{
	LPWSTR crt_install_message = L"";
	switch (CheckCRTStatus()) {
		case IsCurrent:
			return;
		case NotInstalled:
			crt_install_message = L"Installing some required files, please wait...";
			break;
		case NeedsUpdate:
			crt_install_message = L"Updating some required files, please wait...";
			break;
	}

	// Start the VC runtime installer
	STARTUPINFOW si;
	PROCESS_INFORMATION pi;

	my_memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	my_memset(&pi, 0, sizeof(pi));

	LPWSTR RtPath = my_alloc(my_wcslen(ApplicationPath) + my_wcslen(L"\"vc_redist.x86.exe\" /install /quiet /norestart") + 1, sizeof(wchar_t));
	LPWSTR p = RtPath;
	p = my_strcpy(p, L"\"");
	p = my_strcpy(p, ApplicationPath);
	p = my_strcpy(p, L"vc_redist.x86.exe");
	p = my_strcpy(p, L"\" /install /quiet /norestart");
	BOOL ret = CreateProcess(NULL, RtPath, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
	my_free(RtPath);
	CloseHandle(pi.hThread);

	if (!ret) {
		return;
	}

	// Create a window
	HWND hwnd = createInstallPopup(crt_install_message);

	// Wait for the VC runtime installer
	MSG msg;
	my_memset(&msg, 0, sizeof(msg));
	while (1) {
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) && msg.message != WM_QUIT) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		if (msg.message == WM_QUIT) {
			PostQuitMessage((int)msg.wParam);
			break;
		}
		DWORD waitStatus = MsgWaitForMultipleObjects(1, &pi.hProcess, FALSE, INFINITE, QS_ALLEVENTS);
		if (waitStatus == WAIT_OBJECT_0 + 0) { // Process finished
			DestroyWindow(hwnd);
			break;
		}
	}

	// Cleanup
	CloseHandle(pi.hProcess);
}
