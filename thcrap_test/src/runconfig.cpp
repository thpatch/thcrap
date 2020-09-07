#include "thcrap.h"
#include <filesystem>
#include "gtest/gtest.h"

class ScopedRunconfig
{
public:
    ScopedRunconfig(ScopedJson cfg)
    {
        runconfig_load(*cfg, 0);
    }

    ~ScopedRunconfig()
    {
        runconfig_free();
    }
};

TEST(RunconfigTest, InitFromJson)
{
    ScopedJson base = json_pack("{s:s,s:s}",
        "name 1", "value 1",
        "name 2", "value 2"
    );
    ScopedRunconfig runconfig(base);
    EXPECT_TRUE(json_equal(runconfig_json_get(), *base));
}

TEST(RunconfigTest, InitFromFile)
{
    // Should not crash
    runconfig_load_from_file("filenotfound.js");

    const char *testcfg_fn = "testcfg.js";
    ScopedJson base = json_pack("{s:s,s:s}",
        "name 1", "value 1",
        "name 2", "value 2"
    );
    EXPECT_EQ(json_dump_file(*base, testcfg_fn, 0), 0);

    runconfig_load_from_file(testcfg_fn);
    EXPECT_TRUE(json_equal(runconfig_json_get(), *base));

    runconfig_free();
    std::filesystem::remove(testcfg_fn);
}

TEST(RunconfigTest, InitWithOverwrite)
{
    ScopedJson base = json_pack("{s:s,s:s}",
        "name 1", "value 1",
        "name 2", "value 2"
    );

    // Test with overwrite
    ScopedJson overwrite = json_pack("{s:s,s:s}",
        "name 2", "value from overwrite",
        "name 3", "new value 3"
    );
    ScopedJson overwrite1_result = json_pack("{s:s,s:s,s:s}",
        "name 1", "value 1",
        "name 2", "value from overwrite",
        "name 3", "new value 3"
    );
    runconfig_load(*base, 0);
    runconfig_load(*overwrite, 0);
    EXPECT_TRUE(json_equal(runconfig_json_get(), *overwrite1_result));
    runconfig_free();

    // Testing the RUNCONFIG_NO_OVERWRITE flag
    ScopedJson overwrite2_result = json_pack("{s:s,s:s,s:s}",
        "name 1", "value 1",
        "name 2", "value 2",
        "name 3", "new value 3"
    );
    runconfig_load(*base, 0);
    runconfig_load(*overwrite, RUNCONFIG_NO_OVERWRITE);
    EXPECT_TRUE(json_equal(runconfig_json_get(), *overwrite2_result));
    runconfig_free();
}

TEST(RunconfigTest, ThcrapDir)
{
    EXPECT_EQ(runconfig_thcrap_dir_get(), nullptr);

    runconfig_thcrap_dir_set(nullptr);
    EXPECT_EQ(runconfig_thcrap_dir_get(), nullptr);

    {
        ScopedRunconfig runconfig(json_pack("{s:s}",
            "thcrap_dir", "C:\\somewhere\\"
        ));
        // Should not be read from the json file: use runconfig_thcrap_dir_set to change.
        EXPECT_EQ(runconfig_thcrap_dir_get(), nullptr);

        runconfig_thcrap_dir_set("C:\\example\\");
        EXPECT_STREQ(runconfig_thcrap_dir_get(), "C:\\example\\");
    }

    EXPECT_EQ(runconfig_thcrap_dir_get(), nullptr);
}

TEST(RunconfigTest, RuncfgFn)
{
    EXPECT_EQ(runconfig_runcfg_fn_get(), nullptr);

    runconfig_runcfg_fn_set(nullptr);
    EXPECT_EQ(runconfig_runcfg_fn_get(), nullptr);

    {
        ScopedRunconfig runconfig(json_pack("{s:s}",
            "runcfg_fn", "C:\\somewhere\\en.js"
        ));
        // Should not be read from the json file: use runconfig_runcfg_fn_set to change.
        EXPECT_EQ(runconfig_runcfg_fn_get(), nullptr);

        runconfig_runcfg_fn_set("C:\\example\\en.js");
        EXPECT_STREQ(runconfig_runcfg_fn_get(), "C:\\example\\en.js");
    }

    EXPECT_EQ(runconfig_runcfg_fn_get(), nullptr);
}

TEST(RunconfigTest, Console)
{
    // Defaults to false
    EXPECT_EQ(runconfig_console_get(), false);

    {
        ScopedRunconfig runconfig(json_pack("{s:b}",
            "console", false
        ));
        EXPECT_FALSE(runconfig_console_get());
    }

    {
        ScopedRunconfig runconfig(json_pack("{s:b}",
            "console", true
        ));
        EXPECT_TRUE(runconfig_console_get());
    }

    // Only true enable the console. Ignore other values.
    {
        ScopedRunconfig runconfig(json_pack("{s:i}",
            "console", 1
        ));
        EXPECT_FALSE(runconfig_console_get());
    }

    EXPECT_EQ(runconfig_console_get(), false);
}

