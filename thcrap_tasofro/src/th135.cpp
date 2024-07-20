/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Plugin breakpoints and hooks for th135+.
  */

#include <thcrap.h>
#include <vfs.h>
#include "thcrap_tasofro.h"
#include "files_list.h"
#include "pl.h"
#include "tfcs.h"
#include "act-nut.h"
#include "spellcards_generator.h"
#include "bgm.h"
#include "plugin.h"
#include "nhtex.h"
#include "th155_bmp_font.h"
#include "crypt.h"
#include "mediawiki.h"

const MwDefinition mwdef_th135 =
{
	{ "\\R[", "|", "]", MwDefinition::Ruby::Order::BottomThenTop }, // ruby
};

int th135_init()
{
	if (game_id >= TH145) {
		ICrypt::instance = new CryptTh145();
	}
	else {
		ICrypt::instance = new CryptTh135();
	}
	
	patchhook_register("*/stage*.pl", patch_pl, nullptr);
	patchhook_register("*/ed*.pl", patch_pl, nullptr);
	patchhook_register("*.csv", patch_tfcs, get_tfcs_size);
	patchhook_register("*.dll", patch_dll, [](const char*, json_t*, size_t) -> size_t { return 0; });
	patchhook_register("*.act", patch_act, nullptr);
	patchhook_register("*.nut", patch_nut, nullptr);
	patchhook_register("*.txt", patch_plaintext, nullptr);
	patchhook_register("*.nhtex", patch_nhtex, get_nhtex_size);

	if (game_id >= TH155) {
		jsonvfs_game_add_map("data/spell/*.csv.jdiff",							{ "spells.js" });
		jsonvfs_game_add_map("data/story/spell_list/*.csv.jdiff",				{ "spells.js" });
		jsonvfs_game_add_map("data/actor/*.nut.jdiff",							{ "spells.js" }); // Last words
		jsonvfs_game_add_map("*.nut.jdiff",										{ "nut_strings.js" });
		patchhook_register("data/font/*.bmp", patch_bmp_font, get_bmp_font_size);
	}
	else if (game_id >= TH145) {
		jsonvfs_game_add("data/csv/story/*/stage*.csv.jdiff",					{ "spells.js" }, spell_story_generator);
		jsonvfs_game_add("data/csv/spellcard/*.csv.jdiff",						{ "spells.js" }, spell_player_generator);
		jsonvfs_game_add("data/system/char_select3/*/equip/*/000.png.csv.jdiff",{ "spells.js" }, spell_char_select_generator);
	}
	else {
		jsonvfs_game_add("data/csv/story/*/stage*.csv.jdiff",					{ "spells.js" }, spell_story_generator);
		jsonvfs_game_add_map("data/csv/Item*.csv.jdiff",						{ "spellcomments.js", "spells.js" });
	}

	char *bgm_pattern_fn = fn_for_game("data/bgm/bgm.csv.jdiff");
	char *musiccmt_fn = fn_for_game("musiccmt.js");
	jsonvfs_add(bgm_pattern_fn, { "themes.js", musiccmt_fn }, bgm_generator);
	free(musiccmt_fn);
	free(bgm_pattern_fn);
	if (game_id >= TH155) {
		char *staffroll_fn = fn_for_game("data/system/ed/staffroll.csv.jdiff");
		jsonvfs_add_map(staffroll_fn, { "themes.js" });
		free(staffroll_fn);
	}

	return 0;
}


extern "C" int BP_th135_file_name(x86_reg_t *regs, json_t *bp_info)
{
	if unexpected(runconfig_dat_dump_get()) {
		if (const char *filename = (const char*)json_object_get_immediate(bp_info, regs, "file_name")) {
			register_filename(filename);
		}
	}
	return 1;
}

// The games' file structure have the following layout
struct Th155AVFileReaderVtable
{
	void (*vtable0)();
	bool (*OpenFileOrSomething)(void *filenameOrSomething); // Like openFile, but the parameter can be either a filename or... something, I'm not sure why. This function isn't used anyway.
	bool (*OpenFile)(const char *filename);
	void (*vtable3)();
	void (*ReadAndDecryptFile)();
	void (*SeekAndReadFile)(DWORD distanceToMove, DWORD moveMethod);
	DWORD (*GetFileSize)(void);
	DWORD (*GetOffsetForXor)(void);
	void (*vtable8)();
	DWORD (*GetFilenameHash)(void);
};

