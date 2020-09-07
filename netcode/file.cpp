#include "thcrap.h"
#include <stdexcept>
#include <fstream>
#include "random.h"
#include "server.h"
#include "file.h"

File::File(std::list<DownloadUrl>&& urls,
           success_t successCallback,
           failure_t failureCallback,
           progress_t progressCallback)
    : status(Status::Todo), urls(urls), threadsLeft(urls.size()),
    userSuccessCallback(successCallback), userFailureCallback(failureCallback), userProgressCallback(progressCallback)
{
    if (urls.empty()) {
        throw new std::invalid_argument("Input URL list must not be empty");
    }
}

size_t File::getThreadsLeft() const
{
    return this->threadsLeft;
}

void File::decrementThreadsLeft()
{
    if (this->threadsLeft == 0) {
        throw std::logic_error("Trying to decrement File::threadsLeft, but it is already 0");
    }
    this->threadsLeft--;
}

size_t File::writeCallback(std::vector<uint8_t>& buffer, const uint8_t *data, size_t size)
{
    buffer.insert(buffer.end(), data, data + size);
    return size;
}

bool File::progressCallback(const DownloadUrl& url, size_t dlnow, size_t dltotal)
{
    if (this->status == Status::Done || // If another thread already finished to download this file, we don't need to continue.
        !url.getServer().isAlive()) { // Don't waste time if the server is dead
        return false;
    }
    return userProgressCallback(url, dlnow, dltotal);
}

void File::download(IHttpHandle& http, const DownloadUrl& url)
{
    if (this->status == Status::Done) {
        return ;
    }
    // If the status was Todo, we need to change it.
    // If the status was Downloading, we don't care whether we change it or not.
    // If the status changed to Done since the if above, we must not change it.
    File::Status status_todo = File::Status::Todo;
    this->status.compare_exchange_strong(status_todo, Status::Downloading);

    if (userProgressCallback(url, 0, 0) == false) {
        return ;
    }
    // Fail early if the server is dead
    if (!url.getServer().isAlive()) {
        userFailureCallback(url, IHttpHandle::Status::Cancelled);
        return ;
    }

    std::vector<uint8_t> localData;
    IHttpHandle::Status status = http.download(url.getUrl(),
        [this, &localData](const uint8_t *data, size_t size) {
            return this->writeCallback(localData, data, size);
        },
        [this, &url](size_t dlnow, size_t dltotal) {
            return this->progressCallback(url, dlnow, dltotal);
        }
    );
    if (status != IHttpHandle::Status::Ok) {
        if (status == IHttpHandle::Status::ServerError || status == IHttpHandle::Status::Error) {
            // If the server is dead, we don't want to continue using it.
            // If the library returned an error, future downloads to the
            // same server are also likely to fail.
            // If it's only a 404, other downloads might work.
            url.getServer().fail();
        }
        userFailureCallback(url, status);
        return ;
    }

    {
        File::Status status_downloading = File::Status::Downloading;
        if (this->status.compare_exchange_strong(status_downloading, Status::Done)) {
            // We changed the status from Downloading to Done,
            // so we're the first thread to finish downloading the file
            userSuccessCallback(url, localData);
        }
        // else, do nothing. Another thread finished before us.
    }
}

DownloadUrl File::pickUrl()
{
    std::scoped_lock<std::mutex> lock(this->mutex);
    int n = Random::get() % this->urls.size();
    auto it = this->urls.begin();
    std::advance(it, n);
    DownloadUrl url = *it;
    this->urls.erase(it);
    return url;
}

void File::download()
{
    DownloadUrl url = this->pickUrl();
    BorrowedHttpHandle handle = url.getServer().borrowHandle();
    this->download(*handle, url);
}
