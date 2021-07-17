#include "thcrap.h"
#include <stdexcept>
#include <fstream>
#include "random.h"
#include "server.h"
#include "file.h"

using namespace std::string_literals;

File::File(std::list<DownloadUrl>&& urls,
           success_t successCallback,
           failure_t failureCallback,
           progress_t progressCallback)
    : status(Status::Todo), urls(urls),
    userSuccessCallback(successCallback), userFailureCallback(failureCallback), userProgressCallback(progressCallback)
{
    if (urls.empty()) {
        throw new std::invalid_argument("Input URL list must not be empty");
    }
}

size_t File::writeCallback(std::vector<uint8_t>& buffer, const uint8_t *data, size_t size)
{
    buffer.insert(buffer.end(), data, data + size);
    return size;
}

bool File::progressCallback(const DownloadUrl& url, size_t dlnow, size_t dltotal)
{
    if (!url.getServer().isAlive()) {
        // Don't waste time if the server is dead
        return false;
    }
    return userProgressCallback(url, dlnow, dltotal);
}

void File::download(IHttpHandle& http, const DownloadUrl& url)
{
    if (this->status == Status::Done) {
        throw std::logic_error("Downloading an already downloaded file "s + url.getUrl());
    }
    this->status = Status::Downloading;

    if (userProgressCallback(url, 0, 0) == false) {
        // User-requested cancel
        userFailureCallback(url, HttpStatus::makeCancelled());
        return ;
    }
    // Fail early if the server is dead
    if (!url.getServer().isAlive()) {
        userFailureCallback(url, HttpStatus::makeCancelled());
        return ;
    }

    std::vector<uint8_t> out;
    HttpStatus status = http.download(url.getUrl(),
        [this, &out](const uint8_t *in, size_t size) {
            return this->writeCallback(out, in, size);
        },
        [this, &url](size_t dlnow, size_t dltotal) {
            return this->progressCallback(url, dlnow, dltotal);
        }
    );
    if (!status) {
        if (status.get() == HttpStatus::ServerError || status.get() == HttpStatus::SystemError) {
            // If the server is dead, we don't want to continue using it.
            // If the library returned an error, future downloads to the
            // same server are also likely to fail.
            // If it's only a 404, other downloads might work.
            url.getServer().fail();
        }
        userFailureCallback(url, status);
        return ;
    }

    this->status = Status::Done;
    userSuccessCallback(url, out);
}

DownloadUrl File::pickUrl()
{
    int n = Random::get() % this->urls.size();
    auto it = this->urls.begin();
    std::advance(it, n);
    DownloadUrl url = *it;
    this->urls.erase(it);
    return url;
}

void File::download()
{
    do {
        DownloadUrl url = this->pickUrl();
        BorrowedHttpHandle handle = url.getServer().borrowHandle();
        this->download(*handle, url);
    } while (this->status != Status::Done && this->urls.size() > 0);
}

int download_single_file(const char* url, const char* fn) {
    auto res = ServerCache::get().downloadFile(url);
    if (res.second == HttpStatus::makeOk()) {
        auto file = res.first;
        file_write(fn, file.data(), file.size());
        return 0;
    }
    else {
        return res.second.get();
    }
}
