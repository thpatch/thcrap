#include "thcrap.h"
#include "gtest/gtest.h"
#include <fstream>
#include <filesystem>

// Also test file_stream and file_stream_read
TEST(PatchFile, FileRead)
{
    const char *fn = "FileRead_test.txt";
    const char content[] = "abcde";
    const size_t content_size = sizeof(content);
    void *buffer;
    size_t file_size;

    // Test with an invalid file. file_size must be set to 0 by file_read.
    file_size = 0x12345678;
    buffer = file_read("invalid_file.txt", &file_size);
    EXPECT_EQ(buffer, nullptr);
    EXPECT_EQ(file_size, 0);

    {
        std::ofstream f(fn);
        f.write(content, content_size);
    }

    buffer = file_read(fn, &file_size);
    EXPECT_STREQ((const char*)buffer, content);
    EXPECT_EQ(file_size, content_size);
    free(buffer);

    // Check without the file_size parameter
    buffer = file_read(fn, nullptr);
    EXPECT_STREQ((const char*)buffer, content);
    free(buffer);

    file_size = 1;
    buffer = file_read("nul", &file_size);
    // malloc doesn't offer any guarantee for malloc(0),
    // only that the pointer can be safely passed to free().
    // It can return a NULL pointer or a pointer to a 0-byte buffer.
    EXPECT_EQ(file_size, 0);
    free(buffer);

    std::filesystem::remove(fn);
}

TEST(PatchFile, FileWrite)
{
    EXPECT_NE(file_write("nul/file.txt", "data", 5), 0);

    if (std::filesystem::exists("test.txt")) {
        std::filesystem::remove("test.txt");
    }
    EXPECT_EQ(file_write("test.txt", "data", 5), 0);

    size_t size;
    void *file = file_read("test.txt", &size);
    ASSERT_NE(file, nullptr);
    EXPECT_EQ(size, 5);
    EXPECT_STREQ((char*)file, "data");

    std::filesystem::remove("test.txt");
}

TEST(PatchFile, FnForGame)
{
    char *fn;

    fn = fn_for_game("test.js");
    EXPECT_STREQ(fn, "test.js");
    free(fn);

    ScopedJson runcfg = json_pack("{s:s}", "game", "th06");
    runconfig_load(*runcfg, 0);
    fn = fn_for_game("test.js");
    EXPECT_STREQ(fn, "th06/test.js");
    free(fn);
    runconfig_free();
}

TEST(PatchFile, FnForBuild)
{
    char *fn;

    fn = fn_for_build("test.js");
    EXPECT_EQ(fn, nullptr);

    runconfig_build_set("v1.2.3");
    fn = fn_for_build("test.js");
    EXPECT_STREQ(fn, "test.v1.2.3.js");
    free(fn);
    runconfig_build_set(nullptr);
}

TEST(PatchFile, FnForPatch)
{
    char *fn;
    patch_t patch = { 0 };

    fn = fn_for_patch(nullptr, "test.js");
    EXPECT_EQ(fn, nullptr);

    patch.archive = "C:/Path/to/thcrap/repos/repo_name/patch_name";
    fn = fn_for_patch(&patch, "test.js");
    EXPECT_STREQ(fn, "C:/Path/to/thcrap/repos/repo_name/patch_name/test.js");
    free(fn);

    patch.archive = "C:/Path/to/thcrap/repos/repo_name/patch_name/";
    fn = fn_for_patch(&patch, "test.js");
    EXPECT_STREQ(fn, "C:/Path/to/thcrap/repos/repo_name/patch_name/test.js");
    free(fn);
}

TEST(PatchFile, DirCreateForFn)
{
    if (std::filesystem::exists("test_dir")) {
        std::filesystem::remove_all("test_dir");
    }

    dir_create_for_fn("test_dir/dir2/dir3/file.txt");
    EXPECT_TRUE(std::filesystem::is_directory("test_dir/dir2/dir3"));

    std::filesystem::remove_all("test_dir");
}

class PatchFileTest : public ::testing::Test
{
protected:
    patch_t patch;