TEST(RunconfigTest, Game)
{
    EXPECT_EQ(runconfig_game_get(), nullptr);

    {
        ScopedRunconfig runconfig(json_pack("{s:s}",
            "game", "test_id"
        ));
        EXPECT_STREQ(runconfig_game_get(), "test_id");
    }

    EXPECT_EQ(runconfig_game_get(), nullptr);
}

TEST(RunconfigTest, Build)
{
    EXPECT_EQ(runconfig_build_get(), nullptr);

    runconfig_build_set(nullptr);
    EXPECT_EQ(runconfig_build_get(), nullptr);

    {
        ScopedRunconfig runconfig(json_pack("{s:s}",
            "build", "v1.2.3"
        ));
        EXPECT_STREQ(runconfig_build_get(), "v1.2.3");

        // Can be changed
        runconfig_build_set("v2.0.0");
        EXPECT_STREQ(runconfig_build_get(), "v2.0.0");
    }

    // runconfig_build_set should work when the build haven't been set yet
    {
        ScopedRunconfig runconfig(json_object());
        EXPECT_EQ(runconfig_build_get(), nullptr);

        runconfig_build_set("v3.0.0");
        EXPECT_STREQ(runconfig_build_get(), "v3.0.0");
    }

    EXPECT_EQ(runconfig_build_get(), nullptr);
}

TEST(RunconfigTest, Title)
{
    EXPECT_EQ(runconfig_title_get(), nullptr);

    // TODO: test with a localized title

    // Title from runconfig
    {
        ScopedRunconfig runconfig(json_pack("{s:s,s:s}",
            "title", "Translated title",
            "game",  "test_id"
        ));
        EXPECT_STREQ(runconfig_title_get(), "Translated title");
    }

    // Game ID
    {
        ScopedRunconfig runconfig(json_pack("{s:s}",
            "game",  "test_id"
        ));
        EXPECT_STREQ(runconfig_title_get(), "test_id");
    }

    EXPECT_EQ(runconfig_title_get(), nullptr);
}

TEST(RunconfigTest, UpdateUrl)
{
    EXPECT_EQ(runconfig_update_url_get(), nullptr);

    {
        ScopedRunconfig runconfig(json_pack("{s:s}",
            "url_update", "https://www.example.com/"
        ));
        EXPECT_STREQ(runconfig_update_url_get(), "https://www.example.com/");
    }

    // Invalid URLs are okay, we only use them as strings to display
    {
        ScopedRunconfig runconfig(json_pack("{s:s}",
            "url_update", "something"
        ));
        EXPECT_STREQ(runconfig_update_url_get(), "something");
    }

    EXPECT_EQ(runconfig_update_url_get(), nullptr);
}

TEST(RunconfigTest, DatDump)
{
    EXPECT_EQ(runconfig_dat_dump_get(), nullptr);

    {
        ScopedRunconfig runconfig(json_pack("{s:b}",
            "dat_dump", false
        ));
        EXPECT_EQ(runconfig_dat_dump_get(), nullptr);
    }

    {
        ScopedRunconfig runconfig(json_pack("{s:s}",
            "dat_dump", "C:\\Somewhere\\"
        ));
        EXPECT_STREQ(runconfig_dat_dump_get(), "C:\\Somewhere\\");
    }

    // With true, default to "dat"
    {
        ScopedRunconfig runconfig(json_pack("{s:b}",
            "dat_dump", true
        ));
        EXPECT_STREQ(runconfig_dat_dump_get(), "dat");
    }

    // Any non-false value is equivalent to true
    {
        ScopedRunconfig runconfig(json_pack("{s:o}",
            "dat_dump", json_object()
        ));
        EXPECT_STREQ(runconfig_dat_dump_get(), "dat");
    }

    EXPECT_EQ(runconfig_dat_dump_get(), nullptr);
}

TEST(RunconfigTest, Latest)
{
    EXPECT_EQ(runconfig_latest_check(), false);
    EXPECT_EQ(runconfig_latest_get(), nullptr);

    runconfig_build_set(nullptr);
    EXPECT_EQ(runconfig_latest_get(), nullptr);

    {
        ScopedRunconfig runconfig(json_pack("{s:s,s:s}",
            "build",  "v1.00a",
            "latest", "v1.00a"
        ));
        EXPECT_EQ(runconfig_latest_check(), true);
        EXPECT_STREQ(runconfig_latest_get(), "v1.00a");
    }

    {
        ScopedRunconfig runconfig(json_pack("{s:s,s:s}",
            "build",  "v1.00a",
            "latest", "v1.00b"
        ));
        EXPECT_EQ(runconfig_latest_check(), false);
        EXPECT_STREQ(runconfig_latest_get(), "v1.00b");
    }

    {
        ScopedRunconfig runconfig(json_pack("{s:s,s:[s,s]}",
            "build",  "v0.01a",
            "latest", "v0.01a", "v1.00b"
        ));
        EXPECT_STREQ(runconfig_latest_get(), "v1.00b");

        EXPECT_EQ(runconfig_latest_check(), true);

        runconfig_build_set("v1.00a");
        EXPECT_EQ(runconfig_latest_check(), false);

        runconfig_build_set("v1.00b");
        EXPECT_EQ(runconfig_latest_check(), true);
    }

    EXPECT_EQ(runconfig_game_get(), nullptr);
}

// TODO: test stages
