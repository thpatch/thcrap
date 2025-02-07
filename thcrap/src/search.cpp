/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Games search on disk
  */

#include <thcrap.h>
#include <filesystem>
#include <set>
#include <algorithm>
namespace fs = std::filesystem;

typedef struct alignas(16) {
	size_t size_min;
	size_t size_max;
	json_t *versions;
	std::vector<game_search_result> found;
	std::set<std::string> previously_known_games;
	CRITICAL_SECTION cs_result;
} search_state_t;

struct search_thread_param
{
	search_state_t *state;
	const wchar_t *dir;
};

static bool searchCancelled = false;

template<typename T>
static size_t iter_size(T begin, T end)
{
    size_t size = 0;
    while (begin != end) {
        size++;
        begin++;
    }
    return size;
}

// If thcrap and the game are likely to be moved together, we want a relative path,
// otherwise we want an absolute path.
char *SearchDecideStoredPathForm(std::filesystem::path target, std::filesystem::path self)
{
	auto ret = [](const std::filesystem::path& path) {
		char *return_value = strdup(path.u8string().c_str());
		str_slash_normalize(return_value);
		return return_value;
	};

	if (!self.is_absolute()) {
        self = std::filesystem::absolute(self);
    }
    if (!target.is_absolute()) {
        target = std::filesystem::absolute(target);
    }

    if (target.root_path() != self.root_path()) {
        return ret(target);
    }

    auto [self_diff, target_diff] = std::mismatch(self.begin(), self.end(), target.begin(), target.end());
    // These numbers were decided arbitrarly by making a list of path examples that seemed
    // to make sense and looking at which values they need.
    // These examples are available in the unit tests.
    if (iter_size(self_diff, self.end()) <= 3 && iter_size(target_diff, target.end()) <= 2) {
        return ret(target.lexically_relative(self));
    }
    else {
        return ret(target);
    }
}

static int SearchCheckExe(search_state_t& state, const fs::directory_entry &ent)
{
	int ret = 0;
	game_version *ver = identify_by_size((size_t)ent.file_size(), state.versions);
	if(ver) {
#if !CPP20
		std::string exe_fn = ent.path().u8string();
#else
		std::u8string exe_fn = ent.path().u8string();
#endif
		size_t file_size = (size_t)ent.file_size();
		identify_free(ver);
		ver = identify_by_hash((const char*)exe_fn.c_str(), &file_size, state.versions);
		if(!ver) {
			return ret;
		}

		// Check if user already selected a version of this game in a previous search
		if(state.previously_known_games.count(ver->id) > 0) {
			identify_free(ver);
			return ret;
		}

		// Alright, found a game!
		// Check if it has a vpatch
		auto vpatch_fn = ent.path().parent_path() / L"vpatch.exe";
		bool use_vpatch = false;
		if (strstr(ver->id, "_custom") == nullptr && fs::is_regular_file(vpatch_fn)) {
			use_vpatch = true;
		}

		EnterCriticalSection(&state.cs_result);
		{
			std::string description;
			if (ver->build || ver->variety) {
				if (ver->build)                 description += ver->build;
				if (ver->build && ver->variety) description += " ";
				if (ver->variety)               description += ver->variety;
			}

			game_search_result result = std::move(*ver);
			result.path = SearchDecideStoredPathForm(ent.path(), std::filesystem::current_path());
			result.description = strdup(description.c_str());
			identify_free(ver);

			state.found.push_back(result);

			if (use_vpatch) {
#if !CPP20
				std::string vpatch_path = SearchDecideStoredPathForm(vpatch_fn, std::filesystem::current_path());
#else
				std::u8string vpatch_path = SearchDecideStoredPathForm(vpatch_fn, std::filesystem::current_path());
#endif
				if (std::none_of(state.found.begin(), state.found.end(), [vpatch_path](const game_search_result& it) {
#if !CPP20
					return vpatch_path == it.path;
#else
					return vpatch_path == (const char8_t*)it.path;
#endif
				})) {
					game_search_result result_vpatch = result.copy();
					free(result_vpatch.path);
					free(result_vpatch.description);
					result_vpatch.path = strdup((const char*)vpatch_path.c_str());
					result_vpatch.description = strdup("using vpatch");
					state.found.push_back(result_vpatch);
				}
			}
		}
		LeaveCriticalSection(&state.cs_result);
		ret = 1;
	}
	return ret;
}


