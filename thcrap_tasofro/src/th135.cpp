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
		jsonvfs_game_add_map("data/csv/Item*.csv.jdiff",						{ "spellcomments.js" });
		jsonvfs_game_add_map("data/csv/Item*.csv.jdiff",						{ "spells.js" });
	}

	char *bgm_pattern_fn = fn_for_game("data/bgm/bgm.csv.jdiff");
	char *musiccmt_fn = fn_for_game("musiccmt.js");
	jsonvfs_add(bgm_pattern_fn, { "themes.js", musiccmt_fn }, bgm_generator);
	SAFE_FREE(musiccmt_fn);
	SAFE_FREE(bgm_pattern_fn);
	if (game_id >= TH155) {
		char *staffroll_fn = fn_for_game("data/system/ed/staffroll.csv.jdiff");
		jsonvfs_add_map(staffroll_fn, { "themes.js" });
		SAFE_FREE(staffroll_fn);
	}

	return 0;
}


extern "C" int BP_th135_file_name(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	const char *filename = (const char*)json_object_get_immediate(bp_info, regs, "file_name");
	// ----------

	if (filename) {
		register_filename(filename);
	}
	return 1;
}

// In th145 and th155, the game's file structure have the following layout (TODO: check vtable for th145)
struct AVFileReaderVtable
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

struct AVFileReader // Inherits from AVPackageReader
{
	/* +00000 */ AVFileReaderVtable *vtable;
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

bool th135_init_fr(Th135File *fr, std::filesystem::path path)
{
	fr->init(path.generic_u8string().c_str());
	if (fr->need_replace()) {
		return true;
	}

	fr->clear();

	// If the game loads a DDS file and we have no corresponding DDS file,
	// try to replace it with a PNG file (the game will deal with it)
	if (path.extension() == ".dds") {
		path.replace_extension(".png");
		register_utf8_filename(path.generic_u8string().c_str());

		return th135_init_fr(fr, path);
	}

	return false;
}

extern "C" int BP_th135_openFile(x86_reg_t * regs, json_t * bp_info)
{
	// Parameters
	// ----------
	const char *filename = (const char*)json_object_get_immediate(bp_info, regs, "filename");
	AVFileReader *file = (AVFileReader*)json_object_get_immediate(bp_info, regs, "reader");
	// ----------

	if (!filename || !file)
		return 1;

	Th135File *fr = new Th135File();
	if (!th135_init_fr(fr, filename)) {
		delete fr;
		return 1;
	}

	memcpy(fr->key, file->Key, 4 * sizeof(DWORD));
	ICrypt::instance->convertKey(fr->key);

	file->FileSize = fr->init_game_file_size(static_cast<size_t>(file->FileSize));

	{
		std::scoped_lock lock(openFilesMutex);
		if (openFiles.count(file->hFile) > 0) {
			log_printf("Warning: opening an already-opened file\n");
		}
		openFiles[file->hFile] = fr;
	}

	return 1;
}

extern "C" int BP_th135_replaceReadFile(x86_reg_t *regs, json_t*)
{
	ReadFileStack *stack = (ReadFileStack*)(regs->esp + sizeof(void*));
	std::scoped_lock lock(openFilesMutex);

	auto fr_iterator = openFiles.find(stack->hFile);
	if (fr_iterator == openFiles.end()) {
		return 1;
	}
	Th135File *fr = fr_iterator->second;

	return fr->replace_ReadFile(regs,
		[fr](TasofroFile*, BYTE *buffer, DWORD size) { ICrypt::instance->uncryptBlock(buffer, size, fr->key); },
		[fr](TasofroFile*, BYTE *buffer, DWORD size) { ICrypt::instance->cryptBlock(buffer, size, fr->key); }
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
	{
		// th155 doesn't have a single "close file" function. In a few places, the AVFileReader structure
		// is allocated on the stack, and the game just calls CloseHandle and leaves the function to destroy
		// the object.
		// So, since CloseHandle is the only thing in common between all the places where AVFileReader
		// can be destroyed... We'll use it.
		std::scoped_lock lock(openFilesMutex);
		auto it = openFiles.find(hObject);
		if (it != openFiles.end()) {
			delete it->second;
			openFiles.erase(it);
		}
	}
	return chain_CloseHandle(hObject);
}

void tasofro_mod_detour(void)
{
	if (game_id == TH135 || game_id == TH145 || game_id == TH155) {
		detour_chain("kernel32.dll", 1,
			"CloseHandle", th135_CloseHandle, &chain_CloseHandle,
			nullptr
		);
	}
}
