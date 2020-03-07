#include "netcode.h"

// Structure for thread-local download data
struct FileDownloading
{
    File& file;
    std::vector<uint8_t> local_data;
};

HttpHandle::HttpHandle()
    : curl(curl_easy_init())
{}

HttpHandle::HttpHandle(HttpHandle&& other)
    : curl(other.curl)
{
    other.curl = nullptr;
}

HttpHandle::~HttpHandle()
{
    if (this->curl) {
        curl_easy_cleanup(this->curl);
    }
}

CURL *HttpHandle::operator*()
{
    return this->curl;
}

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

unsigned int File::getThreadsLeft() const
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

size_t File::writeCallback(FileDownloading& fileDownloading, uint8_t *data, size_t size)
{
    if (this->status == FileStatus::DONE) {
        // Any value different from size signals an error to libcurl
        return 0;
    }
    fileDownloading.local_data.insert(fileDownloading.local_data.end(), data, data + size);
    return size;
}

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

void File::setFailed()
{
    // We can't be in todo because we alreadu started a download.
    // If we're already in failed, we don't need to change the status.
    // If we're in done, we don't *want* to change the status.
    // So we only need to change from downloading to failed.
    FileStatus downloading = FileStatus::DOWNLOADING;
    this->status.compare_exchange_strong(downloading, FileStatus::FAILED);
}

size_t File::writeCallbackStatic(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    auto fileDownloading = static_cast<FileDownloading*>(userdata);
    return fileDownloading->file.writeCallback(*fileDownloading, reinterpret_cast<uint8_t*>(ptr), size * nmemb);
};

bool File::download(HttpHandle& http, const std::string& url)
{
    if (!this->setDownloading()) {
        return true;
    }
    printf("Starting %s...\n", url.c_str());

    FileDownloading fileDownloading = {
        .file = *this,
        .local_data = std::vector<uint8_t>(),
    };

    curl_easy_setopt(*http, CURLOPT_WRITEFUNCTION, File::writeCallbackStatic);
    curl_easy_setopt(*http, CURLOPT_WRITEDATA, &fileDownloading);

    char errbuf[CURL_ERROR_SIZE];
    curl_easy_setopt(*http, CURLOPT_ERRORBUFFER, errbuf);
    errbuf[0] = 0;
    curl_easy_setopt(*http, CURLOPT_URL, url.c_str());

    CURLcode res = curl_easy_perform(*http);

    curl_easy_setopt(*http, CURLOPT_WRITEFUNCTION, nullptr);
    curl_easy_setopt(*http, CURLOPT_WRITEDATA, nullptr);

    std::string error;
    if (res != CURLE_OK) {
        if (errbuf[0]) {
            printf("%s: %s\n", url.c_str(), errbuf);
        }
        else {
            printf("%s: %s\n", url.c_str(), curl_easy_strerror(res));
        }
        curl_easy_setopt(*http, CURLOPT_ERRORBUFFER, nullptr);
        this->setFailed();
        return false;
    }
    curl_easy_setopt(*http, CURLOPT_ERRORBUFFER, nullptr);

    long response_code = 0;
    curl_easy_getinfo(*http, CURLINFO_RESPONSE_CODE, &response_code);
    if (response_code != 200) {
        printf("%s: HTTP error code %ld\n", url.c_str(), response_code);
        this->setFailed();
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(this->mutex);
        if (this->status == FileStatus::DOWNLOADING || // We're the first thread to finish downloading the file
            this->status == FileStatus::FAILED) {      // Another thread failed to download the file, but this thread succeeded
            this->data = std::move(fileDownloading.local_data);
            this->status = FileStatus::DONE;
        }
        // else, do nothing. Another thread finished before us.
    }
    return true;
}

DownloadUrl File::pickUrl()
{
    std::scoped_lock<std::mutex> lock(this->mutex);
    // TODO: srand (actually we don't really care about having a good random source)
    int n = rand() % this->urls.size();
    auto it = this->urls.begin();
    std::advance(it, n);
    DownloadUrl url = *it;
    this->urls.erase(it);
    return url;
}

void File::download()
{
    DownloadUrl url = this->pickUrl();
    BorrowedHttpHandle handle = url.server.borrowHandle();
    if (!this->download(*handle, url.server.getUrl() + url.url)) {
        url.server.fail();
    }
}
