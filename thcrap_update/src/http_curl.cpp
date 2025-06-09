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
    if (callback((size_t)dlnow, (size_t)dltotal)) {
        return 0;
    }
    else {
        return -1;
    }
};

static char digit_to_hex(unsigned int digit)
{
    if (digit <= 9) {
        return '0' + digit;
    }
    else if (digit <= 'F') {
        return 'A' + (digit - 10);
    }
    else {
        return '?';
    }
}

static std::string encode_UTF8_char(uint8_t c)
{
    if (c & 0x80) {
        std::string ret = "%";
        ret += digit_to_hex(c >> 4);
        ret += digit_to_hex(c & 0x0F);
        return ret;
    }
    else {
        return std::string() + (char)c;
    }
}

static std::string encode_UTF8_url(std::string in)
{
    std::string out;
    for (auto c : in) {
        out += encode_UTF8_char((uint8_t)c);
    }
    return out;
}

static curl_slist *setup_headers(DownloadCache *cache)
{
	if (!cache) {
		return nullptr;
	}
	curl_slist *headers = nullptr;

	std::optional<std::string> etag = cache->getEtag();
	if (etag) {
		BUILD_VLA_STR(char, header, "If-None-Match: ", etag->c_str());
		headers = curl_slist_append(headers, header);
		VLA_FREE(header);
	}

	std::optional<std::string> lastModifiedDate = cache->getLastModifiedDate();
	if (lastModifiedDate) {
		BUILD_VLA_STR(char, header, "If-Modified-Since: ", lastModifiedDate->c_str());
		headers = curl_slist_append(headers, header);
		VLA_FREE(header);
	}

	return headers;
}

void update_cache(CURL *curl, DownloadCache *cache)
{
	if (!cache) {
		return ;
	}

	// The CURL doc says that the header struct is clobbered between
	// subsequent calls, the strings inside it might also be clobbered
	// (I don't think they are, but the doc doesn't guarantee it).
	// Better play it safe and make a copy of the 1st header.
	std::string etag;
	const char *lastModifiedDate = nullptr;

	curl_header *header;
	if (curl_easy_header(curl, "ETag", 0, CURLH_HEADER, -1, &header) == CURLHE_OK) {
		etag = header->value;
	}

	if (curl_easy_header(curl, "Last-Modified", 0, CURLH_HEADER, -1, &header) == CURLHE_OK) {
		lastModifiedDate = header->value;
		if (lastModifiedDate && !lastModifiedDate[0]) {
			lastModifiedDate = nullptr;
		}
	}

	cache->setCacheData(!etag.empty() ? etag.c_str() : nullptr, lastModifiedDate);
}

HttpStatus CurlHandle::download(const std::string& url, std::function<size_t(const uint8_t*, size_t)> writeCallback, std::function<bool(size_t, size_t)> progressCallback, DownloadCache *cache)
{
	curl_easy_setopt(this->curl, CURLOPT_CAINFO, "bin/cacert.pem");
    curl_easy_setopt(this->curl, CURLOPT_FOLLOWLOCATION, 1);

    // Format according to RFC 7231, section 5.5.3
    std::string userAgent = std::string(PROJECT_NAME_SHORT) + "/" + PROJECT_VERSION_STRING + " (" + windows_version() + ")";
    curl_easy_setopt(this->curl, CURLOPT_USERAGENT, userAgent.c_str());

    curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, CurlHandle::writeCallbackStatic);
    curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, &writeCallback);

    curl_easy_setopt(this->curl, CURLOPT_NOPROGRESS, 0);
    curl_easy_setopt(this->curl, CURLOPT_XFERINFOFUNCTION, CurlHandle::progressCallbackStatic);
    curl_easy_setopt(this->curl, CURLOPT_XFERINFODATA, &progressCallback);

    char errbuf[CURL_ERROR_SIZE];
    curl_easy_setopt(this->curl, CURLOPT_ERRORBUFFER, errbuf);
    errbuf[0] = 0;

	curl_slist *headers = setup_headers(cache);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    std::string url_encoded = encode_UTF8_url(url);
    curl_easy_setopt(this->curl, CURLOPT_URL, url_encoded.c_str());

    CURLcode res = curl_easy_perform(this->curl);

    curl_easy_setopt(this->curl, CURLOPT_WRITEFUNCTION, nullptr);
    curl_easy_setopt(this->curl, CURLOPT_WRITEDATA, nullptr);
    curl_easy_setopt(this->curl, CURLOPT_XFERINFOFUNCTION, nullptr);
    curl_easy_setopt(this->curl, CURLOPT_XFERINFODATA, nullptr);

    std::string error;
    if (res != CURLE_OK) {
        if (errbuf[0]) {
            error = errbuf;
        }
        else {
            error = curl_easy_strerror(res);
        }
        curl_easy_setopt(this->curl, CURLOPT_ERRORBUFFER, nullptr);
        if (res == CURLE_ABORTED_BY_CALLBACK) {
            return HttpStatus::makeCancelled();
        }
        else {
            return HttpStatus::makeSystemError(res, error);
        }
    }
    curl_easy_setopt(this->curl, CURLOPT_ERRORBUFFER, nullptr);
    if (headers) {
        curl_easy_setopt(this->curl, CURLOPT_HTTPHEADER, nullptr);
        curl_slist_free_all(headers);
    }

    long response_code = 0;
    curl_easy_getinfo(this->curl, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code == 200) {
        update_cache(this->curl, cache);
        return HttpStatus::makeOk();
    }
    else if (response_code == 304) {
        if (!cache) {
            throw std::logic_error("Got 304 for a non-cached object");
        }
        std::vector<uint8_t> data = cache->getCachedFile();
        progressCallback(data.size(), data.size());
        writeCallback(data.data(), data.size());
        update_cache(this->curl, cache);
        return HttpStatus::makeOk();
    }
    else {
        return HttpStatus::makeNetworkError(response_code);
    }
}
