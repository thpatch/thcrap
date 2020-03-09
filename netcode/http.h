#pragma once

#include <functional>
#include <string>
#include <curl/curl.h>

class HttpHandle
{
private:
    CURL *curl;

    static size_t writeCallbackStatic(char *ptr, size_t size, size_t nmemb, void *userdata);

public:
    HttpHandle();
    HttpHandle(HttpHandle&& other);
    HttpHandle(HttpHandle& other) = delete;
    HttpHandle& operator=(HttpHandle& other) = delete;
    ~HttpHandle();

    bool download(const std::string& url, std::function<size_t(const uint8_t*, size_t)> writeCallback);
};


