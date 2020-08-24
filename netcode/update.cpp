#include "thcrap.h"
#include <cstring>
#include "update.h"
#include "server.h"
#include "strings_array.h"

PatchUpdate::PatchUpdate(Update& update, const patch_t *patch, std::list<std::string> servers)
    : update(update), patch(patch), servers(std::move(servers))
{}

bool PatchUpdate::onFilesJsComplete(std::shared_ptr<PatchUpdate> thisStorage, const File& file)
{
    if (thisStorage.get() != this) {
        throw std::logic_error("PatchUpdate::onFilesJsComplete: this and thisStorage must be identical");
    }

    patch_file_store(this->patch, "files.js", file.getData().data(), file.getData().size());

    ScopedJson filesJs = patch_json_load(this->patch, "files.js", nullptr);
    if (*filesJs == nullptr) {
        printf("%s: files.js isn't a valid json file!\n", this->patch->id);
        return false;
    }

    const char *key;
    json_t *value;
    json_object_foreach(*filesJs, key, value) {
        if (json_is_null(value)) {
            // TODO: delete file
            continue;
        }
        if (this->update.filterCallback(key) == false) {
            // We don't want to download this file
            continue;
        }
        // TODO: do not redownload if the CRC32 didn't change

        // TODO: check CRC32
        this->update.mainDownloader.addFile(this->servers, key, [thisStorage, key = std::string(key)](const File& file) {
            patch_file_store(thisStorage->patch, key.c_str(), file.getData().data(), file.getData().size());
            return true;
        });
    }

    return true;
}

bool PatchUpdate::start(std::shared_ptr<PatchUpdate> thisStorage)
{
    if (thisStorage.get() != this) {
        throw std::logic_error("PatchUpdate::onFilesJsComplete: this and thisStorage must be identical");
    }

    this->update.filesJsDownloader.addFile(this->servers, "files.js", [thisStorage](const File& file) {
        return thisStorage->onFilesJsComplete(thisStorage, file);
    });
    return true;
}

Update::Update(Update::filter_t filterCallback,
               Update::progress_t progressCallback)
    : filterCallback(filterCallback), progressCallback(progressCallback)
{}

bool Update::startPatchUpdate(const patch_t *patch)
{
    // TODO: see if we need to call patch_init of whatever function loads patch.js
    //ScopedJson patch = patch_json_load("patch.js", nullptr);
    //if (*patch == nullptr) {
    //    return false;
    //}

    std::list<std::string> servers;
    for (size_t i = 0; patch->servers && patch->servers[i]; i++) {
        servers.push_back(patch->servers[i]);
    }

    if (servers.empty()) {
        printf("Empty server list in %s/patch.js\n", patch->archive);
        return false;
    }

    auto patchUpdateObj = std::make_shared<PatchUpdate>(*this, patch, std::move(servers));
    return patchUpdateObj->start(patchUpdateObj);
}

bool Update::run(const std::list<const patch_t*>& patchs)
{
    bool ret;

    for (const patch_t* patch : patchs) {
        if (!this->startPatchUpdate(patch)) {
            // TODO:
            // - If we end up not needing the patch_init call in startPatchUpdate, this message is wrong.
            // - Even if we end up using it, we have the other failure case where patchUpdateObj->start() failed.
            printf("Could not load %s/%s\n", patch->archive, "patch.js");
            ret = false;
            // Continue and update the other patches
        }
    }

    if (!this->filesJsDownloader.wait()) {
        if (this->mainDownloader.count() > 0) {
            // At least one files.js download succeeded, use it
            ret = false;
        }
        else {
            // Nothing to download.
            return false;
        }
    }
    // At this point, dlPatchFiles have been fully populated
    if (!this->mainDownloader.wait()) {
        ret = false;
    }
    return ret;
}

int update_filter_global(const char *fn, void*)
{
	return strchr(fn, '/') == NULL;
}

int update_filter_games(const char *fn, void *param)
{
    const char **games = static_cast<const char **>(param);
	size_t fn_len = strlen(fn);

    if (!games) {
	    return update_filter_global(fn, nullptr);
    }

    for (size_t i = 0; games[i]; i++) {
		// We will need to match "th14", but not "th143".
		size_t game_len = strlen(games[i]);
		if(
			fn_len > game_len
			&& !strncmp(fn, games[i], game_len)
			&& fn[game_len] == '/'
		) {
			return 1;
		}
    }

	return update_filter_global(fn, NULL);
}

// TODO: remove this function. Instead, these call must work:
// stack_update_wrapper(update_filter_global_wrapper, NULL, progress_callback, NULL);
// stack_update_wrapper(update_filter_games_wrapper, filter, progress_callback, NULL);
//bool patches_update(const std::list<std::string>& patchPaths)
//{
//    return Update().run(patchPaths);
//}

