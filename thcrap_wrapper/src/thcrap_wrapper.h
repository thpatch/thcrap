#pragma once

#include <windows.h>
#include <shlwapi.h>

void installCrt(LPWSTR ApplicationPath);
int installDotNET(LPWSTR ApplicationPath);

HWND createInstallPopup(LPWSTR install_message);
void errorCodeMsg(const wchar_t* msg, HWND hParent);

size_t my_wcslen(const wchar_t *str);
int my_wcscmp(const wchar_t *s1, const wchar_t *s2);
// Returns the pointer to the end of dst, so that you can chain the call to append
// several strings.
LPWSTR my_strcpy(LPWSTR dst, LPCWSTR src);
void *my_memcpy(void *dst, const void *src, size_t n);
void *my_memset(void *dst, int ch, size_t n);
void *my_alloc(size_t num, size_t size);
void my_free(void *ptr);

#ifdef _DEBUG
# define DEBUG_OR_RELEASE L"_d"
#else
# define DEBUG_OR_RELEASE
#endif