struct Th135AVFileReader // Inherits from AVPackageFileReader
{
	/* +00000 */ void *vtable;
	/* +00004 */ BYTE buffer[0x10000];
	/* +10004 */ DWORD LastReadFileSize; // Last value returned in lpNumberOfBytesRead in ReadFile
	/* +10008 */ DWORD unk1; // An offset or a size used when reading from the archive file
	/* +1000C */ HANDLE hFile;
	/* +10010 */ DWORD unk2; // An offset or a size updated after reading from the archive file
	/* +10014 */ DWORD Offset; // Offset for the reader
	/* +10018 */ DWORD FileSize;
	/* +1001C */ DWORD FileNameHash;
	/* +10020 */ DWORD unk3;
	/* +10024 */ DWORD XorOffset; // Offset used by the XOR function
	/* +10028 */ DWORD Key[5]; // 32-bytes encryption key used for XORing. The 5th DWORD is a copy of the 1st DWORD.
};

// th145 and th155
struct Th145AVFileReader // Inherits from AVPackageReader
{
	/* +00000 */ Th155AVFileReaderVtable *vtable; // The th145 vtable is different
	/* +00004 */ HANDLE hFile;
	/* +00008 */ BYTE buffer[0x10000];
	/* +10008 */ DWORD LastReadFileSize; // Last value returned in lpNumberOfBytesRead in ReadFile
	/* +1000C */ DWORD Offset; // Offset for the reader. I don't know what happens to it when it's bigger than 0x10000
	/* +10010 */ DWORD LastReadSize; // Last read size asked to the reader
	/* +10014 */ DWORD unknown1;
	/* +10018 */ DWORD FileSize;
	/* +1001C */ DWORD FileNameHash;
	/* +10020 */ BOOL  unknown2;
	/* +10024 */ DWORD XorOffset; // Offset used by the XOR function
	/* +10028 */ DWORD Key[5]; // 32-bytes encryption key used for XORing. The 5th DWORD is a copy of the 1st DWORD.
	/* +1003C */ DWORD Aux; // Contains the last DWORD read from the file. Used during XORing.
	/* +10040 */ DWORD unknown3; // Used during decryption
};

std::unordered_map<HANDLE, Th135File*> openFiles;
std::mutex openFilesMutex;

#define AoCF_PROFILING 0

#if AoCF_PROFILING
size_t file_count = 0;
LARGE_INTEGER qpc_start;
#endif

static inline bool th135_init_fr_inner(Th135File* fr, const char* path) {
	fr->init(path);
	if (fr->need_replace()) {
		return true;
	}
	fr->clear();
	return false;
}

bool th135_init_fr(Th135File *fr, std::filesystem::path& path)
{
	if (path.is_absolute()) {
		path = path.lexically_relative(std::filesystem::current_path());
	}

#if !CPP20
	std::string path_str = path.generic_u8string();
#else
	std::u8string path_str = path.generic_u8string();
#endif
	char* path_ptr = (char*)path_str.c_str();

	if (th135_init_fr_inner(fr, path_ptr)) {
		return true;
	}

	// If the game loads a DDS file and we have no corresponding DDS file,
	// try to replace it with a PNG file (the game will deal with it)
	size_t final_dot = path_str.find_last_of('.');
	if (
		final_dot == path_str.length() - 4 &&
		*(uint32_t*)&path_ptr[final_dot + 1] == TextInt('d', 'd', 's', '\0')
	) {
		// This write is safe because the string length is known and doesn't change
		*(uint32_t*)&path_ptr[final_dot + 1] = TextInt('p', 'n', 'g', '\0');

		if unexpected(runconfig_dat_dump_get()) {
			register_utf8_filename(path_ptr);
		}

		return th135_init_fr_inner(fr, path_ptr);
	}

	return false;
}

bool th135_init_fr(Th135File *fr, const char *path) {
	size_t path_len = strlen(path) + 1;
	VLA(wchar_t, path_w, path_len);
	size_t path_w_len = (size_t)StringToUTF16(path_w, path, path_len) - 1;
	std::filesystem::path fs_path = std::filesystem::path(path_w, path_w + path_w_len);
	bool ret = th135_init_fr(fr, fs_path);
	VLA_FREE(path_w);
	return ret;
}

