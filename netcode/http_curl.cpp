#include "thcrap.h"
#include "http_curl.h"

CurlHandle::CurlHandle()
    : curl(curl_easy_init())
{}

CurlHandle::CurlHandle(CurlHandle&& other)
    : curl(other.curl)
{
    other.curl = nullptr;
}

CurlHandle::~CurlHandle()
{
    if (this->curl) {
        curl_easy_cleanup(this->curl);
    }
}

size_t CurlHandle::writeCallbackStatic(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    auto callback = *static_cast<std::function<size_t(const uint8_t*, size_t)>*>(userdata);
    return callback(reinterpret_cast<const uint8_t*>(ptr), size * nmemb);
};

int CurlHandle::progressCallbackStatic(void *userdata, curl_off_t dltotal, curl_off_t dlnow, curl_off_t /* ultotal */, curl_off_t /* ulnow */)
{
    auto callback = *static_cast<std::function<bool(size_t, size_t)>*>(userdata);
    if (callback(dlnow, dltotal)) {
        return 0;
    }
    else {
        return -1;
    }
};

IHttpHandle::Status CurlHandle::download(const std::string& url, std::function<size_t(const uint8_t*, size_t)> writeCallback, std::function<bool(size_t, size_t)> progressCallback)
{
    curl_easy_setopt(this->curl, CURLOPT_FOLLOWLOCATION, 1);

    curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, CurlHandle::writeCallbackStatic);
    curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, &writeCallback);

    curl_easy_setopt(this->curl, CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt(this->curl, CURLOPT_XFERINFOFUNCTION, CurlHandle::progressCallbackStatic);
    curl_easy_setopt(this->curl, CURLOPT_XFERINFODATA, &progressCallback);

    char errbuf[CURL_ERROR_SIZE];
    curl_easy_setopt(this->curl, CURLOPT_ERRORBUFFER, errbuf);
    errbuf[0] = 0;
    curl_easy_setopt(this->curl, CURLOPT_URL, url.c_str());

    CURLcode res = curl_easy_perform(this->curl);

    curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, nullptr);
    curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, nullptr);
    curl_easy_setopt(this->curl, CURLOPT_XFERINFOFUNCTION, nullptr);
    curl_easy_setopt(this->curl, CURLOPT_XFERINFODATA, nullptr);

    std::string error;
    if (res != CURLE_OK) {
        if (errbuf[0]) {
            log_printf("%s: %s\n", url.c_str(), errbuf);
        }
        else {
            log_printf("%s: %s\n", url.c_str(), curl_easy_strerror(res));
        }
        curl_easy_setopt(this->curl, CURLOPT_ERRORBUFFER, nullptr);
        if (res == CURLE_ABORTED_BY_CALLBACK) {
            return Status::Cancelled;
        }
        else {
            return Status::Error;
        }
    }
    curl_easy_setopt(this->curl, CURLOPT_ERRORBUFFER, nullptr);

    long response_code = 0;
    curl_easy_getinfo(this->curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code != 200) {
        log_printf("%s: HTTP error code %ld\n", url.c_str(), response_code);
        if (300 <= response_code && response_code <= 499) {
            return Status::ClientError;
        }
        else {
            return Status::ServerError;
        }
    }

    return Status::Ok;
}
