#pragma once

#include <string>
#include "http_status.h"
#include "download_cache.h"

class IHttpHandle
{
public:
    virtual ~IHttpHandle() {}
    virtual HttpStatus download(const std::string& url, std::function<size_t(const uint8_t*, size_t)> writeCallback, std::function<bool(size_t, size_t)> progressCallback, DownloadCache *cache) = 0;
};
