#include "i18n_private.h"
#include "resource.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Windowsx.h>
#include <wchar.h>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <jansson.h>
#include <limits.h>
using std::vector;

typedef unsigned(*i18n_plural_func)(unsigned long num);
static unsigned plural_germanic(unsigned long n) {
	return n != 1;
}
static unsigned plural_french(unsigned long n) {
	return n > 1;
}
static unsigned plural_eastslavic(unsigned long n) {
	return n % 10 == 1 && n % 100 != 11 ? 0 :
		n % 10 >= 2 && n % 10 <= 4 && (n % 100<10 || n % 100 >= 20) ? 1 : 2;
}
struct I18nLang {
	const char *langid;
	const char *name;
	const wchar_t *nativename;
	int nplurals;
	i18n_plural_func plural;

	bool operator<(const I18nLang &rhs) const {
		return strcmp(name, rhs.name) < 0;
	}
};

static I18nLang currentLang = {"en", "English", L"English", 2, plural_germanic};
static vector<I18nLang> langs = {
	{ "en", "English", L"English", 2, plural_germanic },
	{ "de", "German", L"Deutsch", 2, plural_germanic },
	{ "fr", "French", L"français", 2, plural_french },
	{ "ru", "Russian", L"русский", 3, plural_eastslavic },
	{ "uk", "Ukrainian", L"українська", 3, plural_eastslavic },
};

const char* i18n_langid() {
	return currentLang.langid;
}
unsigned i18n_plural(unsigned long num) {
	if (num == ULONG_MAX) {
		return 0;
	}
	return currentLang.plural(num);
}

static INT_PTR CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_INITDIALOG: {
		HWND combo = GetDlgItem(hwndDlg, IDC_COMBO1);
		vector<wchar_t> vec;
		for (auto lang = langs.begin(); lang != langs.end(); ++lang) {
			if (lang->langid != "en" && !i18n_exists_json((const char*)lParam, lang->langid)) {
				continue;
			}
			size_t len = strlen(lang->name) + wcslen(lang->nativename) + 4;
			vec.reserve(len);
			wsprintf(vec.data(), L"%S (%s)", lang->name, lang->nativename);
			int index = ComboBox_AddString(combo, vec.data());
			ComboBox_SetItemData(combo, index, lang - langs.begin());
			if (!strcmp(lang->langid, "en")) {
				ComboBox_SetCurSel(combo, index);
			}
		}
		return TRUE;
	}
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_BUTTON1:
			HWND combo = GetDlgItem(hwndDlg, IDC_COMBO1);
			int sel = ComboBox_GetCurSel(combo);
			intptr_t index = ComboBox_GetItemData(combo, sel);
			const I18nLang &lang = langs[index];
			currentLang = lang;
			i18n_save_lang(lang.langid);
			EndDialog(hwndDlg,0);
			return TRUE;
		}
		return FALSE;
	case WM_CLOSE:
		EndDialog(hwndDlg, 0);
		return TRUE;
	}
	return FALSE;
}
void i18n_lang_selector(const char *domain) {
	if (IS_I18N_ENABLED()) {
		DialogBoxParam(i18n_instance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DialogProc, (LPARAM)domain);
	}
}

static bool i18n_try_init() {
	if (IS_I18N_ENABLED()) {
		char lang[32];
		if (!i18n_load_lang(lang, 32) && lang[0]) {
			auto it = std::find_if(langs.begin(), langs.end(), [lang](const I18nLang&l) {
				return !strcmp(l.langid, lang);
			});
			if (it != langs.end()) {
				currentLang = *it;
				return true;
			}
		}
	}
	return false;
}

void i18n_lang_init(const char *domain) {
	if (IS_I18N_ENABLED()) {
		if (!i18n_try_init()) {
			i18n_lang_selector(domain);
		}
	}
}

void i18n_lang_init_quiet() {
	if (IS_I18N_ENABLED()) {
		i18n_try_init();
	}
}
