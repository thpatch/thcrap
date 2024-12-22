#include "thcrap.h"
#include <filesystem>
#include "gtest/gtest.h"

template<typename T>
static size_t iter_size(T begin, T end)
{
    size_t size = 0;
    while (begin != end) {
        size++;
        begin++;
    }
    return size;
}

static void test(const char *self, const char *target, const char *expected)
{
    if (expected == nullptr) {
        expected = target;
    }

    char *ret = SearchDecideStoredPathForm(target, self);
    EXPECT_STREQ(ret, expected);
    free(ret);
}

TEST(Search, SearchDecideStoredPathForm)
{
    test("C:\\a\\1\\2\\3\\4\\5\\6\\", "C:\\b\\1\\2\\3\\4\\5\\6", nullptr);
    test("D:\\thcrap\\", "E:\\th07\\th07.exe", nullptr);
    test("C:\\Users\\brliron\\Downlods\\thcrap\\", "C:\\Program Files\\Steam\\common\\th07\\th07.exe", nullptr);
    test("C:\\Users\\brliron\\Desktop\\games\\tools\\thcrap\\", "C:\\Users\\brliron\\Desktop\\games\\th07\\th07.exe", "..\\..\\th07\\th07.exe");
    test("C:\\Users\\brliron\\Desktop\\games\\th07\\thcrap\\", "C:\\Users\\brliron\\Desktop\\games\\th07\\th07.exe", "..\\th07.exe");
    test("C:\\Users\\brliron\\Desktop\\games\\", "C:\\Users\\brliron\\Desktop\\games\\th07\\th07.exe", "th07\\th07.exe");
    test("D:\\thcrap\\", "D:\\th07\\th07.exe", "..\\th07\\th07.exe");
}
