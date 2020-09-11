/**
  * Touhou Community Reliant Automatic Patcher
  * Cheap command-line patch stack configuration tool
  */

#include <thcrap.h>
#include <thcrap/src/thcrap_update_wrapper.h>
#include "configure.h"
#include "repo.h"
#include "console.h"

int file_write_error(const char *fn)
{
	static int error_nag = 0;
	log_printf(
		"\n"
		"Error writing to %s!\n"
		"You probably do not have the permission to write to the current directory,\n"
		"or the file itself is write-protected.\n",
		fn
	);
	if (!error_nag) {
		log_printf("Writing is likely to fail for all further files as well.\n");
		error_nag = 1;
	}
	return console_ask_yn("Continue configuration anyway?") == 'y';
}

int file_write_text(const char *fn, const char *str)
{
	int ret;
	FILE *file = fopen_u(fn, "wt");
	if (!file) {
		return -1;
	}
	ret = fputs(str, file);
	fclose(file);
	return ret;
}

std::string run_cfg_fn_build(const patch_sel_stack_t& sel_stack)
{
	bool skip = false;
	std::string ret;

	// If we have any translation patch, skip everything below that
	for (const patch_desc_t& sel : sel_stack) {
		if (strnicmp(sel.patch_id, "lang_", 5) == 0) {
			skip = true;
			break;
		}
	}

	for (const patch_desc_t& sel : sel_stack) {
		std::string patch_id;

		if (strnicmp(sel.patch_id, "lang_", 5) == 0) {
			patch_id = sel.patch_id + 5;
			skip = false;;
		}
		else {
			patch_id = sel.patch_id;
		}

		if (!skip) {
			if (!ret.empty()) {
				ret += "-";
			}
			ret += patch_id;
		}
	}

	return ret;
}

std::string EnterRunCfgFN(std::string& run_cfg_fn)
{
	int ret = 0;
	char run_cfg_fn_new[MAX_PATH];
	std::string run_cfg_fn_js;
	do {
		log_printf(
			"\n"
			"Enter a custom name for this configuration, or leave blank to use the default\n"
			" (%s): ", run_cfg_fn.c_str()
		);
		console_read(run_cfg_fn_new, sizeof(run_cfg_fn_new));
		if (run_cfg_fn_new[0]) {
			run_cfg_fn = run_cfg_fn_new;
		}
		if (PathFileExists(run_cfg_fn_js.c_str())) {
			log_printf("\"%s\" already exists. ", run_cfg_fn_js.c_str());
			ret = console_ask_yn("Overwrite?") == 'n';
		}
		else {
			ret = 0;
		}
	} while (ret);
	return run_cfg_fn;
}

struct progress_state_t
{
    // This callback can be called from a bunch of threads
    std::mutex mutex;
    std::map<std::string, std::chrono::steady_clock::time_point> files;
};

bool progress_callback(progress_callback_status_t *status, void *param)
{
    using namespace std::literals;
    progress_state_t *state = (progress_state_t*)param;
    std::scoped_lock lock(state->mutex);

    switch (status->status) {
        case GET_DOWNLOADING: {
            // Using the URL instead of the filename is important, because we may
            // be downloading the same file from 2 different URLs, and the UI
            // could quickly become very confusing, with progress going backwards etc.
            auto& file_time = state->files[status->url];
            auto now = std::chrono::steady_clock::now();
            if (file_time.time_since_epoch() == 0ms) {
                file_time = now;
            }
            else if (now - file_time > 5s) {
                log_printf("[%u/%u] %s: in progress (%ub/%ub)...\n", status->nb_files_downloaded, status->nb_files_total,
                           status->url, status->file_progress, status->file_size);
                file_time = now;
            }
            return true;
        }

        case GET_OK:
            log_printf("[%u/%u] %s/%s: OK (%ub)\n", status->nb_files_downloaded, status->nb_files_total, status->patch->id, status->fn, status->file_size);
            return true;

        case GET_CLIENT_ERROR:
            log_printf("%s: file not available\n", status->url);
            return true;
        case GET_CRC32_ERROR:
            log_printf("%s: CRC32 error\n", status->url);
            return true;
        case GET_SERVER_ERROR:
            log_printf("%s: server error\n", status->url);
            return true;
        case GET_CANCELLED:
            // Another copy of the file have been downloader earlier. Ignore.
            return true;
        case GET_SYSTEM_ERROR:
            log_printf("%s: system error\n", status->url);
            return true;
        default:
            log_printf("%s: unknown status\n", status->url);
            return true;
    }
}


char **games_json_to_array(json_t *games)
{
    char **array;
    const char *key;
    json_t *value;

    array = strings_array_create();
    json_object_foreach(games, key, value) {
        array = strings_array_add(array, key);
    }
    return array;
}

