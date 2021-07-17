#include "thcrap_wrapper.h"

// TODO: Get these dynamically from the bundled installer
#define SuppliedCRTVersionMajor		14u
#define SuppliedCRTVersionMinor		22u
#define SuppliedCRTVersionBuild		27821u
#define SuppliedCRTVersionRebuild	0u

typedef enum {
	NotInstalled,
	NeedsUpdate,
	IsCurrent
} CrtStatus_t;

static CrtStatus_t CheckCRTStatus()
{
	// Look in the registry
	HKEY Key;
	LSTATUS status;

	status = RegOpenKeyW(
		HKEY_LOCAL_MACHINE,
		L"SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\X86",
		&Key
	);

	CrtStatus_t CrtStatus;

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
			PostQuitMessage(msg.wParam);
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
