/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Games search on disk
  */

#pragma once

typedef struct games_js_entry
{
	const char *id;
	const char *path;
} games_js_entry;

typedef struct game_search_result
{
	char *path;
	char *id;
	char *build;
	// Description based on the version and the variety
	char *description;

#ifdef __cplusplus
	game_search_result()
		: path(nullptr), id(nullptr), build(nullptr), description(nullptr)
	{}

	game_search_result(game_version&& ver)
		: path(nullptr), id(ver.id), build(ver.build), description(nullptr)
	{
		ver.id = nullptr;
		ver.build = nullptr;
	}

	game_search_result copy()
	{
		game_search_result other;
		other.path        = strdup(this->path);
		other.id          = strdup(this->id);
		other.build       = strdup(this->build);
		other.description = strdup(this->description);
		return other;
	}
#endif
} game_search_result;


// Search for games recognized by thcrap in the given directory.
// If [dir] is NULL or an empty string, it will search on the whole system.
// [games_in] contains the list of games already known before the search.
game_search_result* SearchForGames(const wchar_t *dir, const games_js_entry *games_in);

// Free the return of SearchForGames
void SearchForGames_free(game_search_result *games);
