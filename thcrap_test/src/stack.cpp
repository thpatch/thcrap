#include "thcrap.h"
#include "gtest/gtest.h"

TEST(StackTest, fn_for_filename)
{
	ASSERT_EQ(fn_for_build("abc.def/g.txt"), nullptr);

	runconfig_build_set("v1.00b");

	char* test1 = fn_for_build("abc.def/g.txt");
	char* test2 = fn_for_build("abc/def.g.txt");
	char* test3 = fn_for_build("abc/def");
	char* test4 = fn_for_build("abc.txt");

	ASSERT_STREQ(test1, "abc.def/g.v1.00b.txt");
	ASSERT_STREQ(test2, "abc/def.v1.00b.g.txt");
	ASSERT_STREQ(test3, "abc/def.v1.00b");
	ASSERT_STREQ(test4, "abc.v1.00b.txt");

	ASSERT_EQ(fn_for_build(nullptr), nullptr);

	thcrap_free(test1);
	thcrap_free(test2);
	thcrap_free(test3);
	thcrap_free(test4);
}
