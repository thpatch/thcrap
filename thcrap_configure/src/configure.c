/**
  * Touhou Community Reliant Automatic Patcher
  * Cheap command-line patch stack configuration tool
  */

#include <thcrap.h>
#include <thcrap_update/src/update.h>
#include "configure.h"

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

// Because I've now learned how bad pause() can be
void pause()
{
	int ret;
	printf("Press ENTER to continue . . . ");
	while((ret = getchar()) != '\n' && ret != EOF);
}

json_t* BootstrapPatch(const char *patch_id, const char *base_dir, json_t *remote_servers)
{
	const char *main_fn = "patch.js";

	json_t *patch_info;
	size_t patch_len;
	size_t local_dir_len;

	if(!patch_id || !base_dir) {
		return NULL;
	}
	patch_info = json_object();
	patch_len = strlen(patch_id) + 1;
	local_dir_len = strlen(base_dir) + 1 + patch_len + 1;

	{
		VLA(char, local_dir, local_dir_len);
		strcpy(local_dir, base_dir);
		strcat(local_dir, patch_id);
		strcat(local_dir, "/");
		json_object_set_new(patch_info, "archive", json_string(local_dir));
		VLA_FREE(local_dir);
	}

	if(patch_update(patch_info) == 1) {
		// Bootstrap the new patch by downloading patch.js and deleting its 'files' object
		char *patch_js_buffer;
		DWORD patch_js_size;
		json_t *patch_js;

		size_t remote_patch_fn_len = patch_len + 1 + strlen(main_fn) + 1;
		VLA(char, remote_patch_fn, remote_patch_fn_len);
		sprintf(remote_patch_fn, "%s/%s", patch_id, main_fn);

		patch_js_buffer = (char*)ServerDownloadFileA(remote_servers, remote_patch_fn, &patch_js_size, NULL);
		// TODO: Nice, friendly error

		VLA_FREE(remote_patch_fn);

		patch_js = json_loadb_report(patch_js_buffer, patch_js_size, 0, main_fn);
		SAFE_FREE(patch_js_buffer);

		json_object_del(patch_js, "files");
		patch_json_store(patch_info, main_fn, patch_js);
		json_decref(patch_js);
		patch_update(patch_info);
	}
	return patch_info;
}

