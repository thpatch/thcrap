#include <windows.h>

// A quick implementation of a few CRT functions because we don't link with the CRT.
size_t my_wcslen(const wchar_t *str)
{
	size_t n = 0;
	while (str[n]) {
		n++;
	}
	return n;
}

// Returns the pointer to the end of dst, so that you can chain the call to append
// several strings.
LPWSTR my_strcpy(LPWSTR dst, LPCWSTR src)
{
	while (*src) {
		*dst = *src;
		src++;
		dst++;
	}
	*dst = '\0';
	return dst;
}

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
        }
        else {
            size = size * size;
            buffer = HeapReAlloc(GetProcessHeap(), 0, buffer, size);
        }

        count = LoadString(GetModuleHandle(NULL), id, buffer, size);
        if (count == 0) {
            HeapFree(GetProcessHeap(), 0, buffer);
            return NULL;
        }
    } while (count + 1 == size);

    return buffer;
}

int main()
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    LPWSTR lpApplicationName;
    LPWSTR lpCommandLine;
    LPWSTR lpCurrentDirectory;

    lpApplicationName  = getStringResource(0);
    lpCommandLine      = getStringResource(1);
    lpCurrentDirectory = getStringResource(2);

    for (unsigned int i = 0; i < sizeof(si); i++) ((BYTE*)&si)[i] = 0;
    si.cb = sizeof(si);
    for (unsigned int i = 0; i < sizeof(pi); i++) ((BYTE*)&pi)[i] = 0;

    if (CreateProcess(lpApplicationName, lpCommandLine, NULL, NULL, FALSE, 0, NULL, lpCurrentDirectory, &si, &pi) == 0) {
        printError(lpApplicationName ? lpApplicationName : lpCommandLine);
        if (lpApplicationName)  HeapFree(GetProcessHeap(), 0, lpApplicationName);
        if (lpCommandLine)      HeapFree(GetProcessHeap(), 0, lpCommandLine);
        if (lpCurrentDirectory) HeapFree(GetProcessHeap(), 0, lpCurrentDirectory);
        return 1;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    if (lpApplicationName)  HeapFree(GetProcessHeap(), 0, lpApplicationName);
    if (lpCommandLine)      HeapFree(GetProcessHeap(), 0, lpCommandLine);
    if (lpCurrentDirectory) HeapFree(GetProcessHeap(), 0, lpCurrentDirectory);
    return 0;
}
