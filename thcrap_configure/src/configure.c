/**
  * Touhou Community Reliant Automatic Patcher
  * Cheap command-line patch stack configuration tool
  */

#include <thcrap.h>
#include <thcrap_update/src/update.h>
#include "configure.h"
#include "repo.h"

typedef enum {
	LINK_FN,
	LINK_ARGS,
	RUN_CFG_FN,
	RUN_CFG_FN_JS
} configure_slot_t;

int wine_flag = 0;

int file_write_error(const char *fn)
{
	static int error_nag = 0;
	log_printf(
		"\n"
		"Erreur d'ecriture dans %s !\n"
		"Vous n'avez probablement pas le droit d'ecriture dans le dossier courant,\n"
		"ou le fichier est protege en ecriture.\n",
		fn
	);
	if(!error_nag) {
		log_printf("L'ecriture risque egalement d'echouer pour tous les fichiers restants.\n");
		error_nag = 1;
	}
	return Ask("Continuer quand meme la configuration ?");
}

int Ask(const char *question)
{
	int ret = 0;
	while(ret != 'y' && ret != 'n') {
		char buf[2];
		if(question) {
			log_printf(question);
		}
		log_printf(" oui(y)/non(n) ");

		console_read(buf, sizeof(buf));
		ret = tolower(buf[0]);
	}
	return ret == 'y';
}

char* console_read(char *str, int n)
{
	int ret;
	int i;
	fgets(str, n, stdin);
	{
		// Ensure UTF-8
		VLA(wchar_t, str_w, n);
		StringToUTF16(str_w, str, n);
		StringToUTF8(str, str_w, n);
		VLA_FREE(str_w);
	}
	// Get rid of the \n
	for(i = 0; i < n; i++) {
		if(str[i] == '\n') {
			str[i] = 0;
			return str;
		}
	}
	while((ret = getchar()) != '\n' && ret != EOF);
	return str;
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
	printf("Appuyez sur ENTER pour continuer");
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
		if(!strnicmp(patch_id, "lang_", 5)) {
			skip = 1;
			break;
		}
	}

	json_array_foreach(sel_stack, i, sel) {
		const char *patch_id = json_array_get_string(sel, 1);
		if(!patch_id) {
			continue;
		}

		if(ret && ret[0]) {
			ret = strings_strcat(slot, "-");
		}

		if(!strnicmp(patch_id, "lang_", 5)) {
			patch_id += 5;
			skip = 0;
		}
		if(!skip) {
			ret = strings_strcat(slot, patch_id);
		}
	}
	return ret;
}

int CreateShortcuts(const char *run_cfg_fn, json_t *games)
{
#ifdef _DEBUG
	const char *loader_exe = "thcrap_loader_d.exe";
#else
	const char *loader_exe = "thcrap_loader.exe";
#endif
	int ret = 0;
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

		log_printf("Creation des raccourcis");

		json_object_foreach(games, key, cur_game) {
			const char *game_fn = json_string_value(cur_game);
			const char *link_fn = strings_sprintf(LINK_FN, "%s (%s).lnk", key, run_cfg_fn);
			const char *link_args = strings_sprintf(LINK_ARGS, "\"%s.js\" %s", run_cfg_fn, key);

			log_printf(".");

			if(
				CreateLink(link_fn, self_fn, link_args, self_path, game_fn)
				&& !file_write_error(link_fn)
			) {
				ret = 1;
				break;
			}
		}
		VLA_FREE(self_path);
	}
	VLA_FREE(self_fn);
	CoUninitialize();
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
			"Entrez un nom pour cette configuration, ou appuyez sur Entree pour utiliser le\n"
			"nom par defaut (%s): ", run_cfg_fn
		);
		console_read(run_cfg_fn_new, sizeof(run_cfg_fn_new));
		if(run_cfg_fn_new[0]) {
			run_cfg_fn = strings_sprintf(slot_fn, "%s", run_cfg_fn_new);
		}
		run_cfg_fn_js = strings_sprintf(slot_js, "%s.js", run_cfg_fn);
		if(PathFileExists(run_cfg_fn_js)) {
			log_printf("\"%s\" existe deja. ", run_cfg_fn_js);
			ret = !Ask("Ecraser ?");
		} else {
			ret = 0;
		}
	} while(ret);
	return run_cfg_fn;
}