#include <win32_utf8/entry_winmain.c>
#include "thcrap_i18n/src/thcrap_i18n.h"
int __cdecl win32_utf8_main(int argc, const char *argv[])
{
	VLA(char, current_dir, MAX_PATH);
	GetModuleFileNameU(NULL, current_dir, MAX_PATH);
	PathRemoveFileSpecU(current_dir);
	PathAppendA(current_dir, "..");
	SetCurrentDirectoryU(current_dir);
	VLA_FREE(current_dir);
	int ret = 0;
	i18n_lang_init(THCRAP_I18N_APPDOMAIN);
	// Global URL cache to not download anything twice
	json_t *url_cache = json_object();
	// Repository ID cache to prioritize the most local repository if more
	// than one repository with the same name is discovered in the network
	json_t *id_cache = json_object();

	repo_t **repo_list = nullptr;

	const char *start_repo = "https://srv.thpatch.net/";

	patch_sel_stack_t sel_stack;
	json_t *new_cfg = json_pack("{s[]}", "patches");

	size_t cur_dir_len = GetCurrentDirectory(0, NULL) + 1;
	VLA(char, cur_dir, cur_dir_len);
	json_t *games = NULL;

	std::string run_cfg_fn;
	std::string run_cfg_fn_js;
	char *run_cfg_str = NULL;

    progress_state_t state;

	strings_mod_init();
	log_init(0);
	console_init();

	GetCurrentDirectory(cur_dir_len, cur_dir);
	PathAddBackslashA(cur_dir);
	str_slash_normalize(cur_dir);

	if (argc > 1) {
		start_repo = argv[1];
	}

	console_prepare_prompt();
	log_printf(_A(
		"==========================================\n"
		"Touhou Community Reliant Automatic Patcher - Patch configuration tool\n"
		"==========================================\n"
		"\n"
		"\n"
		"This tool will create a new patch configuration for the\n"
		"Touhou Community Reliant Automatic Patcher.\n"
		"\n"
		"\n"
	));
	if (thcrap_update_module()) {
		log_printf(_A(
			"The configuration process has four steps:\n"
			"\n"
			"\t\t1. Selecting patches\n"
			"\t\t2. Downloading game-independent data\n"
			"\t\t3. Locating game installations\n"
			"\t\t4. Downloading game-specific data\n"
			"\n"
			"\n"
			"\n"
			"Patch repository discovery will start at\n"
			"\n"
			"\t%s\n"
			"\n"
			"You can specify a different URL as a command-line parameter.\n"
			"Additionally, all patches from previously discovered repositories, stored in\n"
			"subdirectories of the current directory, will be available for selection.\n"),
			start_repo
		);
	}
	else {
		log_printf(_A(
			"The configuration process has two steps:\n"
			"\n"
			"\t\t1. Selecting patches\n"
			"\t\t2. Locating game installations\n"
			"\n"
			"\n"
			"\n"
			"Updates are disabled. Therefore, patch selection is limited to the\n"
			"repositories that are locally available.\n")
		);
	}
	log_print(
		"\n"
		"\n"
	);
	pause();

	CreateDirectoryU("repos", NULL);
    repo_list = RepoDiscover_wrapper(start_repo);
	if (!repo_list || !repo_list[0]) {
		log_printf(_A("No patch repositories available...\n"));
		pause();
		goto end;
	}
	sel_stack = SelectPatchStack(repo_list);
	if (!sel_stack.empty()) {
		log_printf(_A("Downloading game-independent data...\n"));
		stack_update_wrapper(update_filter_global_wrapper, NULL, progress_callback, &state);
        state.files.clear();

		/// Build the new run configuration
		json_t *new_cfg_patches = json_object_get(new_cfg, "patches");
		for (patch_desc_t& sel : sel_stack) {
			patch_t patch = patch_build(&sel);
			json_array_append_new(new_cfg_patches, patch_to_runconfig_json(&patch));
            patch_free(&patch);
		}
	}


	// Other default settings
	json_object_set_new(new_cfg, "console", json_false());
	json_object_set_new(new_cfg, "dat_dump", json_false());

	run_cfg_fn = run_cfg_fn_build(sel_stack);
	run_cfg_fn = EnterRunCfgFN(run_cfg_fn);
	run_cfg_fn_js = std::string("config/") + run_cfg_fn + ".js";


	CreateDirectoryU("config", NULL);
	run_cfg_str = json_dumps(new_cfg, JSON_INDENT(2) | JSON_SORT_KEYS);
	if (!file_write_text(run_cfg_fn_js.c_str(), run_cfg_str)) {
		log_printf(_A("\n\nThe following run configuration has been written to %s:\n"), run_cfg_fn_js.c_str());
		log_printf(run_cfg_str);
		log_printf("\n\n");
		pause();
	}
	else if (!file_write_error(run_cfg_fn_js.c_str())) {
		goto end;
	}

	// Step 2: Locate games
	games = ConfigureLocateGames(cur_dir);

	if (json_object_size(games) > 0 && (console_ask_yn(_A("Create shortcuts? (required for first run)")) == 'n' || !CreateShortcuts(run_cfg_fn.c_str(), games))) {
        char **filter = games_json_to_array(games);
		log_printf(_A("\nDownloading data specific to the located games...\n"));
		stack_update_wrapper(update_filter_games_wrapper, filter, progress_callback, &state);
        state.files.clear();
        strings_array_free(filter);
		log_printf(_A(
			"\n"
			"\n"
			"Done.\n"
			"\n"
			"You can now start the respective games with your selected configuration\n"
			"through the shortcuts created in the current directory\n"
			"(%s).\n"
			"\n"
			"These shortcuts work from anywhere, so feel free to move them wherever you like.\n"
			"Alternatively, if you find yourself drowned in shortcuts, you should try this tool:\n"
			"https://github.com/thpatch/Universal-THCRAP-Launcher/releases\n"
			"\n"), cur_dir
		);
		con_can_close = true;
		pause();
	}
end:
	con_can_close = true;
	SAFE_FREE(run_cfg_str);
	json_decref(new_cfg);
	json_decref(games);

	VLA_FREE(cur_dir);
	stack_free();
    for (auto& sel : sel_stack) {
        free(sel.repo_id);
        free(sel.patch_id);
    }
	for (size_t i = 0; repo_list[i]; i++) {
		RepoFree(repo_list[i]);
	}
	free(repo_list);
	json_decref(url_cache);
	json_decref(id_cache);

	globalconfig_release();
	runconfig_free();
	
	thcrap_update_exit_wrapper();
	con_end();
	return 0;
}