    void SetUp() override
    {
        if (std::filesystem::exists("testdir")) {
            std::filesystem::remove_all("testdir");
        }
        std::filesystem::create_directory("testdir");

        ScopedJson patch_js = json_pack("{s:s,s:s,s:[s,s],s:[s,s],s:{s:b,s:b},s:s,s:s,s:i}",
            "id", "base_tsa",
            "title", "Base patch for TSA games",
            "servers", "https://www.example.com/", "https://www.example2.com/",
            "dependencies", "thpatch/patch1", "patch2",
            "fonts", "arial.ttf", true, "comicsans.ttf", true,
            "motd", "This is a message",
            "motd_title", "Title for motd",
            "motd_type", 12
        );
        json_dump_file(*patch_js, "testdir/patch.js", 0);

        ScopedJson runconfig_js = json_pack("{s:[s,s],s:b}",
            "ignore", "*.bmp", "*.jpg",
            "update", false
        );
        this->patch = patch_init("testdir", *runconfig_js, 0);
    }

    void TearDown() override
    {
        patch_free(&this->patch);
        std::filesystem::remove_all("testdir");
    }
};

TEST_F(PatchFileTest, PatchInit)
{
    // Writing a nice test for patch_archive is a bit hard and pointless,
    // we don't really care about its exact value.
    // The patch_file_exist will test if the archive value is actually meaningful.
    EXPECT_NE(patch.archive, nullptr);
    EXPECT_STREQ(patch.id, "base_tsa");
    EXPECT_STREQ(patch.title, "Base patch for TSA games");

    ASSERT_NE(patch.servers, nullptr);
    EXPECT_STREQ(patch.servers[0], "https://www.example.com/");
    EXPECT_STREQ(patch.servers[1], "https://www.example2.com/");
    EXPECT_EQ(patch.servers[2], nullptr);

    // TODO: dependencies
    ASSERT_NE(patch.dependencies, nullptr);
    EXPECT_STREQ(patch.dependencies[0].repo_id, "thpatch");
    EXPECT_STREQ(patch.dependencies[0].patch_id, "patch1");
    EXPECT_EQ(patch.dependencies[1].repo_id, nullptr);
    EXPECT_STREQ(patch.dependencies[1].patch_id, "patch2");
    EXPECT_EQ(patch.dependencies[2].patch_id, nullptr);

    ASSERT_NE(patch.fonts, nullptr);
    EXPECT_STREQ(patch.fonts[0], "arial.ttf");
    EXPECT_STREQ(patch.fonts[1], "comicsans.ttf");
    EXPECT_EQ(patch.fonts[2], nullptr);

    ASSERT_NE(patch.ignore, nullptr);
    EXPECT_STREQ(patch.ignore[0], "*.bmp");
    EXPECT_STREQ(patch.ignore[1], "*.jpg");
    EXPECT_EQ(patch.ignore[2], nullptr);

    EXPECT_EQ(patch.update, false);

    EXPECT_STREQ(patch.motd, "This is a message");
    EXPECT_STREQ(patch.motd_title, "Title for motd");
    EXPECT_EQ(patch.motd_type, 12);

    // TODO: test invalid parameters
}

TEST_F(PatchFileTest, PatchFileExists)
{
    std::ofstream file;

    file.open("testdir/test_exist.js");
    file.close();
    EXPECT_TRUE (patch_file_exists(&patch, "test_exist.js"));
    EXPECT_FALSE(patch_file_exists(&patch, "test_missing.js"));

    std::filesystem::create_directory("testdir/subdir");
    file.open("testdir/subdir/test_exist.js");
    file.close();
    EXPECT_TRUE (patch_file_exists(&patch, "subdir/test_exist.js"));
    EXPECT_FALSE(patch_file_exists(&patch, "subdir/test_missing.js"));

    // Test blacklist
    file.open("testdir/test_blacklist.bmp");
    file.close();
    EXPECT_FALSE(patch_file_exists(&patch, "test_blacklist.bmp"));
}

