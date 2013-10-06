/**
  * Touhou Community Reliant Automatic Patcher
  * Update plugin
  *
  * ----
  *
  * wininet.dll functions.
  */

#pragma once

BOOL WINAPI InternetCombineUrlU(
	__in LPCSTR lpszBaseUrl,
	__in LPCSTR lpszRelativeUrl,
	__out_ecount(*lpdwBufferLength) LPSTR lpszBuffer,
	__inout LPDWORD lpdwBufferLength,
	__in DWORD dwFlags
);
#undef InternetCombineUrl
#define InternetCombineUrl InternetCombineUrlU

BOOL WINAPI InternetCrackUrlU(
	__in_ecount(dwUrlLength) LPCSTR lpszUrl,
	__in DWORD dwUrlLength,
	__in DWORD dwFlags,
	__inout LPURL_COMPONENTSA lpUrlComponents
);
#undef InternetCrackUrl
#define InternetCrackUrl InternetCrackUrlU

HINTERNET WINAPI InternetOpenUrlU(
	__in HINTERNET hInternet,
	__in LPCSTR lpszUrl,
	__in_ecount_opt(dwHeadersLength) LPCSTR lpszHeaders,
	__in DWORD dwHeadersLength,
	__in DWORD dwFlags,
	__in_opt DWORD_PTR dwContext
);
#undef InternetOpenUrl
#define InternetOpenUrl InternetOpenUrlU
