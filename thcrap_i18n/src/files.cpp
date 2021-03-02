#include "i18n_private.h"
#include <stdio.h>
static wchar_t i18n_dir[MAX_PATH + 4]; // strlen("i18n")

bool i18n_init_path() {
	// NOTE: this is called from DllMain
	// FIXME: MAX_PATH limitation
	GetModuleFileName(i18n_instance, i18n_dir, MAX_PATH);
	// poor man's wcsrchr
	wchar_t *p = i18n_dir;
	while (*p) p++;
	for (; p >= i18n_dir; p--) {
		if (*p == L'\\' || *p == L'/') {
			break;
		}
	}
	// poor man's wcscpy
	*++p = L'i';
	*++p = L'1';
	*++p = L'8';
	*++p = L'n';
	*++p = L'\0';

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

int i18n_dump_json(const json_t *json, const char *subdir, const char *file, size_t flags)
{
	int res = -1;

	wchar_t *buf = i18n_make_path(subdir, file);
	FILE *f = _wfopen(buf, L"w");
	if (f) {
		res = json_dumpf(json, f, flags);
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
