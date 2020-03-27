#pragma once

#include <functional>
#include <string>
#include <Windows.h>
#include <wininet.h>

class HttpHandle
{
private:
    HINTERNET internet;

public:
    HttpHandle();
    HttpHandle(HttpHandle&& other);
    HttpHandle(HttpHandle& other) = delete;
    HttpHandle& operator=(HttpHandle& other) = delete;
    ~HttpHandle();

    bool download(const std::string& url, std::function<size_t(const uint8_t*, size_t)> writeCallback, std::function<bool(size_t, size_t)> progressCallback);
};