static DWORD WINAPI SearchThread(void *param_)
{
	search_thread_param *param = (search_thread_param*)param_;
	const wchar_t *dir = param->dir;
	search_state_t *state = param->state;
	delete param;
	try {
		for (auto &ent : fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied)) {
			if (searchCancelled)
				return 0;
			try {
				if (ent.is_regular_file()
					&& PathMatchSpecExW(ent.path().c_str(), L"*.exe", PMSF_NORMAL) == S_OK
					&& ent.file_size() >= state->size_min
					&& ent.file_size() <= state->size_max)
				{
					SearchCheckExe(*state, ent);
				}
			}
			catch (std::system_error &) {}
		}
	} catch (std::system_error &) {}

	return 0;
}

static HANDLE LaunchSearchThread(search_state_t& state, const wchar_t *dir)
{
	search_thread_param *param = new search_thread_param{&state, dir};
	DWORD thread_id;
	return CreateThread(NULL, 0, SearchThread, param, 0, &thread_id);
}

// Used in std::sort. Return true if a < b.
bool compare_search_results(const game_search_result& a, const game_search_result& b)
{
	int cmp = strcmp(a.id, b.id);
	if (cmp < 0)
		return true;
	if (cmp > 0)
		return false;

	if (a.build && b.build) {
		// We want the higher version first
		cmp = strcmp(a.build, b.build);
		if (cmp > 0)
			return true;
		if (cmp < 0)
			return false;
	}

	if (a.description && b.description) {
		bool a_not_recommended = strstr(a.description, "(not recommended)");
		bool b_not_recommended = strstr(b.description, "(not recommended)");
		if (!a_not_recommended && b_not_recommended)
			return true;
		if (a_not_recommended && !b_not_recommended)
			return false;

		bool a_is_vpatch = strstr(a.description, "using vpatch");
		bool b_is_vpatch = strstr(b.description, "using vpatch");
		if (a_is_vpatch && !b_is_vpatch)
			return true;
		if (!a_is_vpatch && b_is_vpatch)
			return false;

		bool a_is_original = strstr(a.description, "original");
		bool b_is_original = strstr(b.description, "original");
		if (a_is_original && !b_is_original)
			return true;
		if (!a_is_original && b_is_original)
			return false;
	}

	// a == b
	return false;
}

game_search_result* SearchForGames(const wchar_t **dir, const games_js_entry *games_in)
{
	search_state_t state;
	const char *versions_js_fn = "versions" VERSIONS_SUFFIX ".js";

	state.versions = stack_json_resolve(versions_js_fn, NULL);
	if(!state.versions) {
		log_printf(
			"ERROR: No version definition file (%s) found!\n"
			"Seems as if base_tsa didn't download correctly.\n"
			"Try deleting the thpatch directory and running this program again.\n", versions_js_fn
		);
		return NULL;
	}

	// Get file size limits
	json_t *sizes = json_object_get(state.versions, "sizes");
	// Error...
	state.size_min = -1;
	state.size_max = 0;
	const char *key;
	json_object_foreach_key(sizes, key) {
		size_t cur_size = atoi(key);

		if (cur_size < state.size_min)
			state.size_min = cur_size;
		if (cur_size > state.size_max)
			state.size_max = cur_size;
	}

	for (int i = 0; games_in && games_in[i].id; i++) {
		state.previously_known_games.insert(games_in[i].id);
	}

	searchCancelled = false;
	InitializeCriticalSection(&state.cs_result);

	constexpr size_t max_threads = 32;
	HANDLE threads[max_threads];
	DWORD count = 0;

	if(dir && dir[0] && dir[0][0]) {
		while (dir[count] && dir[count][0]) {
			if ((threads[count] = LaunchSearchThread(state, dir[count])) != NULL)
				count++;
		}
	} else {
		wchar_t drive_strings[512];
		wchar_t *p = drive_strings;

		GetLogicalDriveStringsW(512, drive_strings);
		while(p && p[0] && count < max_threads) {
			UINT drive_type = GetDriveTypeW(p);
			if(
				(drive_type != DRIVE_CDROM) &&
				(p[0] != L'A') &&
				(p[0] != L'a')
			) {
				if ((threads[count] = LaunchSearchThread(state, p)) != NULL)
					count++;
			}
			p += wcslen(p) + 1;
		}
	}
	WaitForMultipleObjects(count, threads, TRUE, INFINITE);
	while (count)
		CloseHandle(threads[--count]);

	DeleteCriticalSection(&state.cs_result);
	searchCancelled = false;
	json_decref(state.versions);

	std::sort(state.found.begin(), state.found.end(), compare_search_results);
	game_search_result *ret = (game_search_result*)malloc((state.found.size() + 1) * sizeof(game_search_result));
	size_t i;
	for (i = 0; i < state.found.size(); i++) {
		ret[i] = state.found[i];
	}
	memset(&ret[i], 0, sizeof(game_search_result));
	return ret;
}

