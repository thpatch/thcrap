#pragma once

#include <windows.h>
#include <shlwapi.h>

void installCrt(LPWSTR ApplicationPath, BOOL is_x64_crt);
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

#if NDEBUG
# define DEBUG_OR_RELEASE
# define DEBUG_OR_RELEASE_W
#if _M_X64
#define FILE_SUFFIX "_64"
#define FILE_SUFFIX_W L"_64"
#else
#define FILE_SUFFIX
#define FILE_SUFFIX_W
#endif
#else
# define DEBUG_OR_RELEASE "_d"
# define DEBUG_OR_RELEASE_W L"_d"
#if _M_X64
#define FILE_SUFFIX "_64_d"
#define FILE_SUFFIX_W L"_64_d"
#else
#define FILE_SUFFIX "_d"
#define FILE_SUFFIX_W L"_d"
#endif
#endif
