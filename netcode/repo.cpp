#include <algorithm>
#include "netcode.h"

Repo::Repo()
{}

Repo::~Repo()
{
    for (Server& server : this->servers) {
        server.join();
    }
    for (File *file : this->todo) {
        delete file;
    }
    for (File *file : this->downloading) {
        delete file;
    }
    for (File *file : this->done) {
        delete file;
    }
}

void Repo::download()
{
    // Somewhere between 8 and 16 threads
    unsigned int nbThreads = 8 / this->servers.size() + 1;

    for (Server& server : this->servers) {
        server.startDownload(*this, nbThreads);
    }
    for (Server& server : this->servers) {
        server.join();
    }
}

File *Repo::takeFile()
{
    std::lock_guard<std::mutex> guard(this->mutex);
    if (this->todo.empty()) {
        return nullptr;
    }

    File *file = this->todo.front();
    this->todo.pop_front();
    this->downloading.push_back(file);
    return file;
}

void Repo::markFileAsDone(File *file)
{
    std::lock_guard<std::mutex> guard(this->mutex);
    auto fileInDownloading = std::find(this->downloading.begin(), this->downloading.end(), file);
    if (fileInDownloading != this->downloading.end()) {
        this->downloading.erase(fileInDownloading);
        this->done.push_back(file);
        return ;
    }

    if (std::find(this->done.begin(), this->done.end(), file) == this->done.end()) {
        throw std::runtime_error("marking unknown file as done");
    }

    return ;
}