void SearchForGames_cancel()
{
	searchCancelled = true;
}

void SearchForGames_free(game_search_result *games)
{
	if (!games)
		return;
	for (size_t i = 0; games[i].id; i++) {
		free(games[i].path);
		free(games[i].id);
		free(games[i].build);
		free(games[i].description);
	}
	free(games);
}

static bool FilterOnSubkeyName(LPCWSTR subkeyName)
{
	return wcsncmp(subkeyName, L"Steam App ", wcslen(L"Steam App ")) == 0;
}

static std::wstring GetValueFromKey(HKEY key, LPCWSTR subkey, LPCWSTR valueName)
{
	LSTATUS ret;
	std::vector<WCHAR> value;
	DWORD valueSize = 64;
	HKEY hSub;
	DWORD type = 0;

	if (RegOpenKeyExW(key, subkey, 0, KEY_READ | KEY_WOW64_64KEY, &hSub)) {
		return L"";
	}

	do {
		value.resize(valueSize);
		valueSize *= sizeof(WCHAR);
		ret = RegQueryValueExW(hSub, valueName, 0, &type, (LPBYTE)value.data(), &valueSize);
		valueSize /= sizeof(WCHAR);
	} while (ret == ERROR_MORE_DATA);
	RegCloseKey(hSub);
	if (ret != ERROR_SUCCESS || type != REG_SZ) {
		return L"";
	}

	if (value[valueSize - 1] == '\0') {
		valueSize--;
	}
	return std::wstring(value.data(), valueSize);
}

static bool FilterOnKey(HKEY key, LPCWSTR subkey)
{
	std::wstring publisher = GetValueFromKey(key, subkey, L"Publisher");

	return publisher == L"上海アリス幻樂団"
		|| publisher == L"黄昏フロンティア";
}

static std::vector<wchar_t*> FindInstalledGamesDir()
{
	log_printf("Loading games from registry...");

	LSTATUS ret;
	HKEY hUninstallKey;

	ret = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
		L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
		0,
		KEY_READ | KEY_WOW64_64KEY,
		&hUninstallKey);
	if (ret != ERROR_SUCCESS)
	{
		log_printf(" failed (RerOpenKeyEx error %d)\n", ret);
		return {};
	}

	DWORD    cSubKeys;                 // number of subkeys
	DWORD    cbMaxSubKeyLen;           // longest subkey size

	RegQueryInfoKeyW(
		hUninstallKey,   // key handle
		nullptr,         // buffer for class name
		nullptr,         // size of class string
		nullptr,         // reserved
		&cSubKeys,       // number of subkeys
		&cbMaxSubKeyLen, // longest subkey size
		nullptr,         // longest class string
		nullptr,         // number of values for this key
		nullptr,         // longest value name
		nullptr,         // longest value data
		nullptr,         // security descriptor
		nullptr);        // last write time

	if (!cSubKeys) {
		log_print(" no installed programs found.\n");
		RegCloseKey(hUninstallKey);
		return {};
	}
	log_print("\n");

	std::vector<wchar_t*> dirlist;
	auto subkeyName = std::make_unique<WCHAR[]>(cbMaxSubKeyLen + 1);

	for (DWORD i = 0; i < cSubKeys; i++) {
		DWORD cchName = cbMaxSubKeyLen + 1;
		ret = RegEnumKeyEx(hUninstallKey, i,
			subkeyName.get(),
			&cchName,
			nullptr,
			nullptr,
			nullptr,
			nullptr);
		if (ret != ERROR_SUCCESS || !FilterOnSubkeyName(subkeyName.get())) {
			continue;
		}

		if (!FilterOnKey(hUninstallKey, subkeyName.get())) {
			continue;
		}
		std::wstring location = GetValueFromKey(hUninstallKey, subkeyName.get(), L"InstallLocation");
		log_printf("Found %S at %S\n", subkeyName.get(), location.c_str());
		dirlist.push_back(wcsdup(location.c_str()));
	}

	RegCloseKey(hUninstallKey);
	return dirlist;
}

game_search_result* SearchForGamesInstalled(const games_js_entry *games_in)
{
	std::vector<wchar_t*> paths = FindInstalledGamesDir();
	if (paths.empty()) {
		return nullptr;
	}

	paths.push_back(nullptr);
	auto ret = SearchForGames(const_cast<const wchar_t**>(paths.data()), games_in);

	for (auto& it : paths) {
		if (it) {
			free(it);
		}
	}
	return ret;
}
