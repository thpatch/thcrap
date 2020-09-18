#pragma once

#include <string>
#include "thcrap_update_api.h"

class HttpStatus
{
public:
    enum Status {
        // 200 - success
        Ok,
        // Download cancelled by the progress callback, or another client
        // declared the server as dead
        Cancelled,
        // 3XX and 4XX - file not found, not accessible, moved, etc.
        ClientError,
        // 5XX errors - server errors, further requests are likely to fail.
        // Also covers weird error codes like 1XX and 2XX which we shouldn't see.
        ServerError,
        // Error returned by the download library or by the write callback
        SystemError
    };

private:
    Status status;
    unsigned int code;
    std::string text;

    HttpStatus(Status status, unsigned int code = 0, std::string text = "");

public:
    THCRAP_UPDATE_API ~HttpStatus();
    // Keep the copy constructors etc

    static THCRAP_UPDATE_API HttpStatus makeOk();
    static THCRAP_UPDATE_API HttpStatus makeCancelled();
    static THCRAP_UPDATE_API HttpStatus makeNetworkError(unsigned int httpCode);
    static THCRAP_UPDATE_API HttpStatus makeSystemError(unsigned int systemCode, std::string text);

    Status get() const;
    operator bool() const;
    bool operator==(Status status) const;
    bool operator!=(Status status) const;

    std::string toString() const;
};
