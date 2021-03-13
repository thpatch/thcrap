#include "thcrap.h"
#include "gtest/gtest.h"
#include <filesystem>

static x86_reg_t DummyRegs = { 0 };

#define ExprRepeatCount 100000

#define ExprRepeatTest for (size_t i = 0; i < ExprRepeatCount; ++i)

TEST(ExpressionTest, ExpressionTesting1A) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("1 ? 2 : 3", '\0', &expr_ret, NULL, NULL);
	}
	EXPECT_EQ(expr_ret, 2);
}

TEST(ExpressionTest, ExpressionTesting1B) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("1 ? 2 : 3", '\0', &expr_ret, &DummyRegs, NULL);
	}
	EXPECT_EQ(expr_ret, 2);
}

TEST(ExpressionTest, ExpressionTesting2A) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("10 , 1 ? 2 : 3", '\0', &expr_ret, NULL, NULL);
	}
	EXPECT_EQ(expr_ret, 2);
}

TEST(ExpressionTest, ExpressionTesting2B) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("10 , 1 ? 2 : 3", '\0', &expr_ret, &DummyRegs, NULL);
	}
	EXPECT_EQ(expr_ret, 2);
}

TEST(ExpressionTest, ExpressionTesting3A) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("10 + 1 ? 2 : 3", '\0', &expr_ret, NULL, NULL);
	}
	EXPECT_EQ(expr_ret, 2);
}

TEST(ExpressionTest, ExpressionTesting3B) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("10 + 1 ? 2 : 3", '\0', &expr_ret, &DummyRegs, NULL);
	}
	EXPECT_EQ(expr_ret, 2);
}

TEST(ExpressionTest, ExpressionTesting4A) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("10 += 1 ? 2 : 3", '\0', &expr_ret, NULL, NULL);
	}
	EXPECT_EQ(expr_ret, 12);
}

TEST(ExpressionTest, ExpressionTesting4B) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("10 += 1 ? 2 : 3", '\0', &expr_ret, &DummyRegs, NULL);
	}
	EXPECT_EQ(expr_ret, 12);
}

TEST(ExpressionTest, ExpressionTesting5A) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("1 ? 4 : 1 ? 2 : 3", '\0', &expr_ret, NULL, NULL);
	}
	EXPECT_EQ(expr_ret, 4);
}

TEST(ExpressionTest, ExpressionTesting5B) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("1 ? 4 : 1 ? 2 : 3", '\0', &expr_ret, &DummyRegs, NULL);
	}
	EXPECT_EQ(expr_ret, 4);
}

TEST(ExpressionTest, ExpressionTesting6A) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("0 ? 4 : 1 ? 2 : 3", '\0', &expr_ret, NULL, NULL);
	}
	EXPECT_EQ(expr_ret, 2);
}

TEST(ExpressionTest, ExpressionTesting6B) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("0 ? 4 : 1 ? 2 : 3", '\0', &expr_ret, &DummyRegs, NULL);
	}
	EXPECT_EQ(expr_ret, 2);
}

TEST(ExpressionTest, ExpressionTesting7A) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("4 * 1 + 2 * 3", '\0', &expr_ret, NULL, NULL);
	}
	EXPECT_EQ(expr_ret, 10);
}

TEST(ExpressionTest, ExpressionTesting7B) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("4 * 1 + 2 * 3", '\0', &expr_ret, &DummyRegs, NULL);
	}
	EXPECT_EQ(expr_ret, 10);
}

TEST(ExpressionTest, ExpressionTesting8A) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("((1+1) ? (2):(3))", '\0', &expr_ret, NULL, NULL);
	}
	EXPECT_EQ(expr_ret, 2);
}

TEST(ExpressionTest, ExpressionTesting8B) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("((1+1) ? (2):(3))", '\0', &expr_ret, &DummyRegs, NULL);
	}
	EXPECT_EQ(expr_ret, 2);
}

TEST(ExpressionTest, ExpressionTesting9A) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("((0)?(2):(3))", '\0', &expr_ret, NULL, NULL);
	}
	EXPECT_EQ(expr_ret, 3);
}

TEST(ExpressionTest, ExpressionTesting9B) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("((0)?(2):(3))", '\0', &expr_ret, &DummyRegs, NULL);
	}
	EXPECT_EQ(expr_ret, 3);
}

TEST(ExpressionTest, ExpressionTesting10A) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("(((1) + (1)))", '\0', &expr_ret, NULL, NULL);
	}
	EXPECT_EQ(expr_ret, 2);
}

TEST(ExpressionTest, ExpressionTesting10B) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("(((1) + (1)))", '\0', &expr_ret, &DummyRegs, NULL);
	}
	EXPECT_EQ(expr_ret, 2);
}

TEST(ExpressionTest, ExpressionTesting11A) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("1=2 = 3", '\0', &expr_ret, NULL, NULL);
	}
	EXPECT_EQ(expr_ret, 3);
}

TEST(ExpressionTest, ExpressionTesting11B) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("1=2 = 3", '\0', &expr_ret, &DummyRegs, NULL);
	}
	EXPECT_EQ(expr_ret, 3);
}

TEST(ExpressionTest, ExpressionTesting12A) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("((1)=(2) = (3))", '\0', &expr_ret, NULL, NULL);
	}
	EXPECT_EQ(expr_ret, 3);
}

TEST(ExpressionTest, ExpressionTesting12B) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("((1)=(2) = (3))", '\0', &expr_ret, &DummyRegs, NULL);
	}
	EXPECT_EQ(expr_ret, 3);
}

TEST(ExpressionTest, ExpressionTesting13A) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("255 / 10 + !!(255 % 10)", '\0', &expr_ret, NULL, NULL);
	}
	EXPECT_EQ(expr_ret, 26);
}

TEST(ExpressionTest, ExpressionTesting13B) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("255 / 10 + !!(255 % 10)", '\0', &expr_ret, &DummyRegs, NULL);
	}
	EXPECT_EQ(expr_ret, 26);
}

TEST(ExpressionTest, ExpressionTesting14A) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("4**4", '\0', &expr_ret, NULL, NULL);
	}
	EXPECT_EQ(expr_ret, 256);
}

TEST(ExpressionTest, ExpressionTesting14B) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("4**4", '\0', &expr_ret, &DummyRegs, NULL);
	}
	EXPECT_EQ(expr_ret, 256);
}

TEST(ExpressionTest, ExpressionTesting15B) {
	size_t expr_ret = 0;
	DummyRegs.ah = 2;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("&AH + 1", '\0', &expr_ret, &DummyRegs, NULL);
	}
	EXPECT_EQ(expr_ret, (size_t)&DummyRegs.ah + 1);
}

TEST(ExpressionTest, ExpressionTesting16A) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("100**-1", '\0', &expr_ret, NULL, NULL);
	}
	EXPECT_EQ(expr_ret, UINT_MAX);
}

TEST(ExpressionTest, ExpressionTesting16B) {
	size_t expr_ret = 0;
	ExprRepeatTest{
		expr_ret = 0;
		eval_expr("100**-1", '\0', &expr_ret, &DummyRegs, NULL);
	}
	EXPECT_EQ(expr_ret, UINT_MAX);
}
