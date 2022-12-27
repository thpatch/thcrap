#include "thcrap.h"
#include <filesystem>
#include "gtest/gtest.h"

std::filesystem::path get_dll_source_path()
{
	WCHAR exe_path_c[MAX_PATH];
	GetModuleFileNameW(nullptr, exe_path_c, MAX_PATH);
	std::filesystem::path exe_path = exe_path_c;

	exe_path.remove_filename();
	// This dll doesn't have any dependency (outside of the CRT),
	// we don't link statically to it, and it doesn't run any code
	// from DllMain, so it's a nice target for a quick DLL that
	// we just want to load.
	exe_path /= "act_nut_lib" DEBUG_OR_RELEASE ".dll";

	return exe_path;
}

TEST(NewUTF8Test, UTF8_TO_UTF16) {

#define MakeUTFStrings(string) \
	const char utf8_in[] = u8 ## string;\
	const char16_t utf16[] = u ## string;\
	const char32_t utf32[] = U ## string;

	//MakeUTFStrings("Iñtërnâtiônà£ißætiøn☃💩");
	//MakeUTFStrings("e💩🍙e");

	const char utf8_in[] = u8"e💩🍙e";
	const char16_t utf16[] = u"e💩🍙e";
	const char32_t utf32[] = U"e💩🍙e";

	const char16_t* utf16_out_bit_test_reset = utf8_to_utf16_bit_test_reset(utf8_in);
	const char16_t* utf16_out_masking = utf8_to_utf16_masking(utf8_in);
	const char16_t* utf16_out_bit_scan = utf8_to_utf16_bit_scan(utf8_in);
	//const char32_t* utf32_out = utf8_to_utf32(utf8_in);

	printf("\nConverted UTF8 Bytes: (Bit Test And Reset)\n");
	for (size_t i = 0; i < elementsof(utf16); ++i) {
		printf("%04hX", (uint32_t)utf16_out_bit_test_reset[i]);
	}
	
	printf("\nConverted UTF8 Bytes: (Masking)\n");
	for (size_t i = 0; i < elementsof(utf16); ++i) {
		printf("%04hX", (uint32_t)utf16_out_masking[i]);
	}

	printf("\nConverted UTF8 Bytes: (Bit Scan)\n");
	for (size_t i = 0; i < elementsof(utf16); ++i) {
		printf("%04hX", (uint32_t)utf16_out_bit_scan[i]);
	}
	
	printf("\nCorrect UTF16 Bytes:\n");
	for (size_t i = 0; i < elementsof(utf16); ++i) {
		printf("%04hX", (uint32_t)utf16[i]);
	}

	/*printf("\nConverted UTF8 Bytes:\n");
	for (size_t i = 0; i < elementsof(utf32); ++i) {
		printf("%08X", utf32_out[i]);
	}

	printf("\nCorrect UTF32 Bytes:\n");
	for (size_t i = 0; i < elementsof(utf32); ++i) {
		printf("%08X", utf32[i]);
	}*/


	//WCHAR_T_DEC(utf8_in);
	//WCHAR_T_CONV(utf8_in);
	/*printf("WChar Output Bytes:\n");
	for (size_t i = 0; i < elementsof(utf16); ++i) {
		printf("%04hX", utf8_in_w[i]);
	}*/
	//WCHAR_T_FREE(utf8_in);
	EXPECT_TRUE(
		!wcscmp((wchar_t*)utf16, (wchar_t*)utf16_out_masking) &&
		!wcscmp((wchar_t*)utf16, (wchar_t*)utf16_out_bit_scan)
	);
}

TEST(Win32Utf8Test, GetModuleHandleEx)
{
	if (GetACP() == CP_UTF8) {
		FAIL() << "This test can't run with an UTF-8 code page.";
	}

	// A mix of characters from a western locale and from a Japanese locale.
	// Can't be represented with a single code page.
	std::filesystem::path dll_dir = "tmp";
	std::filesystem::path dll_fn = L"東方_éè.dll";
	std::filesystem::path dll_path = std::filesystem::absolute(dll_dir / dll_fn);

	if (std::filesystem::create_directory(dll_dir) == false) {
		throw std::runtime_error("tmp directory already exists");
	}
	std::filesystem::copy_file(get_dll_source_path(), dll_path);

	HMODULE hMod = LoadLibraryW(dll_path.native().c_str());
	ASSERT_NE(hMod, nullptr);

	HMODULE dummy;
	EXPECT_FALSE(GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, dll_fn.u8string().c_str(), &dummy));
	EXPECT_TRUE( GetModuleHandleExU(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, dll_fn.u8string().c_str(), &dummy));

	FreeLibrary(hMod);
	std::filesystem::remove(dll_path);
	std::filesystem::remove(dll_dir);
}
