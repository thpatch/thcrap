/**
  * Win32 UTF-8 wrapper
  *
  * ----
  *
  * Unicode conversion functions.
  */

#include "win32_utf8.h"

int StringToUTF16(wchar_t *str_w, const char *str_mb, size_t str_len)
{
	int ret;
	extern UINT fallback_codepage;

	if(!str_mb || !str_len) {
		return 0;
	}

	ret = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str_mb, str_len, str_w, str_len);
	if(!ret) {
		if(str_mb[str_len - 1] != 0) {
			// The previous conversion attempt still lingers in [str_w].
			// If we don't clear it in case the original string isn't null-terminated,
			// garbage may show up at the end of the converted string...
			ZeroMemory(str_w, str_len * sizeof(wchar_t));
		}
		ret = MultiByteToWideChar(fallback_codepage, MB_PRECOMPOSED, str_mb, str_len, str_w, str_len);
	}
	return ret;
}

wchar_t* StringToUTF16_VLA(wchar_t *str_w, const char *str_mb, size_t str_len)
{
	if(str_mb) {
		StringToUTF16(str_w, str_mb, str_len);
	} else {
		VLA_FREE(str_w);
	}
	return str_w;
}

int StringToUTF8(char *str_utf8, const wchar_t *str_w, size_t str_len)
{
	size_t str_utf8_len = str_len * sizeof(char) * UTF8_MUL;
	return WideCharToMultiByte(CP_UTF8, 0, str_w, -1, str_utf8, str_utf8_len, NULL, NULL);
}
