#include "thcrap_wrapper.h"

void printError(LPCWSTR path)
{
    LPCWSTR format = L"Could not run %s: %s";
    LPWSTR lpMsgBuf;
    LPWSTR lpDisplayBuf;
    DWORD dw = GetLastError();

	if (path == NULL) {
		path = L"(null)";
	}

    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
	NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&lpMsgBuf, 0, NULL);

    lpDisplayBuf = my_alloc(my_wcslen(format) + my_wcslen(path) + my_wcslen(lpMsgBuf) + 1, sizeof(WCHAR));
	LPWSTR ptr = lpDisplayBuf;
	ptr = my_strcpy(ptr, L"Could not run ");
	ptr = my_strcpy(ptr, path);
	ptr = my_strcpy(ptr, L": ");
	ptr = my_strcpy(ptr, lpMsgBuf);
    //StringCchPrintf(lpDisplayBuf, LocalSize(lpDisplayBuf) / sizeof(WCHAR), format, path, lpMsgBuf);
    MessageBox(NULL, lpDisplayBuf, L"Touhou Community Reliant Automatic Patcher", MB_OK);

    my_free(lpDisplayBuf);
    LocalFree(lpMsgBuf);
}

LPWSTR getStringResource(UINT id)
{
    LPWSTR resource = NULL;
    size_t count = LoadStringW(GetModuleHandle(NULL), id, (LPWSTR)&resource, 0);
    if (count == 0 || resource == NULL)
        return NULL;

    LPWSTR buffer = my_alloc(count + 1, sizeof(WCHAR));
    if (buffer == NULL)
        return NULL;

    my_memcpy(buffer, resource, count * sizeof(WCHAR));
    buffer[count] = L'\0';
    return buffer;
}

int main()
{
    UINT exitCode = 0;
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    LPWSTR rcApplicationPath;
    LPWSTR rcCommandLine;
    LPWSTR commandLineUsed;
	LPWSTR rcApplicationName;

	rcApplicationPath = getStringResource(0);
	rcCommandLine = getStringResource(1);
	rcApplicationName = getStringResource(2);

	LPWSTR ApplicationPath = my_alloc(MAX_PATH, sizeof(WCHAR));

	GetModuleFileNameW(NULL, ApplicationPath, MAX_PATH);
	PathRemoveFileSpecW(ApplicationPath);

	if (rcApplicationPath == NULL) {
		PathAppendW(ApplicationPath, L"bin\\");
	}
	else if (!PathIsRelative(rcApplicationPath)) {
		my_strcpy(ApplicationPath, rcApplicationPath);
	}
	else {
		PathAppendW(ApplicationPath, rcApplicationPath);
	}

	installCrt(ApplicationPath);

    if (my_wcscmp(rcApplicationName, L"thcrap_configure_v3" DEBUG_OR_RELEASE L".exe") == 0) {
        if (installDotNET(ApplicationPath))
            ExitProcess(0);
    }

	PathAppendW(ApplicationPath, rcApplicationName);

	if (rcCommandLine && my_wcscmp(rcCommandLine, L"[self]") == 0) {
		commandLineUsed = GetCommandLineW();
	}
	else {
		commandLineUsed = rcCommandLine;
	}

    my_memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
    my_memset(&pi, 0, sizeof(pi));

	if (CreateProcess(ApplicationPath, commandLineUsed, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi) == 0) {
        printError(rcApplicationPath ? rcApplicationPath : commandLineUsed);
        exitCode = 1;
    } else {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    my_free(rcApplicationPath);
    my_free(rcCommandLine);
    my_free(rcApplicationName);
    my_free(ApplicationPath);
    ExitProcess(exitCode);
}
