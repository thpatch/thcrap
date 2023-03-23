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
