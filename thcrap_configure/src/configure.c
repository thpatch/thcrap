/**
  * Touhou Community Reliant Automatic Patcher
  * Cheap command-line patch stack configuration tool
  */

#include <thcrap.h>
#include <thcrap_update/src/update.h>
#include "configure.h"
#include "repo.h"

int Ask(const char *question)
{
	int ret = 0;
	while(ret != 'y' && ret != 'n') {
		char buf[2];
		if(question) {
			log_printf(question);
		}
		log_printf(" (y/n) ");

		fgets(buf, sizeof(buf), stdin);
		// Flush stdin (because fflush(stdin) is "undefined behavior")
		while((ret = getchar()) != '\n' && ret != EOF);

		ret = tolower(buf[0]);
	}
	return ret == 'y';
}

// http://support.microsoft.com/kb/99261
void cls(SHORT top)
{
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

	// here's where we'll home the cursor
	COORD coordScreen = {0, top};

	DWORD cCharsWritten;
	// to get buffer info
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	// number of character cells in the current buffer
	DWORD dwConSize;

	// get the number of character cells in the current buffer
	GetConsoleScreenBufferInfo(hConsole, &csbi);
	dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

	// fill the entire screen with blanks
	FillConsoleOutputCharacter(
		hConsole, TEXT(' '), dwConSize, coordScreen, &cCharsWritten
	);
	// get the current text attribute
	GetConsoleScreenBufferInfo(hConsole, &csbi);

	// now set the buffer's attributes accordingly
	FillConsoleOutputAttribute(
		hConsole, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten
	);

	// put the cursor at (0, 0)
	SetConsoleCursorPosition(hConsole, coordScreen);
	return;
}

// Because I've now learned how bad system("pause") can be
void pause(void)
{
	int ret;
	printf("Press ENTER to continue . . . ");
	while((ret = getchar()) != '\n' && ret != EOF);
}

json_t* patch_build(const json_t *sel)
{
	const char *repo_id = json_array_get_string(sel, 0);
	const char *patch_id = json_array_get_string(sel, 1);
	return json_pack("{ss+++}",
		"archive", repo_id, "/", patch_id, "/"
	);
}

json_t* patch_bootstrap(const json_t *sel, json_t *repo_servers)
{
	const char *main_fn = "patch.js";
	char *patch_js_buffer;
	DWORD patch_js_size;
	json_t *patch_info = patch_build(sel);
	const json_t *patch_id = json_array_get(sel, 1);
	size_t patch_len = json_string_length(patch_id) + 1;

	size_t remote_patch_fn_len = patch_len + 1 + strlen(main_fn) + 1;
	VLA(char, remote_patch_fn, remote_patch_fn_len);
	sprintf(remote_patch_fn, "%s/%s", json_string_value(patch_id), main_fn);

	patch_js_buffer = (char*)ServerDownloadFile(repo_servers, remote_patch_fn, &patch_js_size, NULL);
	patch_file_store(patch_info, main_fn, patch_js_buffer, patch_js_size);
	// TODO: Nice, friendly error

	VLA_FREE(remote_patch_fn);
	SAFE_FREE(patch_js_buffer);
	return patch_info;
}

char* run_cfg_fn_build(const json_t *sel_stack)
{
	size_t i;
	json_t *sel;
	size_t run_cfg_fn_len = strlen(".js") + 1;
	char *run_cfg_fn = NULL;

	// Here at Touhou Patch Center, we love double loops.
	json_array_foreach(sel_stack, i, sel) {
		const json_t *patch_id = json_array_get(sel, 1);
		run_cfg_fn_len += json_string_length(patch_id) + 1;
	}

	run_cfg_fn = (char*)malloc(run_cfg_fn_len);
	run_cfg_fn[0] = 0;

	json_array_foreach(sel_stack, i, sel) {
		const char *patch_id = json_array_get_string(sel, 1);
		if(!patch_id) {
			continue;
		}

		if(run_cfg_fn[0]) {
			strcat(run_cfg_fn, "-");
		}

		if(!stricmp(patch_id, "base_tsa") && json_array_size(sel_stack) > 1) {
			continue;
		}
		if(!strnicmp(patch_id, "lang_", 5)) {
			patch_id += 5;
		}
		strcat(run_cfg_fn, patch_id);
	}
	strcat(run_cfg_fn, ".js");
	return run_cfg_fn;
}

void CreateShortcuts(const char *run_cfg_fn, json_t *games)
{
#ifdef _DEBUG
	const char *loader_exe = "thcrap_loader_d.exe";
#else
	const char *loader_exe = "thcrap_loader.exe";
#endif
	STRLEN_DEC(run_cfg_fn);
	size_t self_fn_len = GetModuleFileNameU(NULL, NULL, 0) + 1;
	VLA(char, self_fn, self_fn_len);

	GetModuleFileNameU(NULL, self_fn, self_fn_len);
	PathRemoveFileSpec(self_fn);
	PathAddBackslashA(self_fn);

	// Yay, COM.
	CoInitializeEx(NULL, COINIT_MULTITHREADED);
	{
		const char *key = NULL;
		json_t *cur_game = NULL;
		VLA(char, self_path, self_fn_len);
		strcpy(self_path, self_fn);

		strcat(self_fn, loader_exe);

		log_printf("Creating shortcuts");

		json_object_foreach(games, key, cur_game) {
			const char *game_fn = json_string_value(cur_game);
			size_t game_len = strlen(key) + 1;
			size_t link_fn_len = game_len + 1 + run_cfg_fn_len + strlen(".lnk") + 1;
			size_t link_args_len = 1 + run_cfg_fn_len + 2 + game_len + 1;
			VLA(char, link_fn, link_fn_len);
			VLA(char, link_args, link_args_len);

			log_printf(".");

			sprintf(link_fn, "%s-%s", key, run_cfg_fn);
			PathRenameExtensionA(link_fn, ".lnk");

			sprintf(link_args, "\"%s\" %s", run_cfg_fn, key);
			CreateLink(link_fn, self_fn, link_args, self_path, game_fn);
			VLA_FREE(link_fn);
			VLA_FREE(link_args);
		}
		VLA_FREE(self_path);
	}
	VLA_FREE(self_fn);
	CoUninitialize();
}

