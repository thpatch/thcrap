/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Plugin entry point and breakpoints.
  */

#include <thcrap.h>
#include <bp_file.h>
#include "thcrap_tasofro.h"

// TODO: read the file names list in JSON format
int __stdcall thcrap_plugin_init()
{
	BYTE* filenames_list;
	size_t filenames_list_size;
	
	patchhook_register("*/stage*.pl", patch_pl);
	filenames_list = stack_game_file_resolve("fileslist.txt", &filenames_list_size);
	LoadFileNameListFromMemory(filenames_list, filenames_list_size);
	return 0;
}


int BP_file_header(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	struct FileHeader **header = (struct FileHeader**)json_object_get_register(bp_info, regs, "struct");
	DWORD **key = (DWORD**)json_object_get_register(bp_info, regs, "key");
	// ----------
	struct FileHeaderFull *full_header = register_file_header(*header, *key);
	file_rep_t fr;
	ZeroMemory(&fr, sizeof(file_rep_t));

	if (full_header->path[0]) {
		file_rep_init(&fr, full_header->path);
	}

	// If the game loads a DDS file and we have no corresponding DDS file, try to replace it with a PNG file (the game will deal with it)
	if (fr.rep_buffer == NULL && strlen(fr.name) > 4 && strcmp(fr.name + strlen(fr.name) - 4, ".dds") == 0) {
		char png_path[MAX_PATH];
		strcpy(png_path, full_header->path);
		strcpy(png_path + strlen(full_header->path) - 3, "png");
		file_rep_clear(&fr);
		file_rep_init(&fr, png_path);
	}

	if (fr.rep_buffer == NULL && fr.patch != NULL) {
		log_print("Patching without replacement file isn't supported yet. Please provide the file you are patching.\n");
	}

	// TODO: I want my json patch even if I don't have a replacement file!!!
	if (fr.rep_buffer != NULL) {
		fr.game_buffer = malloc(fr.pre_json_size + fr.patch_size);
		memcpy(fr.game_buffer, fr.rep_buffer, fr.pre_json_size);
		patchhooks_run( // TODO: replace with file_rep_hooks_run
			fr.hooks, fr.game_buffer, fr.pre_json_size + fr.patch_size, fr.pre_json_size, fr.patch
			);

		full_header->file_rep = fr.game_buffer;
		full_header->file_rep_size = fr.pre_json_size + fr.patch_size;
		crypt_block(full_header->file_rep, full_header->file_rep_size, full_header->key);
		(*header)->size = full_header->file_rep_size;
		full_header->size = full_header->file_rep_size;
	}
	file_rep_clear(&fr);

	return 1;
}

/**
  * Replace file, 3rd attempt - hopefully I won't have to write a 4th one.
  * This breakpoint takes care of patching the files.
  * It takes place in the game's file reader, right after it calls its ReadFile caller.
  * At this point, we have the game's file structure in ESI, and so the filename's hash - we can't
  * read the wrong file.
  * The game's file structure have the following fields:
  * DWORD unknown1;
  * HANDLE hFile;
  * BYTE buffer[0x10000];
  * DWORD LastReadFileSize; // Last value returned in lpNumberOfBytesRead in ReadFile
  * DWORD Offset; // Offset for the reader. I don't know what happens to it when it's bigger than 0x10000
  * DWORD LastReadSize; // Last read size asked to the reader
  * DWORD unknown2;
  * DWORD FileSize;
  * DWORD FileNameHash;
  * DWORD unknown3;
  * DWORD XorOffset; // Offset used by the XOR function
  * DWORD Key[5]; // 32-bytes encryption key used for XORing. The 5th DWORD is a copy of the 1st DWORD.
  * DWORD Aux; // Contains the last DWORD read from the file. Used during XORing.
  * We use hFile (file_struct + 4), buffer (file_struct + 8) and FileNameHash (file_struct + 0x1001c).
  * All the other fields are listed for documentation.
  */
int BP_replace_file(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	BYTE **file_struct = (BYTE**)json_object_get_register(bp_info, regs, "file_struct");
	// ----------

	if (!file_struct) {
		return 1;
	}

	struct FileHeaderFull *header = hash_to_file_header(*(DWORD*)(*file_struct + 0x1001c));
	if (!header || !header->path[0] || !header->file_rep) {
		// Nothing to patch.
		return 1;
	}

	if (header->effective_offset == -1) {
		header->effective_offset = SetFilePointer(*(HANDLE*)(*file_struct + 4), 0, NULL, FILE_CURRENT) - 65536;
	}

	char *buffer = *file_struct + 8;
	size_t offset = SetFilePointer(*(HANDLE*)(*file_struct + 4), 0, NULL, FILE_CURRENT) - 65536 - header->effective_offset;
	int copy_size = min(header->file_rep_size - offset, 65536);

	log_printf("[replace_file]  known path: %s, hash %.8x, offset: %d, file_rep_size left: %d, chosen size: %d\n",
		header->path, *(DWORD*)(*file_struct + 0x1001c), offset, header->file_rep_size - offset, copy_size);
	memcpy(buffer, (BYTE*)header->file_rep + offset, copy_size);

	return 1;
}
