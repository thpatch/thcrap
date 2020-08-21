#pragma once

#include <functional>
#include <string>
#include <curl/curl.h>

class HttpHandle
{
public:
    enum class Status
    {
        // 200 - success
        Ok,
        // 3XX and 4XX - file not found, not accessible, moved, etc.
        ClientError,
        // 5XX errors - server errors, further requests are likely to fail.
        // Also covers weird error codes like 1XX and 2XX which we shouldn't see.
        ServerError,
        // Download cancelled by the progress callback, or another client
        // declared the server as dead
        Cancelled,
        // Error returned by the download library or by the write callback
        Error
    };

private:
    CURL *curl;

    static size_t writeCallbackStatic(char *ptr, size_t size, size_t nmemb, void *userdata);
    static int progressCallbackStatic(void *userdata, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);

public:
    HttpHandle();
    HttpHandle(HttpHandle&& other);
    HttpHandle(HttpHandle& other) = delete;
    HttpHandle& operator=(HttpHandle& other) = delete;
    ~HttpHandle();

    Status download(const std::string& url, std::function<size_t(const uint8_t*, size_t)> writeCallback, std::function<bool(size_t, size_t)> progressCallback);
};