int __cdecl wmain(int argc, wchar_t *wargv[])
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

	json_t *args = json_array_from_wchar_array(argc, wargv);

	wine_flag = GetProcAddress(
		GetModuleHandleA("kernel32.dll"), "wine_get_unix_file_name"
	) != 0;

	strings_mod_init();
	log_init(1);

	// Necessary to correctly process *any* input of non-ASCII characters
	// in the console subsystem
	w32u8_set_fallback_codepage(GetOEMCP());

	GetCurrentDirectory(cur_dir_len, cur_dir);
	PathAddBackslashA(cur_dir);
	str_slash_normalize(cur_dir);

	// Maximize the height of the console window... unless we're running under
	// Wine, where this 1) doesn't work and 2) messes up the console buffer
	if(!wine_flag) {
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

	http_init();

	if(json_array_size(args) > 1) {
		start_repo = json_array_get_string(args, 1);
	}

	log_printf(
		"==============================================================================\n"
		"Patcheur automatique de la communaute Touhou - Outil de configuration de patch\n"
		"==============================================================================\n"
		"Outil cree par Touhou Patch Center. http://thpatch.net\n"
		"\n"
		"\n"
		"Traduction en francais par Touhou-Online:\n"
		"\n"
		" @@@@@@@@          @@                       @@@@         @@ @@             \n"
		"    @@             @@                      @@@@@@        @@ @@             \n"
		"    @@             @@                      @@  @@        @@                \n"
		"    @@   @@  @@ @@ @@@@    @@  @@ @@       @@  @@  @@@@  @@ @@  @@@@   @@  \n"
		"    @@  @@@@ @@ @@ @@@@@  @@@@ @@ @@       @@  @@ @@@@@@ @@ @@ @@@@@@ @@@@ \n"
		"    @@  @  @ @@ @@ @@  @@ @  @ @@ @@  @@@  @@  @@ @@  @@ @@ @@ @@  @@ @  @ \n"
		"    @@  @  @ @@ @@ @@  @@ @  @ @@ @@       @@  @@ @@  @@ @@ @@ @@  @@ @  @ \n"
		"    @@  @  @ @@ @@ @@  @@ @  @ @@ @@       @@  @@ @@  @@ @@ @@ @@  @@ @@@  \n"
		"    @@  @  @ @@ @@ @@  @@ @  @ @@ @@       @@  @@ @@  @@ @@ @@ @@  @@ @    \n"
		"    @@  @@@@ @@ @@ @@  @@ @@@@ @@ @@       @@@@@@ @@  @@ @@ @@ @@  @@ @@   \n"
		"    @@   @@   @@@@ @@  @@  @@   @@@@        @@@@  @@  @@ @@ @@ @@  @@  @@@ \n"
		"\n"
		"=> http://www.touhou-online.net\n"
		"\n"
		"\n"
		"Cet outil va creer un nouvelle configuration de patch pour le\n"
		"Patcheur automatique de la communaute Touhou\n"
		"\n"
		"Ce processus se fera en quatre etapes:\n"
		"\n"
		"\t\t1. Selection des patchs\n"
		"\t\t2. Telechargement des donnees independantes des jeux\n"
		"\t\t3. Localisation des dossiers d'installation des jeux\n"
		"\t\t4. Telechargement des donnees specifiques aux jeux\n"
		"\n"
		"\n"
		"\n"
		"La recherche dans le depot des patchs commencera a\n"
		"\n"
		"\t%s\n"
		"\n"
		"Vous pouvez specifier une URL differente en l'ajoutant en parametre de ligne de commande.\n"
		"De plus, tous les patchs provenant de depots precedemment trouves et\n"
		"stockes dans des sous-dossiers du dossier courant pourront etre selectionnes.\n"
		"\n"
		"\n",
		start_repo
	);
	pause();

	if(RepoDiscoverAtURL(start_repo, id_cache, url_cache)) {
		goto end;
	}
	if(RepoDiscoverFromLocal(id_cache, url_cache)) {
		goto end;
	}
	repo_list = RepoLoad();
	if(!json_object_size(repo_list)) {
		log_printf("Aucun depot de patchs disponible...\n");
		pause();
		goto end;
	}
	sel_stack = SelectPatchStack(repo_list);
	if(json_array_size(sel_stack)) {
		json_t *new_cfg_patches = json_object_get(new_cfg, "patches");
		size_t i;
		json_t *sel;

		log_printf("Telechargement des donnees independantes des jeux...\n");
		stack_update(update_filter_global, NULL);

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
	if(!file_write(run_cfg_fn_js, run_cfg_str, strlen(run_cfg_str))) {
		log_printf("\n\nLa configuration suivante a ete ecrite dans %s:\n", run_cfg_fn);
		log_printf(run_cfg_str);
		log_printf("\n\n");
		pause();
	} else if(!file_write_error(run_cfg_fn_js)) {
		goto end;
	}

	// Step 2: Locate games
	games = ConfigureLocateGames(cur_dir);

	if(json_object_size(games) > 0 && !CreateShortcuts(run_cfg_fn, games)) {
		json_t *filter = json_object_get_keys_sorted(games);
		log_printf("\nDownloading data specific to the located games...\n");
		stack_update(update_filter_games, filter);
		filter = json_decref_safe(filter);
		log_printf(
			"\n"
			"\n"
			"Termine.\n"
			"\n"
			"\n\nVous pouvez maintenant lancer les differents jeux patches avec la configuration\n"
			"selectionnee en double-cliquant sur les raccourcis crees dans le dossier actuel\n"
			"(%s).\n"
			"\n"
			"Ces raccourcis fonctionnent partout. Vous pouvez donc les deplacer a votre gre.\n"
			"\n", cur_dir
		);
		pause();
	}
end:
	SAFE_FREE(run_cfg_str);
	json_decref(new_cfg);
	json_decref(sel_stack);
	json_decref(games);

	json_decref(args);

	VLA_FREE(cur_dir);
	json_decref(repo_list);
	json_decref(url_cache);
	json_decref(id_cache);
	return 0;
}
