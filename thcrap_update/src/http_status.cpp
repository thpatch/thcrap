#include "thcrap.h"
#include <map>
#include <sstream>
#include "http_status.h"

HttpStatus::HttpStatus(Status status, unsigned int code, const std::string& text)
    : status(status), code(code), text(text)
{}

HttpStatus::~HttpStatus()
{}

HttpStatus HttpStatus::makeOk()
{
    return HttpStatus(Ok, 0, "success"s);
}

HttpStatus HttpStatus::makeCancelled()
{
    return HttpStatus(Cancelled, 0, "cancelled"s);
}

static const std::map<unsigned int, const std::string_view> messages = {
    { 301, "URL moved permanently"sv },
    { 302, "URL moved temporarily"sv },
    { 304, "HTTP cache error"sv }, // If this wasn't an error, the HTTP library would have handled it
    { 307, "URL moved temporarily"sv },
    { 308, "URL moved permanently"sv },
    { 400, "bad request"sv },
    { 401, "requires authentication"sv },
    { 403, "forbidden"sv },
    { 404, "not found"sv },
    { 409, "request timeout"sv },
    { 429, "too many requests"sv },
    { 500, "internal server error"sv },
    // As a user, I used to be really confused by these errors. I didn't know what a "gateway" or CDN is.
    // If a server managed to send me an HTML page, the server must be available, right?
    // So let's reword them, with hope that these will be more clear.
    { 502, "server error (bad gateway)"sv },
    { 503, "server unavailable"sv },
    { 504, "server timed out"sv },
    // All other codes are either illegal or uncommon, returning a description
    // wouldn't help the user, and returning the error code number will be enough
    // for more advanced users.
};

HttpStatus HttpStatus::makeNetworkError(unsigned int httpCode)
{
    HttpStatus::Status status;
    if (300 <= httpCode && httpCode < 499) {
        status = ClientError;
    }
    else {
        status = ServerError;
    }


    auto it = messages.find(httpCode);
    std::string msg(it != messages.end() ? it->second : "unknown HTTP error"sv);
    return HttpStatus(status, httpCode, msg);
}

HttpStatus HttpStatus::makeSystemError(unsigned int systemCode, const std::string& text)
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

    return this->text + " (code " + std::to_string(this->code) + ')';
}
