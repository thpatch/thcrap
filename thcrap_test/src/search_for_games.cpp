#include "thcrap.h"
#include <filesystem>
#include <fstream>
#include "gtest/gtest.h"

class SearchForGamesTest : public ::testing::Test
{
protected:
    const char *abcSha   = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    const char *defSha   = "cb8379ac2098aa165029e3938a51da0bcecfc008fd6795f401178647f96c5b34";

    json_t *versions;
    json_t *hashes;
    json_t *sizes;

    const wchar_t *dirlist[2] = {
        L"testdir",
        nullptr,
    };

    void SetUp() override
    {
        if (std::filesystem::exists("testdir")) {
            std::filesystem::remove_all("testdir");
        }
        std::filesystem::create_directory("testdir");

        auto abc = std::ofstream("testdir/abc.exe");
        abc.write("abc", 3);

        auto def = std::ofstream("testdir/def.exe");
        def.write("def", 3);


        ScopedJson patch_js = json_pack("{s:s}",
            "id", "base_tsa"
        );
        json_dump_file(*patch_js, "testdir/patch.js", 0);

        patch_t patch = patch_init("testdir", nullptr, 0);
        stack_add_patch(&patch);

        versions = json_object();
        hashes = json_object();
        sizes = json_object();
        json_object_set(versions, "hashes", hashes);
        json_object_set(versions, "sizes",  sizes);
    }

    void TearDown() override
    {
        stack_free();
        std::filesystem::remove_all("testdir");
    }

    void add_version(const char *hash, unsigned int size, const char *id, const char *build, const char *variant, unsigned int codepage = 0)
    {
        json_t *version = json_pack("[s, s, s]", id, build, variant);
        if (codepage) {
            json_array_append_new(version, json_integer(codepage));
        }

        std::string size_s = std::to_string(size);
        json_object_set(hashes, hash, version);
        json_object_set(sizes, size_s.c_str(), version);
        json_decref(version);

        json_dump_file(versions, "testdir/versions.js", JSON_INDENT(2));
    }
};

TEST_F(SearchForGamesTest, TestSearchNormalFile)
{
    add_version(abcSha, 3, "th_abc", "v1.00", "(original)");

    game_search_result *found = SearchForGames(dirlist, nullptr);
    ASSERT_NE(found, nullptr);
    EXPECT_STREQ(found[0].path, "testdir/abc.exe");
    EXPECT_STREQ(found[0].id, "th_abc");
    EXPECT_STREQ(found[0].build, "v1.00");
    EXPECT_STREQ(found[0].description, "v1.00 (original)");
    EXPECT_EQ(found[1].id, nullptr);
    SearchForGames_free(found);
}

TEST_F(SearchForGamesTest, TestSearch2games)
{
    add_version(abcSha, 3, "th_abc", "v1.00", "(original)");
    add_version(defSha, 3, "th_def", "v1.00", "(original)");

    game_search_result *found = SearchForGames(dirlist, nullptr);
    ASSERT_NE(found, nullptr);
    EXPECT_STREQ(found[0].id, "th_abc");
    EXPECT_STREQ(found[1].id, "th_def");
    EXPECT_EQ(found[2].id, nullptr);
    SearchForGames_free(found);
}

TEST_F(SearchForGamesTest, TestSearch2versions)
{
    add_version(abcSha, 3, "th_abc", "v1.00", "(original)");
    add_version(defSha, 3, "th_abc", "v1.01", "(original)");

    game_search_result *found = SearchForGames(dirlist, nullptr);
    ASSERT_NE(found, nullptr);
    EXPECT_STREQ(found[0].build, "v1.01");
    EXPECT_STREQ(found[1].build, "v1.00");
    EXPECT_EQ(found[2].id, nullptr);
    SearchForGames_free(found);
}

TEST_F(SearchForGamesTest, TestSearchNoResults)
{
    game_search_result *found = SearchForGames(dirlist, nullptr);
    EXPECT_EQ(found, nullptr);
}

TEST_F(SearchForGamesTest, TestSearchWithInputGames)
{
    add_version(abcSha, 3, "th_abc", "v1.00", "(original)");
    add_version(defSha, 3, "th_def", "v1.00", "(original)");

    games_js_entry games_in[2] = {
        { "th_abc", "randompath.exe" },
        { nullptr, nullptr},
    };
    game_search_result *found = SearchForGames(dirlist, games_in);
    ASSERT_NE(found, nullptr);
    EXPECT_STREQ(found[0].id, "th_def");
    EXPECT_EQ(found[1].id, nullptr);
    SearchForGames_free(found);
}
