#include "i18n_private.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unordered_map>
#include <mutex>
#include <string>
#include <atomic>
#include <vector>
#include <stdexcept>
#include <jansson.h>
using std::mutex;
using std::lock_guard;
using std::unordered_map;
using std::string;
using std::wstring;
using std::vector;
using std::atomic_bool;

struct I18nCache {
	json_t *narrow;
	unordered_map<wstring, vector<wstring>> wide;
	bool narrowFailed;
	atomic_bool hasWide;
	bool wideFailed;
	mutex wideConversionMutex;

	I18nCache() {
		narrow = nullptr;
		narrowFailed = false;
		hasWide = false;
		wideFailed = false;
	}
	I18nCache(I18nCache const&) = delete;
	I18nCache& operator=(I18nCache const&) = delete;
	~I18nCache() {
		json_decref(narrow);
		narrow = nullptr;
	}

	static wstring utf8_to_wide(vector<wchar_t>& buf, const char *utf8) {
		int reqsize = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
		buf.reserve(reqsize);
		MultiByteToWideChar(CP_UTF8, 0, utf8, -1, buf.data(), buf.capacity());
		return wstring(buf.data());
	}
	void assert_wide() {
		if (!hasWide) {
			lock_guard<mutex> lock(wideConversionMutex);
			if (hasWide) {
				return;
			}
			hasWide = true;
			if (narrowFailed) {
				return;
			}

			unsigned i;
			const char *k;
			json_t *v, *vv;
			vector<wchar_t> buf;
			buf.reserve(512);
			json_object_foreach(narrow, k, v) {
				if (json_is_array(v)) {
					vector<wstring> vec;
					vec.reserve(json_array_size(v));
					json_array_foreach(v, i, vv) {
						if (json_is_string(v)) {
							//log("Expected string");
							wideFailed = true;
							return;
						}
						vec.push_back(utf8_to_wide(buf, json_string_value(vv)));
					}
					wide[utf8_to_wide(buf, k)] = std::move(vec);
				}
				else if (json_is_string(v)) {
					vector<wstring> vec;
					vec.push_back(utf8_to_wide(buf, json_string_value(v)));
					wide[utf8_to_wide(buf, k)] = std::move(vec);
				}
				else {
					//log("Expected string or array");
					wideFailed = true;
					return;
				}
			}
		}
	}
};

static mutex g_cachemutex;
static unordered_map<string, I18nCache*> g_cache;

void i18n_load_narrow(const char *domain) {
	if (IS_I18N_ENABLED()) {
		if (domain == lastdomain_raw) return;
		lastdomain_raw = domain;
		if (*lastdomain == domain) return;
		*lastdomain = domain;

		{
			lock_guard<mutex> lock(g_cachemutex);
			auto it = g_cache.find(*lastdomain);
			if (it == g_cache.end()) {
				lastcache = nullptr;
			}
			else {
				lastcache = it->second;
			}
		}

		if (!lastcache) {
			lastcache = new I18nCache;
			lastcache->narrow = i18n_load_json(domain, i18n_langid(), 0, nullptr);
			lastcache->narrowFailed = json_is_object(lastcache->narrow);
		}
	}
}
void i18n_load_wide(const char *domain) {
	if (IS_I18N_ENABLED()) {
		i18n_load_narrow(domain);
		lastcache->assert_wide();
	}
}
void i18n_load_narrow_only(const char *domain) {
	if (IS_I18N_ENABLED()) {
		i18n_load_narrow(domain);
		lock_guard<mutex> lock(lastcache->wideConversionMutex);
		lastcache->hasWide = true;
		lastcache->wideFailed = true;
	}
}
void i18n_load_wide_only(const char *domain) {
	if (IS_I18N_ENABLED()) {
		i18n_load_narrow(domain);
		lastcache->assert_wide();
		lastcache->narrowFailed = true;
		json_decref(lastcache->narrow);
		lastcache->narrow = nullptr;
	}
}

const char *i18n_translate(const char *domain, const char *s, const char *def_s, const char *def_p, unsigned long num) {
	if (IS_I18N_ENABLED()) {
		i18n_load_narrow(domain);
		unsigned form = _i18n_plural(num);
		json_t* json = json_object_get(lastcache->narrow, s);
		if (json) {
			if (json_is_array(json)) {
				json = json_array_get(json, form);
			}

			if (json_is_string(json)) {
				const char *str = json_string_value(json);
				return str;
			}
		}
	}
	return num == 1 ? def_s : def_p;
}

const wchar_t *i18n_translate_wide(const char *domain, const wchar_t *s, const wchar_t *def_s, const wchar_t *def_p, unsigned long num) {
	if (IS_I18N_ENABLED()) {
		i18n_load_wide(domain);
		unsigned form = _i18n_plural(num);
		auto it = lastcache->wide.find(s);
		if (it != lastcache->wide.end() && form < it->second.size()) {
			return it->second[form].c_str();
		}
	}
	return num==1?def_s:def_p;
}
