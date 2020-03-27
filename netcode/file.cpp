#include <fstream>
#include "random.h"
#include "server.h"
#include "file.h"

File::File(std::list<DownloadUrl>&& urls)
    : status(FileStatus::TODO), urls(urls), threadsLeft(urls.size())
{}

const std::vector<uint8_t>& File::getData() const
{
    return this->data;
}

FileStatus File::getStatus() const
{
    return this->status;
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

bool File::progressCallback(const DownloadUrl& url, size_t /*dlnow*/, size_t /*dltotal*/)
{
    if (this->status == FileStatus::DONE || // If another thread already finished to download this file, we don't need to continue.
        !url.getServer().isAlive()) { // Don't waste time if the server is dead
        return false;
    }
    // TODO: call a user-written callback
    return true;
}

// Set the File as downloading. Fails if the file is already downloaded
// (telling the caller to not bother downloading it again).
bool File::setDownloading()
{
    FileStatus todo = FileStatus::TODO;
    if (this->status.compare_exchange_strong(todo, FileStatus::DOWNLOADING)) {
        return true;
    }
    else if (this->status == FileStatus::DOWNLOADING) {
        return true;
    }
    else {
        return false;
    }
}

// Set the File as failed. Do nothing if another thread succeeded.
void File::setFailed(const DownloadUrl& url)
{
    // We can't be in todo because we alreadu started a download.
    // If we're already in failed, we don't need to change the status.
    // If we're in done, we don't *want* to change the status.
    // So we only need to change from downloading to failed.
    FileStatus downloading = FileStatus::DOWNLOADING;
    this->status.compare_exchange_strong(downloading, FileStatus::FAILED);

    url.getServer().fail();
}

bool File::download(HttpHandle& http, const DownloadUrl& url)
{
    bool ret = false;

    if (!this->setDownloading()) {
        return false;
    }
    // Fail early if the server is dead
    if (!url.getServer().isAlive()) {
        printf("%s: server is dead\n", url.getUrl().c_str());
        this->setFailed(url);
        return false;
    }
    printf("Starting %s...\n", url.getUrl().c_str());

    std::vector<uint8_t> localData;
    if (!http.download(url.getUrl(),
        [this, &localData](const uint8_t *data, size_t size) {
            return this->writeCallback(localData, data, size);
        },
        [this, &url](size_t dlnow, size_t dltotal) {
            return this->progressCallback(url, dlnow, dltotal);
        }
    )) {
        // TODO: do not mark the server as failed if the download was cancelled
        this->setFailed(url);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(this->mutex);
        if (this->status == FileStatus::DOWNLOADING || // We're the first thread to finish downloading the file
            this->status == FileStatus::FAILED) {      // Another thread failed to download the file, but this thread succeeded
            this->data = std::move(localData);
            this->status = FileStatus::DONE;
            ret = true;
        }
        // else, do nothing. Another thread finished before us.
    }
    return ret;
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

bool File::download()
{
    DownloadUrl url = this->pickUrl();
    BorrowedHttpHandle handle = url.getServer().borrowHandle();
    return this->download(*handle, url);
}

void File::write(const std::filesystem::path& path) const
{
    if (this->status != FileStatus::DONE) {
        throw std::logic_error("Calling File::write but the file isn't downloaded.");
    }

    std::filesystem::path dir = std::filesystem::path(path).remove_filename();
    std::filesystem::create_directories(dir);

    std::ofstream f;
    f.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    f.open(path, std::ofstream::binary);
    f.write(reinterpret_cast<const char*>(this->data.data()), this->data.size());
}
