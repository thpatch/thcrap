#pragma once

#include <functional>
#include <string>
#include <Windows.h>
#include <wininet.h>
#include "http_interface.h"

class WininetHandle : public IHttpHandle
{
private:
    HINTERNET internet;

public:
    WininetHandle();
    WininetHandle(WininetHandle&& other);
    WininetHandle(WininetHandle& other) = delete;
    WininetHandle& operator=(WininetHandle& other) = delete;
    ~WininetHandle();

    Status download(const std::string& url, std::function<size_t(const uint8_t*, size_t)> writeCallback, std::function<bool(size_t, size_t)> progressCallback) override;
};


