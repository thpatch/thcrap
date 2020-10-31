#include "thcrap.h"
#include "gtest/gtest.h"

TEST(PluginTest, FuncList) {
	EXPECT_EQ(func_get("test"), NULL);
	EXPECT_EQ(func_add("test", 0x12345678), 0);
	EXPECT_EQ(func_add("test", 0x87654321), 1);
	EXPECT_EQ((size_t)func_get("test"), 0x87654321);
	EXPECT_EQ(func_remove("test"), true);
	EXPECT_EQ(func_get("test"), NULL);
	EXPECT_EQ(func_remove("test"), false);
}
