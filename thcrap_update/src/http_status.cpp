#include "thcrap.h"
#include <map>
#include <sstream>
#include "http_status.h"

HttpStatus::HttpStatus(Status status, unsigned int code, std::string text)
    : status(status), code(code), text(text)
{}

HttpStatus HttpStatus::makeOk()
{
    return HttpStatus(Ok, 0, "success");
}

HttpStatus HttpStatus::makeCancelled()
{
    return HttpStatus(Cancelled, 0, "cancelled");
}

HttpStatus HttpStatus::makeNetworkError(unsigned int httpCode)
{
    HttpStatus::Status status;
    if (300 <= httpCode && httpCode < 499) {
        status = ClientError;
    }
    else {
        status = ServerError;
    }

    std::map<unsigned int, const char*> messages = {
        { 301, "URL moved permanently" },
        { 302, "URL moved temporarily" },
        { 304, "HTTP cache error" }, // If this wasn't an error, the HTTP library would have handled it
        { 307, "URL moved temporarily" },
        { 308, "URL moved permanently" },
        { 400, "bad request" },
        { 401, "requires authentication" },
        { 403, "forbidden" },
        { 404, "not found" },
        { 409, "request timeout" },
        { 429, "too many requests" },
        { 500, "internal server error" },
        // As a user, I used to be really confused by these errors. I didn't know what a "gateway" or CDN is.
        // If a server managed to send me an HTML page, the server must be available, right?
        // So let's reword them, with hope that these will be more clear.
        { 502, "server error (bad gateway)" },
        { 503, "server unavailable" },
        { 504, "server timed out" },
        // All other codes are either illegal or uncommon, returning a description
        // wouldn't help the user, and returning the error code number will be enough
        // for more advanced users.
    };
    std::string msg;
    auto it = messages.find(httpCode);
    if (it != messages.end()) {
        msg = it->second;
    }
    else {
        msg = "unknown HTTP error";
    }

    return HttpStatus(status, httpCode, msg);
}

HttpStatus HttpStatus::makeSystemError(unsigned int systemCode, std::string text)
{
    return HttpStatus(SystemError, systemCode, text);
}

HttpStatus::Status HttpStatus::get() const
{
    return this->status;
}

HttpStatus::operator bool() const
{
    return this->status == Ok;
}

bool HttpStatus::operator==(Status status) const
{
    return this->status == status;
}

bool HttpStatus::operator!=(Status status) const
{
    return this->status != status;
}

std::string HttpStatus::toString() const
{
    if (this->code == 0) {
        return this->text;
    }

    std::ostringstream ss;
    ss << this->text << " (code " << this->code << ")";
    return ss.str();
}
