/**
  * Touhou Community Reliant Automatic Patcher
  * Cheap command-line patch stack configuration tool
  */

#include <thcrap.h>
#include <thcrap/src/thcrap_update_wrapper.h>
#include "configure.h"
#include "repo.h"
#include "console.h"

typedef enum {
	RUN_CFG_FN,
	RUN_CFG_FN_JS
} configure_slot_t;

int wine_flag = 0;

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

const char* run_cfg_fn_build(const size_t slot, const json_t *sel_stack)
{
	const char *ret = NULL;
	size_t i;
	json_t *sel;
	int skip = 0;

	ret = strings_strclr(slot);

	// If we have any translation patch, skip everything below that
	json_array_foreach(sel_stack, i, sel) {
		const char *patch_id = json_array_get_string(sel, 1);
		if (!strnicmp(patch_id, "lang_", 5)) {
			skip = 1;
			break;
		}
	}

	json_array_foreach(sel_stack, i, sel) {
		const char *patch_id = json_array_get_string(sel, 1);
		if (!patch_id) {
			continue;
		}

		if (ret && ret[0]) {
			ret = strings_strcat(slot, "-");
		}

		if (!strnicmp(patch_id, "lang_", 5)) {
			patch_id += 5;
			skip = 0;
		}
		if (!skip) {
			ret = strings_strcat(slot, patch_id);
		}
	}
	return ret;
}

const char* EnterRunCfgFN(configure_slot_t slot_fn, configure_slot_t slot_js)
{
	int ret = 0;
	const char* run_cfg_fn = strings_storage_get(slot_fn, 0);
	char run_cfg_fn_new[MAX_PATH];
	const char *run_cfg_fn_js;
	do {
		log_printf(
			"\n"
			"Enter a custom name for this configuration, or leave blank to use the default\n"
			" (%s): ", run_cfg_fn
		);
		console_read(run_cfg_fn_new, sizeof(run_cfg_fn_new));
		if (run_cfg_fn_new[0]) {
			run_cfg_fn = strings_sprintf(slot_fn, "%s", run_cfg_fn_new);
		}
		run_cfg_fn_js = strings_sprintf(slot_js, "%s.js", run_cfg_fn);
		if (PathFileExists(run_cfg_fn_js)) {
			log_printf("\"%s\" already exists. ", run_cfg_fn_js);
			ret = console_ask_yn("Overwrite?") == 'n';
		}
		else {
			ret = 0;
		}
	} while (ret);
	return run_cfg_fn;
}

int progress_callback(DWORD stack_progress, DWORD stack_total,
	const json_t *patch, DWORD patch_progress, DWORD patch_total,
	const char *fn, get_result_t ret, DWORD file_progress, DWORD file_total,
	void *param)
{
	(void)stack_progress; (void)stack_total;
	(void)patch; (void)patch_progress; (void)patch_total;
	(void)fn; (void)ret; (void)param;
	if (file_total)
		console_print_percent((int)file_progress * 100 / file_total);
	return TRUE;
}

#include <win32_utf8/entry_winmain.c>

