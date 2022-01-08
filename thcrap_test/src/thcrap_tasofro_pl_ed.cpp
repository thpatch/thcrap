#include "thcrap.h"
#include "thcrap_tasofro.h"
#include "gtest/gtest.h"

class ThcrapTasofroPlEdTest : public ::testing::Test
{
protected:
	std::vector<std::string> sample;

	std::unique_ptr<char[]> array_to_file(std::vector<std::string> array, size_t overallocate = 0)
	{
		size_t size = 0;

		for (auto& it : array) {
			size += it.size() + 2;
		}

		auto out = std::make_unique<char[]>(size + 1 + overallocate);
		size_t pos = 0;
		for (auto& it : array) {
			std::copy(it.begin(), it.end(), out.get() + pos);
			pos += it.size();
			memcpy(out.get() + pos, "\r\n", 2);
			pos += 2;
		}
		out[pos] = '\0';

		return out;
	}

	void SetUp() override
	{
		this->sample = {
			// Generic stuff to ignore: comments, other commands, empty lines
			"# Comment",
			",ED_ABCD,0",
			",Sleep,540",
			"",

			// One line
			",ED_MSG,\"One line\"",
			",Sleep,300",

			// 2 lines
			",ED_MSG,\"2 lines - 1\"",
			",ED_MSG,\"2 lines - 2\"",
			",Sleep,300",

			// 3 lines
			",ED_MSG,\"3 lines - 1\"",
			",ED_MSG,\"3 lines - 2\"",
			",ED_MSG,\"3 lines - 3\",R",
			",Sleep,300",
		};
	}

	void RunTest(const std::vector<std::string>& orig, json_t *patch, const std::vector<std::string>& expected)
	{
		auto orig_file = this->array_to_file(orig, 4096);
		auto expected_file = this->array_to_file(expected);

		patch_th175_pl_ed(orig_file.get(), strlen(orig_file.get()) + 4096, strlen(orig_file.get()), "unittest.pl", patch);

		EXPECT_STREQ(expected_file.get(), orig_file.get());

		json_decref(patch);
	}

	void TearDown() override
	{
		this->sample.clear();
	}
};

TEST_F(ThcrapTasofroPlEdTest, EmptyFile) {
	this->RunTest({}, json_object(), {});
}

TEST_F(ThcrapTasofroPlEdTest, PassThrough) {
	this->RunTest(this->sample, json_object(), this->sample);
}

TEST_F(ThcrapTasofroPlEdTest, PatchFile) {
	this->RunTest(this->sample, json_pack(
		"{ s: { s: [ s ] }, s: { s: [ s, s ] }, s: { s: [ s, s, s ] } }",
		"1", "lines", "One line - patched",
		"2", "lines", "2 lines - patched - 1", "2 lines - patched - 2",
		"3", "lines", "3 lines - patched - 1", "3 lines - patched - 2", "3 lines - patched - 3"
	), {
		"# Comment",
		",ED_ABCD,0",
		",Sleep,540",
		"",

		// One line
		",ED_MSG,\"One line - patched\"",
		",Sleep,300",

		// 2 lines
		",ED_MSG,\"2 lines - patched - 1\"",
		",ED_MSG,\"2 lines - patched - 2\"",
		",Sleep,300",

		// 3 lines
		",ED_MSG,\"3 lines - patched - 1\"",
		",ED_MSG,\"3 lines - patched - 2\"",
		",ED_MSG,\"3 lines - patched - 3\",R",
		",Sleep,300",
	});
}

TEST_F(ThcrapTasofroPlEdTest, SkipEmptyLinesInInput) {
	this->RunTest({
		// Empty
		",ED_MSG,\" \"",
		",Sleep,300",

		// Empty with a font color - present only once, in Yorigami ending
		",ED_MSG,\" \",K",
		",Sleep,300",

		// Make sure this one isn't skipped and has an offset of 1
		",ED_MSG,\"line\",R",
		",Sleep,300",
		},
	json_pack("{ s: { s: [ s ] } }",
		"1", "lines", "line - patched"
	), {
		// Empty
		",ED_MSG,\" \"",
		",Sleep,300",

		// Empty with a font color - present only once, in Yorigami ending
		",ED_MSG,\" \",K",
		",Sleep,300",

		// Make sure this one isn't skipped and has an offset of 1
		",ED_MSG,\"line - patched\",R",
		",Sleep,300",
	});
}
