#include "thcrap.h"
#include <deque>
#include <sstream>
#include <cstring>
#include "update.h"
#include "server.h"
#include "strings_array.h"
#include "3rdparty/crc32.h"

Update::Update(Update::filter_t filterCallback,
               progress_callback_t progressCallback, void *progressData)
    : filterCallback(filterCallback), progressCallback(progressCallback), progressData(progressData)
{
    crc32::generate_table(this->crc32Table);
}

get_status_t Update::httpStatusToGetStatus(HttpStatus status)
{
    switch (status.get()) {
        case HttpStatus::Ok:
            return GET_OK;
        case HttpStatus::Cancelled:
            return GET_CANCELLED;
        case HttpStatus::ClientError:
            return GET_CLIENT_ERROR;
        case HttpStatus::ServerError:
            return GET_SERVER_ERROR;
        case HttpStatus::SystemError:
            return GET_SYSTEM_ERROR;
        default:
            throw std::invalid_argument("Invalid status");
    }
}

bool Update::callProgressCallback(const patch_t *patch, const std::string& fn, const DownloadUrl& url, get_status_t getStatus, std::string error,
                                  size_t file_progress, size_t file_size)
{
    if (this->progressCallback == nullptr) {
        return true;
    }

    progress_callback_status_t status;

    status.patch = patch;
    status.fn = fn.c_str();
    status.url = url.getUrl().c_str();
    status.file_progress = file_progress;
    status.file_size = file_size;

    status.status = getStatus;
    if (getStatus == GET_CLIENT_ERROR || getStatus == GET_SERVER_ERROR || getStatus == GET_SYSTEM_ERROR) {
        status.error = error.c_str();
    }
    else {
        status.error = nullptr;
    }

    status.nb_files_downloaded = this->mainDownloader.current();
    if (this->filesJsDownloader.current() == this->filesJsDownloader.total()) {
        status.nb_files_total = this->mainDownloader.total();
    }
    else {
        status.nb_files_total = 0;
    }

    return this->progressCallback(&status, this->progressData);
}

class AutoWriteJson
{
private:
    json_t *json;
    const patch_t *patch;
    const char *fn;

public:
    AutoWriteJson(const patch_t *patch, const char *fn)
        : json(patch_json_load(patch, fn, nullptr)), patch(patch), fn(fn)
    {
        if (!this->json) {
            this->json = json_object();
        }
    }
    ~AutoWriteJson()
    {
        patch_json_store(this->patch, fn, this->json);
        json_decref(this->json);
    }
    operator json_t*() {
        return this->json;
    }

    AutoWriteJson(const AutoWriteJson&) = delete;
    AutoWriteJson(AutoWriteJson&&) = delete;
    AutoWriteJson& operator=(const AutoWriteJson&) = delete;
    AutoWriteJson& operator=(AutoWriteJson&&) = delete;
};

std::string Update::fnToUrl(const std::string& url, uint32_t crc32)
{
    // Formatting with streams is trash. Hopefully we can replace this
    // with the C++20 std::format.
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');

    auto is_valid_for_uri = [](unsigned char c) {
        if ('A' <= c && c <= 'Z') {
            return true;
        }
        if ('a' <= c && c <= 'z') {
            return true;
        }
        if ('0' <= c && c <= '9') {
            return true;
        }
        if (c == '-' || c == '_' || c == '.' || c == '~') {
            return true;
        }
        return false;
    };

    // Escape illegal characters
    for (unsigned char c : url) {
        if (is_valid_for_uri(c) || c == '/') {
            ss.put(c);
        }
        else {
            ss << '%'<< std::setw(2) << static_cast<unsigned int>(c) << std::setw(0);
        }
    }

    // Append CRC32 in order to get the correct version from the CDN
    ss << "?crc32=" << std::setw(8) << crc32 << std::setw(0);
    return ss.str();
}

