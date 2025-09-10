#include "thcrap.h"
#include "gtest/gtest.h"

static void test_logs(const char* str, const char* output) {
	ASSERT_STREQ(prefilter_log(str, strlen(str)), output);
}


TEST(log_tests, verify_filters) {
	test_logs("Normal Unmatched String", "Normal Unmatched String");
	test_logs("C:\\", "C:\\"); //Partial Match
	test_logs("C:/", "C:/"); //Partial Match Unix
	test_logs("C:\\Users\\PartialMatch", "C:\\Users\\PartialMatch");
	test_logs("C:\\Users\\Marisa Kirisame\\", "%userprofile%\\"); //Basic match
	test_logs("c:\\Users\\Marisa Kirisame\\", "%userprofile%\\"); //Basic match lowercase drive
	test_logs("C:\\Users\\霧雨　魔理沙\\", "%userprofile%\\"); //Basic match - UTF-8
	test_logs("D:\\Users\\霧雨　魔理沙\\", "%userprofile%\\"); //Basic match - alternate drive
	test_logs("Ending C:\\Users\\霧雨　魔理沙\\", "Ending %userprofile%\\"); //Ending Match
	test_logs("C:\\Users\\霧雨　魔理沙\\ Starting", "%userprofile%\\ Starting"); //Starting Match
	test_logs("Middle C:\\Users\\霧雨　魔理沙\\ End", "Middle %userprofile%\\ End"); //Middle Match
	test_logs("Opening Text: C:\\Users\\霧雨　魔理沙\\Games\\th07.exe End", "Opening Text: %userprofile%\\Games\\th07.exe End"); //Complete Path
	test_logs("Opening Text: C:\\Users\\霧雨　魔理沙\\Games\\th07.exe Middle Text: C:\\Users\\Hakurei Reimu\\Games\\th06.exe End",
		"Opening Text: %userprofile%\\Games\\th07.exe Middle Text: %userprofile%\\Games\\th06.exe End"); //Double Match
	test_logs("Opening Text:\\Users D:\\User\\NotTheUsersFolder C:\\Users\\霧雨　魔理沙\\Games\\th07.exe Middle Text",
		"Opening Text:\\Users D:\\User\\NotTheUsersFolder %userprofile%\\Games\\th07.exe Middle Text"); //Intentionally obfuscated Match
	test_logs("Opening Text: C:/Users\\霧雨　魔理沙/Games\\th07.exe End", "Opening Text: %userprofile%/Games\\th07.exe End"); //Variable seperators
	test_logs("Opening Text: C:\\Users/霧雨　魔理沙\\Games\\th07.exe End", "Opening Text: %userprofile%\\Games\\th07.exe End"); //Variable seperators 2
	test_logs("Opening Text: C:/home/霧雨　魔理沙/Games/th07.exe End", "Opening Text: %userprofile%/Games/th07.exe End"); //Wine
	test_logs("Opening Text: C:\\Documents and Settings/霧雨　魔理沙\\Games\\th07.exe End", "Opening Text: %userprofile%\\Games\\th07.exe End"); //XP
}
