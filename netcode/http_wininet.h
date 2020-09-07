#pragma once

#include <functional>
#include <string>
#include <Windows.h>
#include <wininet.h>
#include "http_interface.h"

class HttpWininet : public IHttpHandle
{
private:
    HINTERNET internet;

public:
    HttpWininet();
    HttpWininet(HttpWininet&& other);
    HttpWininet(HttpWininet& other) = delete;
    HttpWininet& operator=(HttpWininet& other) = delete;
    ~HttpWininet();

    Status download(const std::string& url, std::function<size_t(const uint8_t*, size_t)> writeCallback, std::function<bool(size_t, size_t)> progressCallback) override;
};


