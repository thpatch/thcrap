/**
  * Win32 UTF-8 wrapper
  *
  * ----
  *
  * Unicode conversion functions.
  */

#include "win32_utf8.h"

int StringToUTF16(wchar_t *str_w, const char *str_mb, int str_len)
{
	extern UINT fallback_codepage;
	int ret;
	int str_len_w;

	if(!str_mb || !str_len) {
		return 0;
	}
	if(str_len == -1) {
		str_len = strlen(str_mb) + 1;
	}
	str_len_w = str_w ? str_len : 0;
	ret = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str_mb, str_len, str_w, str_len_w);
	if(!ret) {
		if(str_mb[str_len - 1] != 0) {
			// The previous conversion attempt still lingers in [str_w].
			// If we don't clear it, garbage may show up at the end of the
			// converted string if the original string wasn't null-terminated...
			ZeroMemory(str_w, str_len * sizeof(wchar_t));
		}
		ret = MultiByteToWideChar(fallback_codepage, MB_PRECOMPOSED, str_mb, str_len, str_w, str_len_w);
	}
	return ret;
}

wchar_t* StringToUTF16_VLA(wchar_t *str_w, const char *str_mb, int str_len)
{
	if(str_mb) {
		StringToUTF16(str_w, str_mb, str_len);
	} else {
		VLA_FREE(str_w);
	}
	return str_w;
}

int StringToUTF8(char *str_utf8, const wchar_t *str_w, int str_utf8_len)
{
	return WideCharToMultiByte(CP_UTF8, 0, str_w, -1, str_utf8, str_utf8_len, NULL, NULL);
}

char* EnsureUTF8(const char *str, int str_len)
{
	if(!str) {
		return NULL;
	}
	{
		size_t str_w_len = str_len + 1;
		size_t str_utf8_len = str_w_len * UTF8_MUL;
		VLA(wchar_t, str_w, str_w_len);
		char *str_utf8 = (char*)malloc(str_utf8_len);
		ZeroMemory(str_w, str_w_len * sizeof(wchar_t));
		ZeroMemory(str_utf8, str_utf8_len * sizeof(char));
		WCHAR_T_CONV(str);
		StringToUTF8(str_utf8, str_w, str_utf8_len);
		VLA_FREE(str_w);
		return str_utf8;
	}
}
