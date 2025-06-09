#pragma once

#include <functional>
#include <string>
#include <curl/curl.h>
#include "http_interface.h"
#include "download_cache.h"

class CurlHandle : public IHttpHandle
{
private:
    CURL *curl;

    static size_t writeCallbackStatic(char *ptr, size_t size, size_t nmemb, void *userdata);
    static int progressCallbackStatic(void *userdata, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);

public:
    CurlHandle();
    CurlHandle(CurlHandle&& other);
    CurlHandle(CurlHandle& other) = delete;
    CurlHandle& operator=(CurlHandle& other) = delete;
    ~CurlHandle();

    HttpStatus download(const std::string& url, std::function<size_t(const uint8_t*, size_t)> writeCallback, std::function<bool(size_t, size_t)> progressCallback, DownloadCache *cache) override;
};
