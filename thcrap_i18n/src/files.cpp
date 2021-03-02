#include "i18n_private.h"
#include <stdio.h>
static wchar_t i18n_dir[MAX_PATH + sizeof("i18n")-1];
static wchar_t i18n_lang_path[MAX_PATH + sizeof("i18n\\lang.txt")-1];

static wchar_t *my_findfilespec(wchar_t *str)
{
	if (str[0] == L'\\' && str[1] == L'\\')
		str += 2;
	else if (str[0] == L'\\')
		str += 1;
	else if (str[0] && str[1] == L':' && str[2] == L'\\')
		str += 3;

	wchar_t *p = str;
	while (*p)
		p++;
	while (p >= str) {
		if (*p == L'\\')
			break;
		p--;
	}
	return p;
}
static void my_wcscpy(wchar_t *dst, const wchar_t *src)
{
	while (*src)
		*dst++ = *src++;
	*dst = L'\0';
}
bool i18n_init_path()
{
	// NOTE: this is called from DllMain, so we can't use most functions
	// FIXME: MAX_PATH limitation
	GetModuleFileName(i18n_instance, i18n_dir, MAX_PATH);
	i18n_dir[MAX_PATH] = L'\0';
	my_wcscpy(my_findfilespec(i18n_dir), L"i18n");

	GetModuleFileName(i18n_instance, i18n_lang_path, MAX_PATH);
	i18n_lang_path[MAX_PATH] = L'\0';
	my_wcscpy(my_findfilespec(i18n_lang_path), L"i18n\\lang.txt");

	DWORD attr = GetFileAttributes(i18n_dir);
	return attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY);
}
static wchar_t *i18n_make_path(const char *subdir, const char *file)
{
	int len = wcslen(i18n_dir) + (subdir ? 1 + strlen(subdir) : 0) + 1 + strlen(file) + 5 + 1;
	wchar_t *buf = new wchar_t[len];
	if (subdir) {
		wsprintf(buf, L"%s\\%S\\%S.json", i18n_dir, subdir, file);
	}
	else {
		wsprintf(buf, L"%s\\%S.json", i18n_dir, file);
	}
	return buf;
}

json_t *i18n_load_json(const char *subdir, const char *file, size_t flags, json_error_t *error)
{
	json_t *res = nullptr;

	wchar_t *buf = i18n_make_path(subdir, file);
	FILE *f = _wfopen(buf, L"rb");
	if (f) {
		res = json_loadf(f, flags, error);
	}
	else {
		// TODO: log()
	}
	delete[] buf;
	
	return res;
}

bool i18n_exists_json(const char *subdir, const char *file)
{
	wchar_t *buf = i18n_make_path(subdir, file);
	bool res = GetFileAttributes(buf) != INVALID_FILE_ATTRIBUTES;
	delete[] buf;
	return res;
}

int i18n_load_lang(char *buf, size_t len)
{
	buf[0] = '\0';
	FILE *f = _wfopen(i18n_lang_path, L"r");
	if (f) {
		if (fgets(buf, len, f)) {
			char *p = buf + strlen(buf);
			if (p > buf && p[-1] == '\n')
				p[-1] = '\0';
			return 0;
		}
	}
	return -1;
}
int i18n_save_lang(const char *buf)
{
	FILE *f = _wfopen(i18n_lang_path, L"w");
	if (f) {
		fputs(buf, f);
		fputc('\n', f);
		return 0;
	}
	return -1;
}
