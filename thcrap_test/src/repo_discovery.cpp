#include "thcrap.h"
#include <filesystem>
#include "gtest/gtest.h"
#include "repo_discovery.h"
#include "server.h"

class FakeHttpHandle : public IHttpHandle
{
private:
    static std::map<std::string, ScopedJson> files;

public:
    static void addFile(std::string url, ScopedJson file)
    {
        files[url] = file;
    }

    static void addFiles(std::map<std::string, ScopedJson> files)
    {
        for (auto& it : files) {
            addFile(it.first, it.second);
        }
    }

    static void clear()
    {
        files.clear();
    }

    HttpStatus download(const std::string& url, std::function<size_t(const uint8_t*, size_t)> writeCallback, std::function<bool(size_t, size_t)>, DownloadCache*) override
    {
        auto it = files.find(url);
        if (it == files.end()) {
            return HttpStatus::makeNetworkError(404);
        }

        json_dump_callback(*it->second, [](const char *buffer, size_t size, void *data) -> int {
            auto writeCallback = reinterpret_cast<std::function<size_t(const uint8_t*, size_t)>*>(data);
            size_t written = (*writeCallback)(reinterpret_cast<const uint8_t*>(buffer), size);
            return written == size ? 0 : -1;
        }, &writeCallback, 0);
        return HttpStatus::makeOk();
    }
};
std::map<std::string, ScopedJson> FakeHttpHandle::files;

class RepoDiscoveryTest : public ::testing::Test
{
protected:
    repo_t **repos;

    void SetUp() override
    {
        ServerCache::get().setHttpHandleFactory([](){ return std::make_unique<FakeHttpHandle>(); });
        // Should do nothing, just in case someone forgot to do that in a previous test
        ServerCache::get().clear();
        FakeHttpHandle::clear();
        repos = nullptr;

        if (std::filesystem::is_directory("repos")) {
            std::filesystem::remove_all("repos");
        }
    }

    void TearDown() override
    {
        ServerCache::get().setHttpHandleFactory(ServerCache::defaultHttpHandleFactory);
        ServerCache::get().clear();
        FakeHttpHandle::clear();

        if (repos) {
            for (size_t i = 0; repos[i]; i++) {
                RepoFree(repos[i]);
            }
            free(repos);
        }
    }
};

TEST_F(RepoDiscoveryTest, BasicTest)
{
    FakeHttpHandle::addFile("https://srv.thpatch.net/repo.js", json_pack("{s:s}", "id", "thpatch"));

    repos = RepoDiscover("https://srv.thpatch.net/");
    ASSERT_NE(repos, nullptr);
    ASSERT_NE(repos[0], nullptr);
    EXPECT_EQ(repos[1], nullptr);
    EXPECT_STREQ(repos[0]->id, "thpatch");
}

TEST_F(RepoDiscoveryTest, TestNeighbors)
{
    FakeHttpHandle::addFiles({
        { "https://srv.thpatch.net/repo.js", json_pack("{s:s,s:[s,s],s:[s]}",
            "id", "thpatch",
            // A normal dependency, and an invalid one
            "neighbors", "https://mirrors.thpatch.net/nmlgc/", "https://mirrors.thpatch.net/404/",
            "servers", "https://srv.thpatch.net/"
        )},
        { "https://mirrors.thpatch.net/nmlgc/repo.js", json_pack("{s:s,s:[s],s:[s]}",
            "id", "nmlgc",
            // Testing circular neighbors
            "neighbors", "https://srv.thpatch.net",
            "servers", "https://mirrors.thpatch.net/nmlgc/"
        )},
    });

    repos = RepoDiscover("https://srv.thpatch.net/");
    ASSERT_NE(repos, nullptr);
    ASSERT_NE(repos[0], nullptr);
    ASSERT_NE(repos[1], nullptr);
    EXPECT_EQ(repos[2], nullptr);
    // They should be returned in sorted order.
    // No need to parse every field, we're just testing the discovery.
    EXPECT_STREQ(repos[0]->id, "nmlgc");
    EXPECT_STREQ(repos[1]->id, "thpatch");
}