template<typename T>
int th135_openFileCommon(const char *filename, T *file)
{
#if AoCF_PROFILING
	if unexpected(!file_count++) {
		QueryPerformanceCounter(&qpc_start);
	}
#endif
	Th135File *fr = new Th135File();
	if (!th135_init_fr(fr, filename)) {
		delete fr;
		return 1;
	}

	memcpy(fr->key, file->Key, 4 * sizeof(DWORD));
	ICrypt::instance->convertKey(fr->key);

	file->FileSize = fr->init_game_file_size(static_cast<size_t>(file->FileSize));

	bool already_opened;
	{
		std::lock_guard lock(openFilesMutex);
		already_opened = !openFiles.insert_or_assign(file->hFile, fr).second;
	}
	if (already_opened) {
		log_print("Warning: opening an already-opened file\n");
	}

	return 1;
}

extern "C" int BP_th135_openFile(x86_reg_t * regs, json_t * bp_info)
{
	// Parameters
	// ----------
	const char *filename_ = (const char*)json_object_get_immediate(bp_info, regs, "filename");
	Th135AVFileReader *file = (Th135AVFileReader*)json_object_get_immediate(bp_info, regs, "reader");
	// ----------
	thread_local const char *filename = nullptr;

	if (filename_)
		filename = filename_;
	if (!filename || !file)
		return 1;

	int ret = th135_openFileCommon(filename, file);
	filename = 0;
	return ret;
}

extern "C" int BP_th145_openFile(x86_reg_t * regs, json_t * bp_info)
{
	// Parameters
	// ----------
	const char *filename = (const char*)json_object_get_immediate(bp_info, regs, "filename");
	Th145AVFileReader *file = (Th145AVFileReader*)json_object_get_immediate(bp_info, regs, "reader");
	// ----------

	if (!filename || !file)
		return 1;

	return th135_openFileCommon(filename, file);
}

extern "C" int BP_th135_replaceReadFile(x86_reg_t *regs, json_t*)
{
#if AoCF_PROFILING
	defer(
		// The initial file loading code for AoCF parses
		// exactly 610 files before reaching the main menu,
		// so this can be used to roughly measure how much
		// time is getting used just by the file replacement code.
		if unexpected(file_count == 610) {
			LARGE_INTEGER qpc_end;
			QueryPerformanceCounter(&qpc_end);
			LARGE_INTEGER perf_freq;
			QueryPerformanceFrequency(&perf_freq);
			double time = (double)(qpc_end.QuadPart - qpc_start.QuadPart) / perf_freq.QuadPart;
			log_printf(
				"---------------------------\n"
				"File startup completed in %f seconds\n"
				"---------------------------\n"
				, time
			);
		}
	);
#endif
	ReadFileStack *stack = (ReadFileStack*)(regs->esp + sizeof(void*));

	std::lock_guard lock(openFilesMutex);
	auto fr_iterator = openFiles.find(stack->hFile);
	if (fr_iterator == openFiles.end()) {
		return 1;
	}
	Th135File* fr = fr_iterator->second;

	return fr->replace_ReadFile(regs,
		[](TasofroFile* fr, BYTE *buffer, size_t size) { ICrypt::instance->uncryptBlock(buffer, size, ((Th135File*)fr)->key); },
		[](TasofroFile* fr, BYTE *buffer, size_t size) { ICrypt::instance->cryptBlock(buffer, size, ((Th135File*)fr)->key); }
	);
}




/// Detour chains
/// -------------
typedef BOOL WINAPI CloseHandle_type(
	HANDLE hObject
);

DETOUR_CHAIN_DEF(CloseHandle);
/// -------------

BOOL WINAPI th135_CloseHandle(
	HANDLE hObject
)
{
	// th155 doesn't have a single "close file" function. In a few places, the AVFileReader structure
	// is allocated on the stack, and the game just calls CloseHandle and leaves the function to destroy
	// the object.
	// So, since CloseHandle is the only thing in common between all the places where AVFileReader
	// can be destroyed... We'll use it.
	openFilesMutex.lock();
	auto node = openFiles.extract(hObject);
	openFilesMutex.unlock();
	if (node) {
		delete node.mapped();
	}
	return chain_CloseHandle(hObject);
}

extern "C" void tasofro_mod_detour(void)
{
	if (game_id == TH135 || game_id == TH145 || game_id == TH155 || game_id == TH_MARILEGA) {
		detour_chain("kernel32.dll", 1,
			"CloseHandle", th135_CloseHandle, &chain_CloseHandle,
			nullptr
		);
	}
}
