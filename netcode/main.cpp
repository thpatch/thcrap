#include <chrono>
#include <cstring>
#include "repo_discovery.h"
#include "update.h"
#include "server.h"
#include "strings_array.h"

repo_t *find_repo_in_list(repo_t **repo_list, const char *repo_id)
{
	for (size_t i = 0; repo_list[i]; i++) {
		if (strcmp(repo_list[i]->id, repo_id) == 0) {
			return repo_list[i];
		}
	}
	return nullptr;
}

patch_t patch_bootstrap(const patch_desc_t *sel, const char * const *repo_servers)
{
    std::string url = repo_servers[0];
    url += sel->patch_id;
    url += "/patch.js";
    json_t *patch_js = ServerCache::get().downloadJsonFile(url);

	patch_t patch_info = patch_build(sel);
	patch_json_store(&patch_info, "patch.js", patch_js);
	// TODO: Nice, friendly error

	return patch_info;
}

bool progress_callback(progress_callback_status_t *status, void *param)
{
    using namespace std::literals;
    auto& files = *reinterpret_cast<std::map<std::string, std::chrono::steady_clock::time_point>*>(param);

    switch (status->status) {
        case GET_DOWNLOADING: {
            auto& file_time = files[status->url];
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
        case GET_SERVER_ERROR:
            log_printf("%s: server error\n", status->url);
            return true;
        case GET_CANCELLED:
            log_printf("%s: cancelled\n", status->url);
            return true;
        case GET_SYSTEM_ERROR:
            log_printf("%s: system error\n", status->url);
            return true;
        default:
            log_printf("%s: unknown status\n", status->url);
            return true;
    }
}

// Test reproducing the thcrap_configure behavior
int do_thcrap_configure()
{
    // Discover repos
	//const char *start_repo = "https://mirrors.thpatch.net/nmlgc/"; // TODO: why is it the default??
	const char *start_repo = "https://srv.thpatch.net/";
    if (RepoDiscoverAtURL(start_repo) != 0) {
        log_printf("Discovery from URL failed\n");
        return 1;
    }
    if (RepoDiscoverFromLocal() != 0) {
        log_printf("Discovery from local failed\n");
        return 1;
    }

    // Load repos
	repo_t **repo_list = RepoLoad();
	if (!repo_list[0]) {
		log_printf("No patch repositories available...\n");
        return 1;
	}

    // Select patches
	std::list<patch_desc_t> sel_stack = {
        { strdup("nmlgc"), strdup("base_tsa") },
        { strdup("nmlgc"), strdup("script_latin") },
        { strdup("nmlgc"), strdup("western_name_order") },
        { strdup("thpatch"), strdup("lang_en") },
    };
    for (auto& sel : sel_stack) {
        const repo_t *repo = find_repo_in_list(repo_list, sel.repo_id);
        patch_t patch_info = patch_bootstrap(&sel, repo->servers);
        patch_t patch_full = patch_init(patch_info.archive, nullptr, 0);
	    stack_add_patch(&patch_full);
	    patch_free(&patch_info);
    }

    // Download global data
    std::map<std::string, std::chrono::steady_clock::time_point> files;
	stack_update(update_filter_global, nullptr, progress_callback, &files);

	/// Build the new run configuration
	json_t *new_cfg = json_pack("{s[]}", "patches");
	json_t *new_cfg_patches = json_object_get(new_cfg, "patches");
	for (patch_desc_t& sel : sel_stack) {
		patch_t patch = patch_build(&sel);
		json_array_append_new(new_cfg_patches, patch_to_runconfig_json(&patch));
	}

    // Write config
    std::filesystem::create_directory("config");
	json_dump_file(new_cfg, "config/en.js", JSON_INDENT(2) | JSON_SORT_KEYS);

    // Locate games
    json_t *games_js = json_pack("{s:s,s:s}",
        "th06", "/some/path",
        "th07", "/some/other/path"
    );
    char **games = strings_array_create_and_fill(2, "th06", "th07");

    // Download games data
    files.clear();
	stack_update(update_filter_games, games, progress_callback, &files);

    json_dump_file(games_js, "config/games.js", JSON_INDENT(2) | JSON_SORT_KEYS);

    return 0;
}

int main()
{
    // TODO: move into thcrap
#ifdef USE_HTTP_CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
#endif

    return do_thcrap_configure();
}