char* run_cfg_fn_build(const json_t *patch_stack)
{
	size_t i;
	json_t *json_val;
	size_t run_cfg_fn_len = strlen(".js") + 1;
	char *run_cfg_fn = NULL;

	// Here at Touhou Patch Center, we love double loops.
	json_array_foreach(patch_stack, i, json_val) {
		if(json_is_string(json_val)) {
			run_cfg_fn_len += strlen(json_string_value(json_val)) + 1;
		}
	}

	run_cfg_fn = (char*)malloc(run_cfg_fn_len);
	run_cfg_fn[0] = 0;

	json_array_foreach(patch_stack, i, json_val) {
		const char *patch_id = json_string_value(json_val);

		if(run_cfg_fn[0]) {
			strcat(run_cfg_fn, "-");
		}

		if(!stricmp(patch_id, "base_tsa") && json_array_size(patch_stack) > 1) {
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
			const char *game_fn = json_object_get_string(games, key);
			STRLEN_DEC(run_cfg_fn);
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
	CoUninitialize();
}

int __cdecl wmain(int argc, wchar_t *wargv[])
{
	int ret = 0;

	const char *local_server_js_fn;
	json_t *local_server_js;
	json_t *server_js;

	json_t *local_servers;
	json_t *remote_servers;

	// JSON array containing ID strings of the selected patches
	json_t *patch_stack = json_array();

	json_t *run_cfg = NULL;

	size_t cur_dir_len = GetCurrentDirectory(0, NULL) + 1;
	VLA(char, cur_dir, cur_dir_len);
	char *base_dir = NULL;
	json_t *games = NULL;

	char *run_cfg_fn = NULL;

	json_t *args = json_array();

	log_init(1);

	// We need to do this here to allow *any* input of localized characters
	// in the console subsystem
	w32u8_set_fallback_codepage(GetOEMCP());

	GetCurrentDirectory(cur_dir_len, cur_dir);
	PathAddBackslashA(cur_dir);
	str_slash_normalize(cur_dir);

	// Convert command-line arguments
	{
		int i;
		for(i = 0; i < argc; i++) {
			size_t arg_len = (wcslen(wargv[i]) * UTF8_MUL) + 1;
			VLA(char, arg, arg_len);
			StringToUTF8(arg, __wargv[i], arg_len);
			json_array_append_new(args, json_string(arg));
			VLA_FREE(arg);
		}
	}

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

	if(argc > 1) {
		local_server_js_fn = json_array_get_string(args, 1);
	} else {
		local_server_js_fn = "server.js";
	}

	local_server_js = json_load_file_report(local_server_js_fn);
	if(!local_server_js) {
		log_mboxf(NULL, MB_ICONSTOP | MB_OK,
			"No %s file found in the current directory (%s).\n"
			"You can download one from\n"
			"\n"
			"\t\thttp://srv.thpatch.net/server.js\n", local_server_js_fn, cur_dir
		);
		ret = -1;
		goto end;
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
	);

	local_servers = ServerInit(local_server_js);
	if(local_servers) {
		// Display first server
		const json_t *server_first = json_array_get(local_servers, 0);
		if(json_is_object(server_first)) {
			const char *server_first_url = json_object_get_string(server_first, "url");
			if(server_first_url) {
				log_printf(
					"Using the definitions in [%s], we're going to pull patches from\n"
					"\n"
					"\t%s\n"
					"\n"
					"You can specify a different server definition file as a command-line parameter.\n"
					"\n"
					"\n",
					local_server_js_fn, server_first_url
				);
			}
		}
	}
	pause();

	{
		DWORD remote_server_js_size;
		void *remote_server_js_buffer;

		remote_server_js_buffer = ServerDownloadFileA(local_servers, "server.js", &remote_server_js_size, NULL);
		if(remote_server_js_buffer) {
			FILE *local_server_js_file = NULL;

			server_js = json_loadb_report(remote_server_js_buffer, remote_server_js_size, 0, "server.js");

			local_server_js_file = fopen(local_server_js_fn, "w");
			fwrite(remote_server_js_buffer, remote_server_js_size, 1, local_server_js_file);
			fclose(local_server_js_file);

			SAFE_FREE(remote_server_js_buffer);
			json_decref(local_server_js);
		} else {
			printf("Download of server.js failed.\nUsing local server definitions...\n");
			server_js = local_server_js;
			pause();
		}

		remote_servers = ServerInit(server_js);
		// TODO: Nice, friendly error
	}

	{
		json_t *json_val;
		const char *key;
		json_t *patches = json_object_get(server_js, "patches");

		// Push base_tsa into the list of selected patches, so that we can lock it later.
		// Really, we _don't_ want people to remove it accidentally
		json_object_foreach(patches, key, json_val) {
			if(!stricmp(key, "base_tsa")) {
				json_array_append_new(patch_stack, json_string(key));
			}
		}
		patch_stack = SelectPatchStack(server_js, patch_stack);
	}

	{
		size_t i;
		json_t *json_val;
		json_t *run_cfg_patches;
		const char *patch_dir;
		const char *server_id = json_object_get_string(server_js, "id");
		size_t base_dir_len = cur_dir_len + 1 + strlen(server_id) + 1;
		VLA(char, base_dir, base_dir_len);

		if(!patch_stack || !json_array_size(patch_stack)) {
			// Error...
		}

		// Construct directory from server ID
		if(server_id) {
			sprintf(base_dir, "%s%s/", cur_dir, server_id);
			CreateDirectory(base_dir, NULL);
			SetCurrentDirectory(base_dir);
			patch_dir = base_dir;
		} else {
			patch_dir = cur_dir;
		}

		run_cfg = json_object();
		run_cfg_patches = json_object_get_create(run_cfg, "patches", json_array());

		log_printf("Bootstrapping selected patches...\n");
		json_array_foreach(patch_stack, i, json_val) {
			const char *patch_id = json_string_value(json_val);
			json_t *patch_info = BootstrapPatch(patch_id, patch_dir, remote_servers);
			json_array_append_new(run_cfg_patches, patch_info);
		}
		VLA_FREE(base_dir);
	}

	// That's all on-line stuff we have to do
	json_decref(server_js);

	SetCurrentDirectory(cur_dir);

	// Other default run_cfg settings
	json_object_set_new(run_cfg, "console", json_false());
	json_object_set_new(run_cfg, "dat_dump", json_false());

	run_cfg_fn = run_cfg_fn_build(patch_stack);

	json_dump_file(run_cfg, run_cfg_fn, JSON_INDENT(2));
	log_printf("\n\nThe following run configuration has been written to %s:\n", run_cfg_fn);
	json_dump_log(run_cfg, JSON_INDENT(2));
	log_printf("\n\n");
	
	pause();

	runconfig_set(run_cfg);

	// Step 2: Locate games
	games = ConfigureLocateGames(cur_dir);

	if(json_object_size(games) > 0) 	{
		CreateShortcuts(run_cfg_fn, games);
		log_printf(
			"\n\nDone. You can now start the respective games with your selected configuration\n"
			"through the shortcuts created in the current directory\n"
			"(%s).\n", cur_dir
		);
	}
end:
	SAFE_FREE(run_cfg_fn);

	json_decref(run_cfg);
	json_decref(patch_stack);
	json_decref(games);

	json_decref(args);

	VLA_FREE(cur_dir);
	
	pause();
	return 0;
}
