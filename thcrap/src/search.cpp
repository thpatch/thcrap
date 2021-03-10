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
namespace fs = std::filesystem;

typedef struct alignas(16) {
	size_t size_min;
	size_t size_max;
	json_t *versions;
	json_t *found;
	json_t *result;
	CRITICAL_SECTION cs_result;
} search_state_t;

static search_state_t state;

static int SearchCheckExe(const fs::directory_entry &ent)
{
	int ret = 0;
	json_t *ver = identify_by_size((size_t)ent.file_size(), state.versions);
	if(ver) {
		const char *key;
		json_t *game_val;

		const std::wstring &exe_fn = ent.path().native();
		size_t exe_fn_a_len = exe_fn.length()*UTF8_MUL + 1;
		VLA(char, exe_fn_a, exe_fn_a_len);
		StringToUTF8(exe_fn_a, exe_fn.c_str(), exe_fn_a_len);
		str_slash_normalize(exe_fn_a);

		size_t file_size = (size_t)ent.file_size();
		ver = identify_by_hash(exe_fn_a, &file_size, state.versions);
		if(!ver) {
			return ret;
		}

		key = json_array_get_string(ver, 0);

		// Check if user already selected a version of this game in a previous search
		if(json_object_get(state.result, key)) {
			return ret;
		}

		// Alright, found a game!
		// Check if it has a vpatch
		auto vpatch_fn = ent.path().parent_path() / L"vpatch.exe";
		bool use_vpatch = false;
		if (strstr(key, "_custom") == nullptr && fs::is_regular_file(vpatch_fn)) {
			use_vpatch = true;
		}

		// Add the game into the result object
		game_val = json_object_get_create(state.found, key, JSON_OBJECT);

		EnterCriticalSection(&state.cs_result);
		{
			const char *build = json_array_get_string(ver, 1);
			const char *variety = json_array_get_string(ver, 2);

			json_t *id_str = json_pack("s++",
				build ? build : "",
				build && variety ? " " : "",
				variety ? variety : ""
			);

			json_object_set_new(game_val, exe_fn_a, id_str);

			if (use_vpatch) {
				json_object_set_new(
					game_val,
					vpatch_fn.generic_u8string().c_str(),
					json_string("using vpatch")
				);
			}
		}
		LeaveCriticalSection(&state.cs_result);
		VLA_FREE(exe_fn_a);
		ret = 1;
	}
	return ret;
}


static DWORD WINAPI SearchThread(void *param)
{
	const wchar_t *dir = (const wchar_t *)param;
	try {
		for (auto &ent : fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied)) {
			try {
				if (ent.is_regular_file()
					&& PathMatchSpecW(ent.path().c_str(), L"*.exe")
					&& ent.file_size() >= state.size_min
					&& ent.file_size() <= state.size_max)
				{
					SearchCheckExe(ent);
				}
			}
			catch (fs::filesystem_error &) {}
		}
	} catch (fs::filesystem_error &) {}

	return 0;
}

static HANDLE LaunchSearchThread(const wchar_t *dir)
{
	DWORD thread_id;
	return CreateThread(NULL, 0, SearchThread, (void*)dir, 0, &thread_id);
}

json_t* SearchForGames(const wchar_t *dir, json_t *games_in)
{
	const char *versions_js_fn = "versions.js";
	json_t *sizes;

	const char *key;
	json_t *val;

	state.versions = stack_json_resolve(versions_js_fn, NULL);
	if(!state.versions) {
		log_printf(
			"ERROR: No version definition file (%s) found!\n"
			"Seems as if base_tsa didn't download correctly.\n"
			"Try deleting the thpatch directory and running this program again.\n", versions_js_fn
		);
		return NULL;
	}
	sizes = json_object_get(state.versions, "sizes");
	// Error...

	state.size_min = -1;
	state.size_max = 0;
	state.found = json_object();
	state.result = games_in ? games_in : json_object();
	InitializeCriticalSection(&state.cs_result);

	// Get file size limits
	json_object_foreach(sizes, key, val) {
		size_t cur_size = atoi(key);

		if (cur_size < state.size_min)
			state.size_min = cur_size;
		if (cur_size > state.size_max)
			state.size_max = cur_size;
	}

	constexpr size_t max_threads = 32;
	HANDLE threads[max_threads];
	DWORD count = 0;

	if(dir && dir[0]) {
		if ((threads[count] = LaunchSearchThread(dir)) != NULL)
			count++;
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
				if ((threads[count] = LaunchSearchThread(p)) != NULL)
					count++;
			}
			p += wcslen(p) + 1;
		}
	}
	WaitForMultipleObjects(count, threads, TRUE, INFINITE);
	while (count)
		CloseHandle(threads[--count]);

	DeleteCriticalSection(&state.cs_result);
	json_decref(state.versions);
	return state.found;
}
