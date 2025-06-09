#pragma once

#include <optional>
#include <string>
#include <vector>
#include <stdint.h>

class DownloadCache
{
public:
	// Called before sending a request, in order to populate the corresponding headers in the request.
	virtual std::optional<std::string> getEtag() const = 0;
	virtual std::optional<std::string> getLastModifiedDate() const = 0;
	// Called after sending a request, if the server answered with a 304 Not Modified.
	virtual std::vector<uint8_t> getCachedFile() const = 0;
	// Called after sending a request.
	virtual void setCacheData(const char *etag, const char *lastModifiedDate) = 0;
};

class FilesJsDownloadCache : public DownloadCache
{
private:
	const patch_t *patch;
	std::optional<std::string> etag;
	std::optional<std::string> lastModifiedDate;

public:
	FilesJsDownloadCache(const patch_t *patch);

	// Called before sending a request, in order to populate the corresponding headers in the request.
	std::optional<std::string> getEtag() const;
	std::optional<std::string> getLastModifiedDate() const;
	// Called after sending a request, if the server answered with a 304 Not Modified.
	std::vector<uint8_t> getCachedFile() const;
	// Called after sending a request.
	// Note: this function is called right after the last progress callback call.
	// If you save the file on disk after receiving the Ok status from the file object
	// (which arrives later), make sure you don't override the values saved in setCacheData().
	void setCacheData(const char *etag, const char *lastModifiedDate);
};
