#pragma once

#include <functional>
#include <string>
#include <windows.h>
#include <wininet.h>
#include "http_interface.h"

class WininetHandle : public IHttpHandle
{
private:
    HINTERNET internet;

	static std::string setup_headers(DownloadCache *cache);
	static std::string read_header(HINTERNET hFile, DWORD header);
	static void update_cache(HINTERNET hFile, DownloadCache *cache);

public:
    WininetHandle();
    WininetHandle(WininetHandle&& other);
    WininetHandle(WininetHandle& other) = delete;
    WininetHandle& operator=(WininetHandle& other) = delete;
    ~WininetHandle();

    HttpStatus download(const std::string& url, std::function<size_t(const uint8_t*, size_t)> writeCallback, std::function<bool(size_t, size_t)> progressCallback, DownloadCache *cache) override;
};


