#include <functional>
#include <stdexcept>
#include "netcode.h"
#include <unistd.h>
#include <iostream>

Server::Server(std::string baseUrl)
    : baseUrl(std::move(baseUrl))
{
    if (this->baseUrl[this->baseUrl.length() - 1] != '/') {
        this->baseUrl.append("/");
    }
}

Server::Server(Server&& src)
    : baseUrl(std::move(src.baseUrl)),
      threads(std::move(src.threads))
{}

void Server::startDownload(Repo &repo, unsigned int nbThreads)
{
    if (this->threads.empty()) {
        throw std::logic_error("can't start a new download when a download is already running");
    }

    for (unsigned int i = 0; i < nbThreads; i++) {
        this->threads.emplace_back([this, &repo]() { this->downloadThread(repo); });
    }
}

void Server::join()
{
    for (std::thread& it : this->threads) {
        if (it.joinable()) {
            it.join();
        }
    }
    this->threads.clear();
}

void Server::downloadThread(Repo& repo)
{
    HttpHandle http;
    File *file;
    while ((file = repo.takeFile()) != nullptr) {
        if (file->download(http, this->baseUrl) == false) {
            // File::download will print more details on the failure
            printf("%s: download failed. Closing download thread.\n", this->baseUrl.c_str());
            return ;
        }
        repo.markFileAsDone(file);
    }
    sleep(1);
    // TODO: write that function (otherwise we may hang on a slow or dead server).
    // It must never give the same file twice to the same server.
    //while (this->repo.takeDownloadingFile(this, &file)) {
    //    this->download(file);
    //}
}

std::unique_ptr<File> Server::downloadFile(const std::string& name)
{
    HttpHandle http;
    auto file = std::make_unique<File>(name);
    file->download(http, this->baseUrl);
    return file;
}

json_t *Server::downloadJsonFile(const std::string& name)
{
    auto file = this->downloadFile(name);
    if (file->getStatus() == FileStatus::FAILED) {
        return nullptr;
    }
    return json_loadb(reinterpret_cast<const char*>(file->getData().data()), file->getData().size(), 0, nullptr);
}