// TODO (in caller), initialize the stack;
void stack_update(update_filter_func_t filter_func, void *filter_data, progress_callback_t progress_callback, void *progress_param)
{
    auto filter_lambda =
        [filter_func, filter_data](const std::string& fn) -> bool {
            return filter_func(fn.c_str(), filter_data) != 0;
        };
    auto progress_lambda =
        [progress_callback, progress_param]
            (const std::string& patchId, const std::string& fn, get_result_t ret,
             size_t dlCur, size_t dlTotal, size_t nbFilesCur, size_t nbFilesTotal) -> bool {
            return progress_callback(patchId.c_str(), fn.c_str(), ret, dlCur, dlTotal, nbFilesCur, nbFilesTotal, progress_param);
        };
    Update update(filter_lambda, progress_lambda);
    // TODO: use the filter and progress functions

    std::list<const patch_t*> patches;
    stack_foreach_cpp([&patches](const patch_t *patch) {
        patches.push_back(patch);
    });
    update.run(patches);
}

// Helper class to free a patch automatically when it goes out of scope
class ScopedPatch
{
private:
    bool active = true;
    patch_t patch;

public:
    // Automatic lifetime management
    ScopedPatch(patch_t patch)
        : patch(patch)
    {}
    ~ScopedPatch()
    {
        if (active) {
            patch_free(&patch);
        }
    }

    // Disallow copy
    ScopedPatch(const ScopedPatch&) = delete;
    ScopedPatch& operator=(const ScopedPatch&) = delete;

    // Allow move (needed for containers)
    ScopedPatch(ScopedPatch&& src)
        : patch(src.patch)
    {
        this->patch = src.patch;
        src.active = false;
    }
    // Containers don't need that one
    ScopedPatch& operator=(ScopedPatch&&) = delete;

    // We want to use them
    operator patch_t*()
    {
        return &this->patch;
    }
};

void global_update(progress_callback_t progress_callback, void *progress_param)
{
	std::list<const patch_t*> patches;
    // We want to free the patches we created, but not the ones
    // from the stack.
    // The ScopedPatch wrapper will free these patches automatically
    // when patches_storage goes out of scope.
	std::vector<ScopedPatch> patches_storage;

	// Get the patches from the stack
	stack_foreach_cpp([&patches](const patch_t *patch) {
		if (patch->archive) {
			patches.push_back(patch);
		}
	});

	// Add all the patches we can find in config files
    if (std::filesystem::is_directory("config")) {
        for (auto& dir_entry : std::filesystem::directory_iterator("config")) {
            std::filesystem::path path = dir_entry.path();
            if (path.extension() != ".js") {
                continue;
            }

	    	ScopedJson config = json_load_file(path.u8string().c_str(), 0, nullptr);
	    	if (!*config) {
	    		continue;
	    	}

	    	size_t i;
	    	json_t *patch_info;
	    	json_array_foreach(json_object_get(*config, "patches"), i, patch_info) {
	    		patch_t patch = patch_init(json_object_get_string(patch_info, "archive"), patch_info, 0);
                if (patch.id == nullptr) {
                    printf("Error loading %s/patch.js\n", json_object_get_string(patch_info, "archive"));
                    continue;
                }
	    		patch_rel_to_abs(&patch, path.c_str());
	    		if (patch.archive && !std::any_of(patches.begin(), patches.end(), [&patch](const patch_t *it) {
	    				return strcmp(patch.archive, it->archive) != 0;
	    			})) {
	    			patches_storage.push_back(patch);
	    			patches.push_back(patches_storage.back());
	    		}
	    		else {
	    			patch_free(&patch);
	    		}
	    	}
	    }
    }

    char **games = strings_array_create();
    {
	    ScopedJson games_js = json_load_file("config/games.js", 0, nullptr);
        const char *key;
        json_t *value;
        json_object_foreach(*games_js, key, value) {
            games = strings_array_add(games, key);
        }
    }

    auto filter_lambda =
        [games](const std::string& fn) -> bool {
            return update_filter_games(fn.c_str(), games);
        };
    // TODO: sync with the stack_update one if it changes
    auto progress_lambda =
        [progress_callback, progress_param]
            (const std::string& patchId, const std::string& fn, get_result_t ret,
             size_t dlCur, size_t dlTotal, size_t nbFilesCur, size_t nbFilesTotal) -> bool {
            return progress_callback(patchId.c_str(), fn.c_str(), ret, dlCur, dlTotal, nbFilesCur, nbFilesTotal, progress_param);
        };
    Update update(filter_lambda, progress_lambda);
    update.run(patches);

    strings_array_free(games);
}