int __cdecl win32_utf8_main(int argc, const char *argv[])
{
	int ret = 0;

	// Global URL cache to not download anything twice
	json_t *url_cache = json_object();
	// Repository ID cache to prioritize the most local repository if more
	// than one repository with the same name is discovered in the network
	json_t *id_cache = json_object();

	json_t *repo_list = NULL;

	const char *start_repo = "http://thcrap.nmlgc.net/repos/nmlgc/";

	json_t *sel_stack = NULL;
	json_t *new_cfg = json_pack("{s[]}", "patches");

	size_t cur_dir_len = GetCurrentDirectory(0, NULL) + 1;
	VLA(char, cur_dir, cur_dir_len);
	json_t *games = NULL;

	const char *run_cfg_fn = NULL;
	const char *run_cfg_fn_js = NULL;
	char *run_cfg_str = NULL;

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
	log_printf(
		"==========================================\n"
		"Touhou Community Reliant Automatic Patcher - Patch configuration tool\n"
		"==========================================\n"
		"\n"
		"\n"
		"This tool will create a new patch configuration for the\n"
		"Touhou Community Reliant Automatic Patcher.\n"
		"\n"
		"\n"
	);
	if (thcrap_update_module()) {
		log_printf(
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
			"subdirectories of the current directory, will be available for selection.\n",
			start_repo
		);
	}
	else {
		log_printf(
			"The configuration process has two steps:\n"
			"\n"
			"\t\t1. Selecting patches\n"
			"\t\t2. Locating game installations\n"
			"\n"
			"\n"
			"\n"
			"Updates are disabled. Therefore, patch selection is limited to the\n"
			"repositories that are locally available.\n"
		);
	}
	log_print(
		"\n"
		"\n"
	);
	pause();

	if (RepoDiscoverAtURL_wrapper(start_repo, id_cache, url_cache)) {
		goto end;
	}
	if (RepoDiscoverFromLocal_wrapper(id_cache, url_cache)) {
		goto end;
	}
	repo_list = RepoLoad();
	if (!json_object_size(repo_list)) {
		log_printf("No patch repositories available...\n");
		pause();
		goto end;
	}
	sel_stack = SelectPatchStack(repo_list);
	if (json_array_size(sel_stack)) {
		json_t *new_cfg_patches = json_object_get(new_cfg, "patches");
		size_t i;
		json_t *sel;

		log_printf("Downloading game-independent data...\n");
		stack_update_wrapper(update_filter_global_wrapper, NULL, progress_callback, NULL);

		/// Build the new run configuration
		json_array_foreach(sel_stack, i, sel) {
			json_array_append_new(new_cfg_patches, patch_build(sel));
		}
	}

	// Other default settings
	json_object_set_new(new_cfg, "console", json_false());
	json_object_set_new(new_cfg, "dat_dump", json_false());

	run_cfg_fn = run_cfg_fn_build(RUN_CFG_FN, sel_stack);
	run_cfg_fn = EnterRunCfgFN(RUN_CFG_FN, RUN_CFG_FN_JS);
	run_cfg_fn_js = strings_storage_get(RUN_CFG_FN_JS, 0);

	run_cfg_str = json_dumps(new_cfg, JSON_INDENT(2) | JSON_SORT_KEYS);
	if (!file_write_text(run_cfg_fn_js, run_cfg_str)) {
		log_printf("\n\nThe following run configuration has been written to %s:\n", run_cfg_fn_js);
		log_printf(run_cfg_str);
		log_printf("\n\n");
		pause();
	}
	else if (!file_write_error(run_cfg_fn_js)) {
		goto end;
	}

	// Step 2: Locate games
	games = ConfigureLocateGames(cur_dir);

	if (json_object_size(games) > 0 && (console_ask_yn("Create shortcuts?") == 'n' || !CreateShortcuts(run_cfg_fn, games))) {
		json_t *filter = json_object_get_keys_sorted(games);
		log_printf("\nDownloading data specific to the located games...\n");
		stack_update_wrapper(update_filter_games_wrapper, filter, progress_callback, NULL);
		filter = json_decref_safe(filter);
		log_printf(
			"\n"
			"\n"
			"Done.\n"
			"\n"
			"You can now start the respective games with your selected configuration\n"
			"through the shortcuts created in the current directory\n"
			"(%s).\n"
			"\n"
			"These shortcuts work from anywhere, so feel free to move them wherever you like.\n"
			"\n", cur_dir
		);
		pause();
	}
end:
	SAFE_FREE(run_cfg_str);
	json_decref(new_cfg);
	json_decref(sel_stack);
	json_decref(games);

	VLA_FREE(cur_dir);
	json_decref(repo_list);
	json_decref(url_cache);
	json_decref(id_cache);

	thcrap_update_exit_wrapper();
	con_end();
	return 0;
}
