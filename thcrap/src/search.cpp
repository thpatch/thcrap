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

typedef struct alignas(16) {
	DWORD size_min;
	DWORD size_max;
	json_t *versions;
	json_t *found;
	json_t *result;
	DWORD max_threads;
	volatile DWORD cur_threads;
	CRITICAL_SECTION cs_result;
} search_state_t;

static search_state_t state;

int SearchCheckExe(wchar_t *local_dir, WIN32_FIND_DATAW *w32fd)
{
	int ret = 0;
	json_t *ver = identify_by_size(w32fd->nFileSizeLow, state.versions);
	if(ver) {
		WCSLEN_DEC(local_dir);
		size_t exe_fn_len = local_dir_len + wcslen(w32fd->cFileName) + 2;
		VLA(wchar_t, exe_fn, exe_fn_len);
		const char *key;
		json_t *game_val;

		wcsncpy(exe_fn, local_dir, local_dir_len);
		exe_fn[local_dir_len - 1] = 0;
		wcscat(exe_fn, w32fd->cFileName);

		size_t exe_fn_a_len = wcslen(exe_fn)*UTF8_MUL + 1;
		VLA(char, exe_fn_a, exe_fn_a_len);
		StringToUTF8(exe_fn_a, exe_fn, exe_fn_a_len);
		str_slash_normalize(exe_fn_a);

		ver = identify_by_hash(exe_fn_a, (size_t*)&w32fd->nFileSizeLow, state.versions);
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
		auto vpatch_fn = std::filesystem::path(local_dir) / L"vpatch.exe";
		bool use_vpatch = false;
		if (strstr(key, "_custom") == nullptr && std::filesystem::is_regular_file(vpatch_fn)) {
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
		VLA_FREE(exe_fn);
		ret = 1;
	}
	return ret;
}


DWORD WINAPI SearchThread(void *param)
{
	HANDLE hFind;
	WIN32_FIND_DATAW w32fd;
	wchar_t *param_dir = (wchar_t*)param;

	// Add the asterisk
	size_t dir_len = wcslen(param_dir) + 2 + 1;
	VLA(wchar_t, local_dir, dir_len);

	BOOL ret = 1;

	wcscpy(local_dir, param_dir);
	wcscat(local_dir, L"*");

	InterlockedIncrement(&state.cur_threads);

	SAFE_FREE(param_dir);
	hFind = FindFirstFileW(local_dir, &w32fd);
	while( (hFind != INVALID_HANDLE_VALUE) && ret ) {
		if(
			!wcscmp(w32fd.cFileName, L".") ||
			!wcscmp(w32fd.cFileName, L"..")
		) {
			ret = FindNextFileW(hFind, &w32fd);
			continue;
		}
		local_dir[dir_len - 3] = 0;

		if(w32fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			DWORD thread_id;
			size_t cur_file_len = dir_len + wcslen(w32fd.cFileName) + 2;
			wchar_t *cur_file = (wchar_t*)malloc(cur_file_len * sizeof(wchar_t));

			wcsncpy(cur_file, local_dir, dir_len);
			wcscat(cur_file, w32fd.cFileName);
			PathAddBackslashW(cur_file);

			// Execute the function normally if we're above the thread limit
			if(InterlockedCompareExchange(&state.cur_threads, -1, -1) > state.max_threads) {
				SearchThread(cur_file);
			} else {
				HANDLE hThread = CreateThread(NULL, 0, SearchThread, cur_file, 0, &thread_id);
				if (hThread) CloseHandle(hThread);
			}
		} else if(
			(PathMatchSpecW(w32fd.cFileName, L"*.exe")) &&
			(w32fd.nFileSizeLow >= state.size_min) &&
			(w32fd.nFileSizeLow <= state.size_max)
		) {
			SearchCheckExe(local_dir, &w32fd);
		}
		ret = FindNextFileW(hFind, &w32fd);
	}
	FindClose(hFind);
	InterlockedDecrement(&state.cur_threads);
	VLA_FREE(local_dir);
	return 0;
}

void LaunchSearchThread(const char *dir)
{
	DWORD thread_id;
	HANDLE initial_thread;

	// Freed inside the thread
	size_t cur_dir_len = strlen(dir) + 1;
	wchar_t *cur_dir = (wchar_t*)malloc(cur_dir_len * sizeof(wchar_t));
	StringToUTF16(cur_dir, dir, cur_dir_len);
	initial_thread = CreateThread(NULL, 0, SearchThread, cur_dir, 0, &thread_id);
	WaitForSingleObject(initial_thread, INFINITE);
}

json_t* SearchForGames(const char *dir, json_t *games_in)
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

	state.size_min = 0xFFFFFFFF;
	state.size_max = 0;
	state.cur_threads = 0;
	state.max_threads = 0;
	state.found = json_object();
	state.result = games_in ? games_in : json_object();
	InitializeCriticalSection(&state.cs_result);

	// Get file size limits
	json_object_foreach(sizes, key, val) {
		size_t cur_size = atoi(key);

		state.size_min = MIN(cur_size, state.size_min);
		state.size_max = MAX(cur_size, state.size_max);
	}

	if(dir && dir[0]) {
		LaunchSearchThread(dir);
	} else {
		char drive_strings[512];
		char *p = drive_strings;

		GetLogicalDriveStringsA(512, drive_strings);
		while(p && p[0]) {
			UINT drive_type = GetDriveTypeA(p);
			if(
				(drive_type != DRIVE_CDROM) &&
				(p[0] != 'A') &&
				(p[0] != 'a')
			) {
				LaunchSearchThread(p);
			}
			p += strlen(p) + 1;
		}
	}
	while(state.cur_threads) {
		Sleep(100);
	}

	DeleteCriticalSection(&state.cs_result);
	json_decref(state.versions);
	return state.found;
}
