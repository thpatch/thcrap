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
TH_CALLER_FREE char *SearchDecideStoredPathForm(std::filesystem::path target, std::filesystem::path self)
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
				if (ver->build && ver->variety) description += ' ';
				if (ver->variety)               description += ver->variety;
			}

			game_search_result result = std::move(*ver);
			static const auto currentPath = std::filesystem::current_path();
			result.path = SearchDecideStoredPathForm(ent.path(), currentPath);
			result.description = strdup(description.c_str());
			identify_free(ver);

			state.found.push_back(result);

			if (use_vpatch) {
#if !CPP20
				std::string vpatch_path = SearchDecideStoredPathForm(vpatch_fn, currentPath);
#else
				std::u8string vpatch_path = SearchDecideStoredPathForm(vpatch_fn, currentPath);
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

	wchar_t tempPath[MAX_PATH];
	wchar_t windowsPath[MAX_PATH];
	GetTempPathW(MAX_PATH, tempPath);
	GetWindowsDirectoryW(windowsPath, MAX_PATH);
	size_t tempPathLength = wcslen(tempPath);
	size_t windowsPathLength = wcslen(windowsPath);

	try {
		for (auto ent = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied); ent != fs::recursive_directory_iterator(); ++ent) {
			if (searchCancelled)
				return 0;
			try {
				const wchar_t* currentPath = ent->path().c_str();

				//If we are in the Temp or Windows directory, don't recurse into it, and skip it
				if ((wcsncmp(currentPath, tempPath, tempPathLength) == 0) ||
					(wcsncmp(currentPath, windowsPath, windowsPathLength) == 0))
				{
					ent.disable_recursion_pending();
					continue;
				}

				if (!ent->is_regular_file())
					continue;

				std::uintmax_t size = ent->file_size();
				if (size < state->size_min || size > state->size_max)
					continue;

				if (ent->path().extension() == L".exe")
					SearchCheckExe(*state, *ent);
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

#define VERSIONS_JS_FN "versions" VERSIONS_SUFFIX ".js"

game_search_result* SearchForGames(const wchar_t **dir, const games_js_entry *games_in)
{
	search_state_t state;

	state.versions = stack_json_resolve(VERSIONS_JS_FN, NULL);
	if(!state.versions) {
		log_print(
			"ERROR: No version definition file (" VERSIONS_JS_FN ") found!\n"
			"Seems as if base_tsa didn't download correctly.\n"
			"Try deleting the thpatch directory and running this program again.\n"
		);
		return NULL;
	}

	// Get file size limits
	json_t *sizes = json_object_get(state.versions, "sizes");
	// Error...
	state.size_min = SIZE_MAX;
	state.size_max = 0;
	const char *key;
	json_object_foreach_key(sizes, key) {
		size_t cur_size = atoi(key);

		state.size_min = (std::min)(cur_size, state.size_min);
		state.size_max = (std::max)(cur_size, state.size_max);
	}

	if (games_in) {
		for (size_t i = 0; games_in[i].id; ++i) {
			state.previously_known_games.insert(games_in[i].id);
		}
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
	size_t found_count = state.found.size();
	for (size_t i = 0; i < found_count; ++i) {
		ret[i] = state.found[i];
	}
	ret[found_count] = {};
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

static bool isInnoSetupWithUUID(LPCWSTR subkeyName)
{
	if (subkeyName[0] != L'{') {
		return false;
	}
	LPCWSTR end = wcschr(subkeyName, L'}');
	if (!end) {
		return false;
	}
	end++;
	return wcscmp(end, L"_is1") == 0;
}

static bool FilterOnSubkeyName(LPCWSTR subkeyName)
{
	return wcsncmp(subkeyName, L"Steam App ", wcslen(L"Steam App ")) == 0 // Steam game
		|| wcsncmp(subkeyName, L"東方", wcslen(L"東方")) == 0 // Official installers (TSA, since th10)
		|| wcscmp(subkeyName, L"ダブルスポイラー_is1") == 0 // Official installer for th125
		|| wcscmp(subkeyName, L"妖精大戦争_is1") == 0 // Official installer for th128
		|| wcscmp(subkeyName, L"弾幕アマノジャク_is1") == 0 // Official installer for th143
		|| wcscmp(subkeyName, L"バレットフィリア達の闇市場_is1") == 0 // Official installer for th185 (I can't check but it should be that if it follows the same pattern as the others)
		|| isInnoSetupWithUUID(subkeyName) // Might be a Tasofro official installer (at least th105, th123, th135 and th145 follow this format)
		;
}

static std::wstring GetValueFromKey(HKEY hKey, LPCWSTR valueName)
{
	LSTATUS ret;
	std::vector<WCHAR> value;
	DWORD valueSize = 64;
	DWORD type = 0;

	do {
		value.resize(valueSize);
		valueSize *= sizeof(WCHAR);
		ret = RegQueryValueExW(hKey, valueName, 0, &type, (LPBYTE)value.data(), &valueSize);
		valueSize /= sizeof(WCHAR);
	} while (ret == ERROR_MORE_DATA);
	if (ret != ERROR_SUCCESS || type != REG_SZ) {
		return L"";
	}

	if (value[valueSize - 1] == L'\0') {
		valueSize--;
	}
	return std::wstring(value.data(), valueSize);
}

static std::wstring GetValueFromSubkey(HKEY hKey, LPCWSTR subkey, LPCWSTR valueName)
{
	HKEY hSubkey;

	if (RegOpenKeyExW(hKey, subkey, 0, KEY_READ | KEY_WOW64_64KEY, &hSubkey)) {
		return false;
	}
	defer(RegCloseKey(hSubkey));
	return GetValueFromKey(hSubkey, valueName);
}

static bool FilterOnValue(HKEY parentKey, LPCWSTR keyName)
{
	HKEY hKey;

	if (RegOpenKeyExW(parentKey, keyName, 0, KEY_READ | KEY_WOW64_64KEY, &hKey)) {
		return false;
	}
	defer(RegCloseKey(hKey));

	// For Steam and Tasofro official installers
	std::wstring publisher = GetValueFromKey(hKey, L"Publisher");
	if (!publisher.empty()) {
		return publisher == L"上海アリス幻樂団"
			|| publisher == L"黄昏フロンティア";
	}

	// TSA official installers don't fill the Publisher field, but they all fill this field instead
	std::wstring iconGroup = GetValueFromKey(hKey, L"Inno Setup: Icon Group");
	if (!iconGroup.empty()) {
		return iconGroup.compare(0, wcslen(L"上海アリス幻樂団"), L"上海アリス幻樂団") == 0;
	}

	return false;
}

class Reg3264Iterator
{
private:
	const LPCWSTR keyName;

	bool mainRegistryOpened = false;
	bool wow64RegistryOpened = false;

	HKEY hKey = nullptr;
	DWORD cSubKeys;
	DWORD nCurrentSubKeyPos;

	DWORD cbMaxSubKeyLen = 0;
	std::unique_ptr<WCHAR[]> subkeyName;

public:
	Reg3264Iterator(LPCWSTR keyName)
		: keyName(keyName)
	{
		BOOL isWow64;
		IsWow64Process(GetCurrentProcess(), &isWow64);
		if (!isWow64) {
			// Mark it as opened so that we don't try to open it later
			wow64RegistryOpened = true;
		}
	}

	~Reg3264Iterator()
	{
		RegCloseKey(this->hKey);
	}

	bool openNextKey()
	{
		const char *regName = "";
		LSTATUS ret;
		DWORD cbMaxSubKeyLen;

		if (mainRegistryOpened && wow64RegistryOpened) {
			return false;
		}

		if (!mainRegistryOpened) {
			ret = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
				keyName,
				0,
				KEY_READ | KEY_WOW64_64KEY,
				&hKey);
			mainRegistryOpened = true;
			regName = "main";
		}
		else {
			ret = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
				keyName,
				0,
				KEY_READ | KEY_WOW64_32KEY,
				&hKey);
			wow64RegistryOpened = true;
			regName = "wow64";
		}
		if (ret != ERROR_SUCCESS) {
			log_printf("Error opening %s registry key (RegOpenKeyEx error %d)\n", regName, ret);
			return false;
		}

		RegQueryInfoKeyW(
			hKey,
			nullptr, nullptr, nullptr,
			&cSubKeys,       // number of subkeys
			&cbMaxSubKeyLen, // longest subkey size
			nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

		if (cSubKeys == 0) {
			log_printf("%s registry: no installed programs found.\n", regName);
			return false;
		}

		if (cbMaxSubKeyLen > this->cbMaxSubKeyLen) {
			subkeyName = std::make_unique<WCHAR[]>(cbMaxSubKeyLen + 1);
			this->cbMaxSubKeyLen = cbMaxSubKeyLen;
		}
		nCurrentSubKeyPos = 0;

		return true;
	}

	LPCWSTR next()
	{
		if (nCurrentSubKeyPos == cSubKeys) {
			if (!openNextKey()) {
				return nullptr;
			}
		}

		DWORD cchName = cbMaxSubKeyLen + 1;
		LSTATUS ret = RegEnumKeyExW(hKey, nCurrentSubKeyPos,
			subkeyName.get(),
			&cchName,
			nullptr,
			nullptr,
			nullptr,
			nullptr);
		nCurrentSubKeyPos++;
		if (ret != ERROR_SUCCESS) {
			return next();
		}

		return subkeyName.get();
	}

	HKEY getRegHandle()
	{
		return hKey;
	}
};

static void FindInstalledGamesDirFromRegistry(std::vector<wchar_t*>& dirlist /* out */)
{
	log_print("Loading games from registry... ");

	Reg3264Iterator uninstallKeysIt(L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall");
	if (!uninstallKeysIt.openNextKey()) {
		return;
	}
	log_print("\n");

	while (LPCWSTR subkeyName = uninstallKeysIt.next()) {
		if (!FilterOnSubkeyName(subkeyName)) {
			continue;
		}

		if (!FilterOnValue(uninstallKeysIt.getRegHandle(), subkeyName)) {
			continue;
		}
		std::wstring location = GetValueFromSubkey(uninstallKeysIt.getRegHandle(), subkeyName, L"InstallLocation");
		log_print("Found ");
		log_printw(subkeyName);
		log_print(" at ");
		log_printw(location.c_str());
		log_print("\n");
		dirlist.push_back(wcsdup(location.c_str()));
	}
}

static void FindInstalledGamesDirFromDefaultPaths(std::vector<wchar_t*>& dirlist /* out */)
{
	// Older games don't register themselves in the registry, try their default paths
	log_print("Loading games from default paths...\n");

	WCHAR programFilesBuffer[MAX_PATH + 1];
	SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILESX86, nullptr, SHGFP_TYPE_CURRENT, programFilesBuffer);
	std::wstring programFiles = programFilesBuffer;
	const std::wstring defaultPaths[] = {
		L"C:\\Program Files\\東方紅魔郷", // th06
		programFiles + L"\\東方妖々夢", // th07
		programFiles + L"\\東方萃夢想", // th075
		programFiles + L"\\東方永夜抄", // th08
		programFiles + L"\\東方花映塚", // th09
		programFiles + L"\\東方文花帖", // th095
		L""
	};

	for (size_t i = 0; !defaultPaths[i].empty(); i++) {
		if (PathIsDirectoryW(defaultPaths[i].c_str())) {
			log_print("Found ");
			log_printw(defaultPaths[i].c_str());
			log_print("\n");
			dirlist.push_back(wcsdup(defaultPaths[i].c_str()));
		}
	}
}

static std::vector<wchar_t*> FindInstalledGamesDir()
{
	std::vector<wchar_t*> dirlist;
	FindInstalledGamesDirFromRegistry(dirlist);
	FindInstalledGamesDirFromDefaultPaths(dirlist);
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
