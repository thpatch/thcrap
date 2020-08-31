#include "thcrap.h"
#include <cstring>
#include "update.h"
#include "server.h"
#include "strings_array.h"

PatchUpdate::PatchUpdate(Update& update, const patch_t *patch, std::list<std::string> servers)
    : update(update), patch(patch), servers(std::move(servers))
{}

get_status_t PatchUpdate::httpStatusToGetStatus(HttpHandle::Status status)
{
    switch (status) {
        case HttpHandle::Status::Ok:
            return GET_OK;
        case HttpHandle::Status::ClientError:
            return GET_CLIENT_ERROR;
        case HttpHandle::Status::ServerError:
            return GET_SERVER_ERROR;
        case HttpHandle::Status::Cancelled:
            return GET_CANCELLED;
        case HttpHandle::Status::Error:
            return GET_SYSTEM_ERROR;
        default:
            throw std::invalid_argument("Invalid status");
    }
}

bool PatchUpdate::callProgressCallback(const std::string& fn, const DownloadUrl& url, get_status_t getStatus,
                                       size_t file_progress, size_t file_size)
{
    if (this->update.progressCallback == nullptr) {
        return true;
    }

    progress_callback_status_t status;

    status.patch = this->patch;
    status.fn = fn.c_str();
    status.url = url.getUrl().c_str();
    status.status = getStatus;
    status.file_progress = file_progress;
    status.file_size = file_size;
    status.nb_files_downloaded = 0; // TODO
    status.nb_files_total = 0; // TODO

    return this->update.progressCallback(&status, this->update.progressData);
}

void PatchUpdate::onFilesJsComplete(std::shared_ptr<PatchUpdate> thisStorage, const std::vector<uint8_t>& data)
{
    if (thisStorage.get() != this) {
        throw std::logic_error("PatchUpdate::onFilesJsComplete: this and thisStorage must be identical");
    }

    patch_file_store(this->patch, "files.js", data.data(), data.size());

    ScopedJson filesJs = patch_json_load(this->patch, "files.js", nullptr);
    if (*filesJs == nullptr) {
        log_printf("%s: files.js isn't a valid json file!\n", this->patch->id);
        return ;
    }

    const char *key;
    json_t *value;
    json_object_foreach(*filesJs, key, value) {
        if (json_is_null(value)) {
            // Delete file
            patch_file_delete(this->patch, key);
            continue;
        }
        if (this->update.filterCallback != nullptr && this->update.filterCallback(key) == false) {
            // We don't want to download this file
            continue;
        }
        // TODO: do not redownload if the CRC32 didn't change

        // TODO: check CRC32
        this->update.mainDownloader.addFile(this->servers, key,
            [thisStorage, key = std::string(key)](const DownloadUrl& url, std::vector<uint8_t>& data) {
                patch_file_store(thisStorage->patch, key.c_str(), data.data(), data.size());
                thisStorage->callProgressCallback(key, url, GET_OK, data.size(), data.size());
            },
            [thisStorage, key = std::string(key)](const DownloadUrl& url, HttpHandle::Status httpStatus) {
                get_status_t getStatus = thisStorage->httpStatusToGetStatus(httpStatus);
                thisStorage->callProgressCallback(key, url, getStatus, 0, 0);
            },
            [thisStorage, key = std::string(key)](const DownloadUrl& url, size_t file_progress, size_t file_size) {
                return thisStorage->callProgressCallback(key, url, GET_DOWNLOADING, file_progress, file_size);
            }
        );
    }
}

void PatchUpdate::start(std::shared_ptr<PatchUpdate> thisStorage)
{
    if (thisStorage.get() != this) {
        throw std::logic_error("PatchUpdate::onFilesJsComplete: this and thisStorage must be identical");
    }

    this->update.filesJsDownloader.addFile(this->servers, "files.js",
        [thisStorage](const DownloadUrl&, std::vector<uint8_t>& data) {
            thisStorage->onFilesJsComplete(thisStorage, data);
        },
        [patch_id = std::string(this->patch->id)](const DownloadUrl& url, HttpHandle::Status httpStatus) {
            log_printf("%s files.js download from %s failed\n", patch_id.c_str(), url.getUrl().c_str());
        }
    );
}

Update::Update(Update::filter_t filterCallback,
               progress_callback_t progressCallback, void *progressData)
    : filterCallback(filterCallback), progressCallback(progressCallback), progressData(progressData)
{}

void Update::startPatchUpdate(const patch_t *patch)
{
    std::list<std::string> servers;
    for (size_t i = 0; patch->servers && patch->servers[i]; i++) {
        servers.push_back(patch->servers[i]);
    }

    if (servers.empty()) {
        log_printf("Empty server list in %s/patch.js\n", patch->archive);
        return ;
    }

    auto patchUpdateObj = std::make_shared<PatchUpdate>(*this, patch, std::move(servers));
    patchUpdateObj->start(patchUpdateObj);
}

void Update::run(const std::list<const patch_t*>& patchs)
{
    for (const patch_t* patch : patchs) {
        this->startPatchUpdate(patch);
    }

    this->filesJsDownloader.wait();
    // At this point, dlPatchFiles have been fully populated
    this->mainDownloader.wait();
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

void stack_update(update_filter_func_t filter_func, void *filter_data, progress_callback_t progress_callback, void *progress_param)
{
    auto filter_lambda =
        [filter_func, filter_data](const std::string& fn) -> bool {
            return filter_func(fn.c_str(), filter_data) != 0;
        };
    Update update(filter_lambda, progress_callback, progress_param);

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
                    log_printf("Error loading %s/patch.js\n", json_object_get_string(patch_info, "archive"));
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
    Update update(filter_lambda, progress_callback, progress_param);
    update.run(patches);

    strings_array_free(games);
}
