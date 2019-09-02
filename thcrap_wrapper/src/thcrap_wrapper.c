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

    lpDisplayBuf = LocalAlloc(LMEM_ZEROINIT, (my_wcslen(format) + my_wcslen(path) + my_wcslen(lpMsgBuf) + 1) * sizeof(WCHAR));
	LPWSTR ptr = lpDisplayBuf;
	ptr = my_strcpy(ptr, L"Could not run ");
	ptr = my_strcpy(ptr, path);
	ptr = my_strcpy(ptr, L": ");
	ptr = my_strcpy(ptr, lpMsgBuf);
    //StringCchPrintf(lpDisplayBuf, LocalSize(lpDisplayBuf) / sizeof(WCHAR), format, path, lpMsgBuf);
    MessageBox(NULL, lpDisplayBuf, L"Touhou Community Reliant Automatic Patcher", MB_OK);

    LocalFree(lpDisplayBuf);
    LocalFree(lpMsgBuf);
}

LPWSTR getStringResource(UINT id)
{
    LPWSTR buffer = NULL;
    size_t size = 128;
    size_t count;

    // LoadString don't give us a way to know the resource size,
    // so we just grow a buffer until it's big enough.
    do {
        if (buffer == NULL) {
            buffer = HeapAlloc(GetProcessHeap(), 0, size);
			if (buffer == NULL) {
				return NULL;
			}
        }
        else {
            size = size * size;
            LPWSTR newBuffer = HeapReAlloc(GetProcessHeap(), 0, buffer, size);
			if (newBuffer == NULL) {
				HeapFree(GetProcessHeap(), 0, buffer);
				return NULL;
			}
			buffer = newBuffer;
        }

        count = LoadStringW(GetModuleHandle(NULL), id, buffer, size / sizeof(WCHAR));
        if (count == 0) {
            HeapFree(GetProcessHeap(), 0, buffer);
            return NULL;
        }
    } while ((count + 1) * sizeof(WCHAR) == size);

    return buffer;
}

int main()
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    LPWSTR rcApplicationPath;
    LPWSTR rcCommandLine;
    LPWSTR commandLineUsed;
	LPWSTR rcApplicationName;

	rcApplicationPath = getStringResource(0);
	rcCommandLine = getStringResource(1);
	rcApplicationName = getStringResource(2);

	LPWSTR ApplicationPath = (LPWSTR)HeapAlloc(GetProcessHeap(), 0, MAX_PATH * sizeof(WCHAR));

	GetModuleFileNameW(NULL, ApplicationPath, MAX_PATH);
	PathRemoveFileSpecW(ApplicationPath);

	if (rcApplicationPath == NULL) {
		PathAppendW(ApplicationPath, L"bin\\");
	}
	else {
		PathAppendW(ApplicationPath, rcApplicationPath);
	}

	installCrt(ApplicationPath);

	PathAppendW(ApplicationPath, rcApplicationName);

	if (rcCommandLine && my_wcscmp(rcCommandLine, L"[self]") == 0) {
		commandLineUsed = GetCommandLineW();
	}
	else {
		commandLineUsed = rcCommandLine;
	}

	for (unsigned int i = 0; i < sizeof(si); i++) ((BYTE*)&si)[i] = 0;
	si.cb = sizeof(si);
	for (unsigned int i = 0; i < sizeof(pi); i++) ((BYTE*)&pi)[i] = 0;

	if (CreateProcess(ApplicationPath, commandLineUsed, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi) == 0) {
        printError(rcApplicationPath ? rcApplicationPath : commandLineUsed);
        if (rcApplicationPath)  HeapFree(GetProcessHeap(), 0, rcApplicationPath);
        if (rcCommandLine)      HeapFree(GetProcessHeap(), 0, rcCommandLine);
        if (rcApplicationName)  HeapFree(GetProcessHeap(), 0, rcApplicationName);
        if (ApplicationPath)    HeapFree(GetProcessHeap(), 0, ApplicationPath);
        return 1;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    if (rcApplicationPath)  HeapFree(GetProcessHeap(), 0, rcApplicationPath);
    if (rcCommandLine)      HeapFree(GetProcessHeap(), 0, rcCommandLine);
    if (rcApplicationName)  HeapFree(GetProcessHeap(), 0, rcApplicationName);
    if (ApplicationPath)    HeapFree(GetProcessHeap(), 0, ApplicationPath);
    return 0;
}
