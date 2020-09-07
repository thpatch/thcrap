#pragma once

class IHttpHandle
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

    ~IHttpHandle() {}
    virtual Status download(const std::string& url, std::function<size_t(const uint8_t*, size_t)> writeCallback, std::function<bool(size_t, size_t)> progressCallback) = 0;
};
