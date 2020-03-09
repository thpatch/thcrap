#pragma once

#include <list>
#include <string>
#include <vector>
#include "ThreadPool.h"
#include "file.h"

class Downloader
{
private:
    ThreadPool pool;
    std::list<File> files;
    std::vector<std::future<void>> futuresList;

    void addToQueue(File& file);

public:
    Downloader();
    ~Downloader();
    const File* addFile(std::list<std::string> servers, std::string filename);
    void wait();
};