TEST_F(PatchFileTest, PatchFileBlacklisted)
{
    EXPECT_TRUE (patch_file_blacklisted(&patch, "file.bmp"));
    EXPECT_TRUE (patch_file_blacklisted(&patch, "file.jpg"));
    EXPECT_FALSE(patch_file_blacklisted(&patch, "file.png"));
    EXPECT_FALSE(patch_file_blacklisted(&patch, "file_without_extension"));
    EXPECT_TRUE (patch_file_blacklisted(&patch, "dir/file.bmp"));
    EXPECT_TRUE (patch_file_blacklisted(&patch, "dir/file.jpg"));
    EXPECT_FALSE(patch_file_blacklisted(&patch, "dir/file.png"));
}

TEST_F(PatchFileTest, PatchFileLoad)
{
    std::ofstream file;
    char *buffer;
    size_t file_size;

    file.open("testdir/test_exist.txt");
    file.write("abcde", 6);
    file.close();
    buffer = (char*)patch_file_load(&patch, "test_exist.txt", &file_size);
    EXPECT_STREQ(buffer, "abcde");
    EXPECT_EQ(file_size, 6);
    free(buffer);

    EXPECT_EQ(patch_file_load(&patch, "test_missing.txt", &file_size), nullptr);
    EXPECT_EQ(file_size, 0);

    std::filesystem::create_directory("testdir/subdir");
    file.open("testdir/subdir/test_exist.txt");
    file.write("abcde", 6);
    file.close();
    buffer = (char*)patch_file_load(&patch, "subdir/test_exist.txt", &file_size);
    EXPECT_STREQ(buffer, "abcde");
    EXPECT_EQ(file_size, 6);
    free(buffer);

    // Test blacklist
    file.open("testdir/test_blacklist.bmp");
    file.write("abcde", 6);
    file.close();
    EXPECT_EQ(patch_file_load(&patch, "test_blacklist.bmp", &file_size), nullptr);
    EXPECT_EQ(file_size, 0);

    // Test without size
    buffer = (char*)patch_file_load(&patch, "test_exist.txt", nullptr);
    EXPECT_STREQ(buffer, "abcde");
    free(buffer);
}

// TODO: patch_json_load, patch_json_merge, patch_file_store, patch_json_store, patch_file_delete

TEST_F(PatchFileTest, PatchToRunconfigJson)
{
    ScopedJson json = patch_to_runconfig_json(&patch);
    EXPECT_NE(*json, nullptr);
    EXPECT_STREQ(json_string_value(json_object_get(*json, "archive")), patch.archive);
}

TEST_F(PatchFileTest, PatchRelToAbs)
{
    // Create a file
    patch_file_store(&patch, "test.txt", "abcde", 6);

    std::string cwd = std::filesystem::current_path().generic_u8string();
    patch_rel_to_abs(&patch, cwd.c_str());

    // The patch path should point to the same directory
    EXPECT_TRUE(patch_file_exists(&patch, "test.txt"));

    // The path should be absolute
    std::filesystem::path path = patch.archive;
    EXPECT_TRUE(path.is_absolute());
}

TEST(PatchFile, PatchBuild)
{
    patch_desc_t desc = {
        "repo_name",
        "patch_name"
    };
    patch_t patch = patch_build(&desc);
    EXPECT_STREQ(patch.archive, "repos/repo_name/patch_name/");
    patch_free(&patch);
}

TEST(PatchFile, PatchDepToDesc)
{
    patch_desc_t desc;

    // Invalid value
    desc = patch_dep_to_desc(nullptr);
    EXPECT_EQ(desc.repo_id, nullptr);
    EXPECT_EQ(desc.patch_id, nullptr);

    // Absolute value
    desc = patch_dep_to_desc("nmlgc/base_tsa");
    EXPECT_STREQ(desc.repo_id, "nmlgc");
    EXPECT_STREQ(desc.patch_id, "base_tsa");
    free(desc.repo_id);
    free(desc.patch_id);

    // Relative value
    desc = patch_dep_to_desc("base_tsa");
    EXPECT_EQ(desc.repo_id, nullptr);
    EXPECT_STREQ(desc.patch_id, "base_tsa");
    free(desc.patch_id);
}

// TODO: patchhook
