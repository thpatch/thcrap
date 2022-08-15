#include "thcrap.h"
#include "gtest/gtest.h"
#include <filesystem>

TEST(RepoTest, RepoGetLocalFN)
{
    char *fn = RepoGetLocalFN("repo_id");
    EXPECT_STREQ(fn, "repos/repo_id/repo.js");
    free(fn);
}

TEST(RepoTest, RepoLoadJsonNull)
{
    EXPECT_EQ(RepoLoadJson(nullptr), nullptr);
}

TEST(RepoTest, RepoLoadJsonInvalid)
{
    ScopedJson json = json_array();
    EXPECT_EQ(RepoLoadJson(*json), nullptr);
}

TEST(RepoTest, RepoLoadJsonEmpty)
{
    ScopedJson json = json_object();
    EXPECT_EQ(RepoLoadJson(*json), nullptr);
}

TEST(RepoTest, RepoLoadJsonValid)
{
    ScopedJson json = json_pack("{s:s, s:s, s:s, s:[s,s], s:[s,s], s:{s:s, s:s} }",
        "id", "test_id",
        "title", "Description of our test repo",
        "contact", "test@test.com",
        "servers", "https://www.example.com/", "https://www.example.com/anotherexample/",
        "neighbors", "https://www.example.com/neighbor1/", "https://www.example.com/neighbor2/",
        "patches", "patch1", "patch1 description", "patch2", "patch2 description"
    );
    repo_t *repo = RepoLoadJson(*json);
    ASSERT_NE(repo, nullptr);

    EXPECT_STREQ(repo->id, "test_id");
    EXPECT_STREQ(repo->title, "Description of our test repo");
    EXPECT_STREQ(repo->contact, "test@test.com");

    ASSERT_NE(repo->servers, nullptr);
    ASSERT_NE(repo->servers[0], nullptr);
    ASSERT_NE(repo->servers[1], nullptr);
    EXPECT_STREQ(repo->servers[0], "https://www.example.com/");
    EXPECT_STREQ(repo->servers[1], "https://www.example.com/anotherexample/");
    EXPECT_EQ(repo->servers[2], nullptr);

    ASSERT_NE(repo->neighbors, nullptr);
    ASSERT_NE(repo->neighbors[0], nullptr);
    ASSERT_NE(repo->neighbors[1], nullptr);
    EXPECT_STREQ(repo->neighbors[0], "https://www.example.com/neighbor1/");
    EXPECT_STREQ(repo->neighbors[1], "https://www.example.com/neighbor2/");
    EXPECT_EQ(repo->neighbors[2], nullptr);

    ASSERT_NE(repo->patches, nullptr);
    ASSERT_NE(repo->patches[0].patch_id, nullptr);
    ASSERT_NE(repo->patches[1].patch_id, nullptr);
    EXPECT_STREQ(repo->patches[0].patch_id, "patch1");
    EXPECT_STREQ(repo->patches[0].title, "patch1 description");
    EXPECT_STREQ(repo->patches[1].patch_id, "patch2");
    EXPECT_STREQ(repo->patches[1].title, "patch2 description");
    EXPECT_EQ(repo->patches[2].patch_id, nullptr);

    RepoFree(repo);
}

TEST(RepoTest, RepoLoad)
{
    std::filesystem::create_directory("repos");
    std::filesystem::create_directory("repos/repo1");
    std::filesystem::create_directory("repos/repo2");

    ScopedJson json1 = json_pack("{s:s}", "id", "repo1");
    ScopedJson json2 = json_pack("{s:s}", "id", "repo2");
    json_dump_file(*json1, "repos/repo1/repo.js", 0);
    json_dump_file(*json2, "repos/repo2/repo.js", 0);

    repo_t **repos = RepoLoad();
    ASSERT_NE(repos, nullptr);
    ASSERT_NE(repos[0], nullptr);
    ASSERT_NE(repos[1], nullptr);
    EXPECT_EQ(repos[2], nullptr);

    if (strcmp(repos[0]->id, "repo1") == 0) {
        EXPECT_STREQ(repos[1]->id, "repo2");
    }
    else if (strcmp(repos[1]->id, "repo1") == 0) {
        EXPECT_STREQ(repos[0]->id, "repo2");
    }
    else {
        ADD_FAILURE() << "repo1 not found";
    }

    for (size_t i = 0; repos[i]; i++) {
        RepoFree(repos[i]);
    }
    free(repos);
    std::filesystem::remove_all("repos");
}