int __cdecl wmain(int argc, wchar_t *wargv[])
{
	int ret = 0;

	json_t *repo_list = NULL;

	const char *start_repo = "http://srv.thpatch.net";

	json_t *sel_stack = NULL;
	json_t *new_cfg = json_pack("{s[]}", "patches");

	size_t cur_dir_len = GetCurrentDirectory(0, NULL) + 1;
	VLA(char, cur_dir, cur_dir_len);
	json_t *games = NULL;

	char *run_cfg_fn = NULL;

	json_t *args = json_array_from_wchar_array(argc, wargv);

	log_init(1);

	// Necessary to correctly process *any* input of non-ASCII characters
	// in the console subsystem
	w32u8_set_fallback_codepage(GetOEMCP());

	GetCurrentDirectory(cur_dir_len, cur_dir);
	PathAddBackslashA(cur_dir);
	str_slash_normalize(cur_dir);

	{
		// Maximize the height of the console window
		CONSOLE_SCREEN_BUFFER_INFO sbi = {0};
		HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
		COORD largest = GetLargestConsoleWindowSize(console);
		HWND console_wnd = GetConsoleWindow();
		RECT console_rect;

		GetWindowRect(console_wnd, &console_rect);
		SetWindowPos(console_wnd, NULL, console_rect.left, 0, 0, 0, SWP_NOSIZE);
		GetConsoleScreenBufferInfo(console, &sbi);
		sbi.srWindow.Bottom = largest.Y - 4;
		SetConsoleWindowInfo(console, TRUE, &sbi.srWindow);
	}

	inet_init();

	if(json_array_size(args) > 1) {
		start_repo = json_array_get_string(args, 1);
	}

	log_printf(
		"==========================================\n"
		"Touhou Community Reliant Automatic Patcher - Patch configuration tool\n"
		"==========================================\n"
		"\n"
		"\n"
		"This tool will create a new patch configuration for the\n"
		"Touhou Community Reliant Automatic Patcher.\n"
		"\n"
		"Unfortunately, we didn't manage to create a nice, graphical user interface\n"
		"for patch configuration in time for the Double Dealing Character release.\n"
		"We hope that this will still be user-friendly enough for initial configuration.\n"
		"\n"
		"\n"
		"This process has two steps:\n"
		"\n"
		"\t\t1. Selecting patches\n"
		"\t\t2. Locating game installations\n"
		"\n"
		"\n"
		"\n"
		"Patch repository discovery will start at\n"
		"\n"
		"\t%s\n"
		"\n"
		"You can specify a different URL as a command-line parameter.\n"
		"Additionally, all patches from previously discovered repositories, stored in\n"
		"subdirectories of the current directory, will be available for selection.\n"
		"\n"
		"\n",
		start_repo
	);
	pause();

	RepoDiscover(start_repo, NULL, NULL);
	repo_list = RepoLoadLocal();
	if(!json_object_size(repo_list)) {
		log_printf("No patch repositories available...\n");
		goto end;
	}
	sel_stack = SelectPatchStack(repo_list);
	if(json_array_size(sel_stack)) {
		json_t *new_cfg_patches = json_object_get(new_cfg, "patches");
		size_t i;
		json_t *sel;

		log_printf("Bootstrapping selected patches...\n");
		stack_update();

		/// Build the new run configuration
		json_array_foreach(sel_stack, i, sel) {
			json_array_append_new(new_cfg_patches, patch_build(sel));
		}
	}

	// Other default settings
	json_object_set_new(new_cfg, "console", json_false());
	json_object_set_new(new_cfg, "dat_dump", json_false());

	run_cfg_fn = run_cfg_fn_build(sel_stack);

	json_dump_file(new_cfg, run_cfg_fn, JSON_INDENT(2));
	log_printf("\n\nThe following run configuration has been written to %s:\n", run_cfg_fn);
	json_dump_log(new_cfg, JSON_INDENT(2));
	log_printf("\n\n");

	pause();

	// Step 2: Locate games
	games = ConfigureLocateGames(cur_dir);

	if(json_object_size(games) > 0) {
		CreateShortcuts(run_cfg_fn, games);
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
	}
end:
	SAFE_FREE(run_cfg_fn);

	json_decref(new_cfg);
	json_decref(sel_stack);
	json_decref(games);

	json_decref(args);

	VLA_FREE(cur_dir);
	json_decref(repo_list);

	pause();
	return 0;
}
