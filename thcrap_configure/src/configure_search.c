/**
  * Touhou Community Reliant Automatic Patcher
  * Cheap command-line patch stack configuration tool
  *
  * ----
  *
  * Game searching front-end code.
  */

#include <thcrap.h>
#include "configure.h"

static const char games_js_fn[] = "games.js";

static const char* ChooseLocation(const char *id, json_t *locs)
{
	size_t num_versions = json_object_size(locs);
	if(num_versions == 1) {
		const char *loc = json_object_iter_key(json_object_iter(locs));
		const char *variety = json_object_get_string(locs, loc);

		log_printf("%s trouve (%s) a %s\n", id, variety, loc);

		return loc;
	} else if(num_versions > 1) {
		const char *loc;
		json_t *val;
		size_t i = 0;
		size_t loc_num;

		log_printf("%d versions de %s trouvees:\n\n", num_versions, id);

		json_object_foreach(locs, loc, val) {
			log_printf(" [%2d] %s: %s\n", ++i, loc, json_string_value(val));
		}
		printf("\n");
		while(1) {
			char buf[16];
			printf("Choisissez une version sur laquelle executer le patch (1 - %u): ", num_versions);

			fgets(buf, sizeof(buf), stdin);
			if(
				(sscanf(buf, "%u", &loc_num) == 1) &&
				(loc_num <= num_versions) &&
				(loc_num >= 1)
			) {
				break;
			}
		}
		i = 0;
		json_object_foreach(locs, loc, val) {
			if(++i == loc_num) {
				return loc;
			}
		}
	}
	return NULL;
}

json_t* ConfigureLocateGames(const char *games_js_path)
{
	json_t *games;
	json_t *found;
	char search_path[MAX_PATH * 2] = {0};

	cls(0);

	log_printf("---------------------\n");
	log_printf("Localisation des jeux\n");
	log_printf("---------------------\n");
	log_printf(
		"\n"
		"\n"
	);

	games = json_load_file_report("games.js");
	if(json_object_size(games) != 0) {
		log_printf("Vous disposez deja de %s avec les contenus suivant\n\n", games_js_fn);
		json_dump_log(games, JSON_INDENT(2) | JSON_SORT_KEYS);
		log_printf("\n\n");
		if(Ask("Recommencer le scan de l'emplacement de ces jeux?")) {
			json_object_clear(games);
		} else if(!Ask("Ajouter de nouveaux jeux a cette liste?")) {
			return games;
		}
	} else {
		games = json_object();
		log_printf(
			"Vous n'avez pas encore le %s\n"
			"\n"
			"Nous avons donc besoin de rechercher tout les jeux Touhou installes sur votre ordinateur.\n"
			"Cela n'a besoin d'etre fait qu'une seule fois - a moins que vous ne changiez vos jeux de\n"
			"dossier plus tard\n"

			"\n"
			"Cela peut prendre un moment selon le nombre de disque dur present\n"
			"et la structure de vos dossiers. Vous pouvez accelerer le processus en\n"
			"indiquant le dossier racine de tous vos jeux Touhou.\n"
			"\n"
			"Par exemple, si vous avez 'Double Dealing Character' dans\n"
			"\n"
				"\tC:\\Games\\Touhou\\DDC\\\n"
			"\n"
			"et 'Imperisable Night' dans\n"
			"\n"
				"\tC:\\Games\\Touhou\\IN\\\n"
			"\n"
			"Vous devez indiquer\n"
			"\n"
				"\tC:\\Games\\Touhou\\\n"
			"\n"
			"... a moins que vos jeux Touhou ne soient eparpilles partout sur vos disques durs -\n"
			"auquel cas, vous n'aurez pas d'autre choix que de faire un scan complet.\n"
			"\n"
			"\n",
			games_js_fn
		);
	}

	while(1) {
		log_printf(
			"Dossier racine pour la recherche\n"
			"( laisser vide pour un scan complet du systeme )"
		);
		console_read(search_path, sizeof(search_path));
		log_printf("\n");

		if(search_path[0] == '\0') {
			break;
		}

		str_slash_normalize_win(search_path);
		PathAddBackslashA(search_path);

		if(!PathFileExists(search_path)) {
			log_printf("Ahem... L'emplacement indique (%s) n'existe pas\n", search_path);
		} else {
			break;
		}
	}
	log_printf("Recherche en cours... cela peut prendre un moment\n\n");
	found = SearchForGames(search_path, games);
	if(json_object_size(found)) {
		char *games_js_str = NULL;
		const char *id;
		json_t *locs;

		json_object_foreach(found, id, locs) {
			const char *loc = ChooseLocation(id, locs);
			json_object_set_new(games, id, json_string(loc));
			printf("\n");
		}

		SetCurrentDirectory(games_js_path);

		games_js_str = json_dumps(games, JSON_INDENT(2) | JSON_SORT_KEYS);
		if(!file_write(games_js_fn, games_js_str, strlen(games_js_str))) {
			log_printf("Les emplacements des jeux suivants ont ete identifies et ajoutes a %s:\n", games_js_fn);
			log_printf(games_js_str);
			log_printf("\n");
		} else if(!file_write_error(games_js_fn)) {
			games = json_decref_safe(games);
		}
		SAFE_FREE(games_js_str);
	} else {
		log_printf("Aucun nouvel emplacement de jeux trouve\n");
	}
	json_decref(found);
	return games;
}