void Update::onFilesJsComplete(const patch_t *patch, const std::vector<uint8_t>& data)
{
    auto localFilesJs = std::make_shared<AutoWriteJson>(patch, "files.js");
    ScopedJson remoteFilesJs = json5_loadb(data.data(), data.size(), nullptr);

    if (*remoteFilesJs == nullptr) {
        log_printf("%s: files.js isn't a valid json file!\n", patch->id);
        return ;
    }

    const char *fn;
    json_t *value;
    json_object_foreach_fast(*remoteFilesJs, fn, value) {
        json_t *localValue = json_object_get(*localFilesJs, fn);
        // Did someone simply drop a full files.js into a standalone
        // package that doesn't actually come with the files for
        // every game?
        // (Necessary in case this patch installation should later
        // cover more games. If the remote files haven't changed by
        // then, they wouldn't be downloaded if files.js pretends
        // that these versions already exist locally.)
        if (localValue && !patch_file_exists(patch, fn)) {
            json_object_del(*localFilesJs, fn);
            localValue = nullptr;
        }
        if (localValue && json_equal(value, localValue)) {
            // The file didn't change since our last update, no need to update or delete it
            continue;
        }
        if (this->filterCallback != nullptr && this->filterCallback(fn) == false) {
            // We don't want to download this file
            continue;
        }
        if (json_is_null(value)) {
            // Delete file
            if (patch_file_exists(patch, fn)) {
                log_printf("Deleting %s/%s\n", patch->id, fn);
                patch_file_delete(patch, fn);
            }
            json_object_set(*localFilesJs, fn, json_null());
            continue;
        }

        this->mainDownloader.addFile(patch->servers, this->fnToUrl(fn, (uint32_t)json_integer_value(value)),

            // Success callback
            [this, patch, fn = std::string(fn), localFilesJs, value = ScopedJson(json_incref(value))]
            (const DownloadUrl& url, std::vector<uint8_t>& data) mutable {
                if (crc32::update(this->crc32Table, 0, data.data(), data.size()) != json_integer_value(*value)) {
                    this->callProgressCallback(patch, fn, url, GET_CRC32_ERROR);
                    return ;
                }
                if (patch_file_store(patch, fn.c_str(), data.data(), data.size()) != 0) {
                    this->callProgressCallback(patch, fn, url, GET_SYSTEM_ERROR, "file write failed");
                    return ;
                }
                this->callProgressCallback(patch, fn, url, GET_OK, "", data.size(), data.size());
                json_object_set(*localFilesJs, fn.c_str(), *value);
            },

            // Failure callback
            [this, patch, fn = std::string(fn)](const DownloadUrl& url, HttpStatus httpStatus) {
                get_status_t getStatus = this->httpStatusToGetStatus(httpStatus);
                this->callProgressCallback(patch, fn, url, getStatus, httpStatus.toString());
            },

            // Progress callback
            [this, patch, fn = std::string(fn)](const DownloadUrl& url, size_t file_progress, size_t file_size) {
                return this->callProgressCallback(patch, fn, url, GET_DOWNLOADING, "", file_progress, file_size);
            }
        );
    }
}

void Update::startPatchUpdate(const patch_t *patch)
{
    if (patch->id == nullptr) {
        log_printf("Invalid patch at %s\n", patch->archive);
        return ;
    }
    if (patch->servers == nullptr || patch->servers[0] == nullptr) {
        log_printf("(%s doesn't have any servers, not updating.)\n", patch->id);
        return;
    }
    // Assuming the repo/patch hierarchy here, but if we ever support
	// repository-less Git patches, they won't be using the files.js
	// protocol anyway.
    if (patch_file_exists(patch, "../.git")) {
		log_printf("(%s is under revision control, not updating.)\n", patch->id);
        return ;
    }
    if (patch->update == false) {
		// Updating manually deactivated on this patch
		log_printf("(%s has updates disabled, not updating.)\n", patch->id);
        return ;
    }

    this->filesJsDownloader.addFile(patch->servers, "files.js",
        [this, patch](const DownloadUrl&, std::vector<uint8_t>& data) {
            this->onFilesJsComplete(patch, data);
        },
        [patch_id = std::string(patch->id)](const DownloadUrl& url, HttpStatus httpStatus) {
            if (httpStatus.get() == HttpStatus::Cancelled) {
                // Another file finished before
                return ;
            }
            log_printf("Downloading files.js from %s failed: %s\n", url.getUrl().c_str(), httpStatus.toString().c_str());
        }
    );
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

patch_t patch_bootstrap(const patch_desc_t *sel, const repo_t *repo)
{
    if (!repo || !repo->servers || !repo->servers[0]) {
        return patch_build(sel);
    }

    std::string url = repo->servers[0];
    url += sel->patch_id;
    url += "/patch.js";
    auto [patch_js, _] = ServerCache::get().downloadJsonFile(url);

    RepoWrite(repo);
	patch_t patch_info = patch_build(sel);
	patch_json_store(&patch_info, "patch.js", *patch_js);
	// TODO: Nice, friendly error

	return patch_info;
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

        // Dirty hack - th123 requires th105 files
        const char *th105_prefix = "th105/";
        if (
            fn_len > strlen(th105_prefix)
            && strcmp(games[i], "th123") == 0
            && strncmp(fn, th105_prefix, strlen(th105_prefix)) == 0
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
	// Deque is used for its reference stability.
	std::deque<ScopedPatch> patches_storage;

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

            ScopedJson config = json_load_file_report(path.u8string().c_str());
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
	    		patch_rel_to_abs(&patch, path.generic_u8string().c_str());
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
	    ScopedJson games_js = json_load_file_report("config/games.js");
        const char *key;
        json_object_foreach_key(*games_js, key) {
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
