#include "thcrap.h"
#include "download_cache.h"

FilesJsDownloadCache::FilesJsDownloadCache(const patch_t *patch)
	: patch(patch)
{
	json_t *files_js = patch_json_load(this->patch, "files.js", nullptr);
	if (files_js == nullptr) {
		return;
	}

	json_t *cache_data = json_object_get(files_js, ".http_cache_data");
	if (cache_data) {
		json_t *etag = json_object_get(cache_data, "etag");
		if (json_is_string(etag)) {
			this->etag = json_string_value(etag);
		}

		json_t *lastModifiedDate = json_object_get(cache_data, "lastModifiedDate");
		if (json_is_string(lastModifiedDate)) {
			this->lastModifiedDate = json_string_value(lastModifiedDate);
		}

		json_decref(cache_data);
	}

	json_decref(files_js);
}

std::optional<std::string> FilesJsDownloadCache::getEtag() const
{
	return this->etag;
}

std::optional<std::string> FilesJsDownloadCache::getLastModifiedDate() const
{
	return this->lastModifiedDate;
}

std::vector<uint8_t> FilesJsDownloadCache::getCachedFile() const
{
	size_t files_js_size;
	uint8_t *files_js = (uint8_t*)patch_file_load(patch, "files.js", &files_js_size);
	if (files_js == nullptr) {
		return {};
	}

	std::vector<uint8_t> ret = { files_js, files_js + files_js_size };
	thcrap_free(files_js);
	return ret;
}

void FilesJsDownloadCache::setCacheData(const char *etag, const char *lastModifiedDate)
{
	json_t *files_js = patch_json_load(patch, "files.js", nullptr);
	if (files_js == nullptr) {
		return ;
	}

	json_t *cache_data = json_object();
	if (etag) {
		json_object_set_new(cache_data, "etag", json_string(etag));
	}
	if (lastModifiedDate) {
		json_object_set_new(cache_data, "lastModifiedDate", json_string(lastModifiedDate));
	}

	json_object_set_new(files_js, ".http_cache_data", cache_data);
	patch_json_store(this->patch, "files.js", files_js);

	json_decref(files_js);
}
