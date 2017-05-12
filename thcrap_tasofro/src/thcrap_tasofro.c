/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * Plugin entry point and breakpoints.
  */

#include <thcrap.h>
#include "thcrap_tasofro.h"
#include "pl.h"
#include "tfcs.h"

// TODO: read the file names list in JSON format
int __stdcall thcrap_plugin_init()
{
	BYTE* filenames_list;
	size_t filenames_list_size;

	int base_tasofro_removed = stack_remove_if_unneeded("base_tasofro");
	if (base_tasofro_removed == 1) {
		return 1;
	} else if(base_tasofro_removed == -1) {
		const char *game = json_object_get_string(runconfig_get(), "game");
		if(!strcmp(game, "th145")) {
			log_mboxf(NULL, MB_OK | MB_ICONINFORMATION,
				"Support for TH14.5 has been moved out of the sandbox.\n"
				"\n"
				"Please reconfigure your patch stack; otherwise, you might "
				"encounter errors or missing functionality after further "
				"updates.\n"
			);
		}
	}
	
	patchhook_register("*/stage*.pl", patch_pl);
	patchhook_register("*/ed_*.pl", patch_pl);
	patchhook_register("*.csv", patch_tfcs);
	filenames_list = stack_game_file_resolve("fileslist.txt", &filenames_list_size);
	LoadFileNameListFromMemory(filenames_list, filenames_list_size);
	return 0;
}


int BP_file_header(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	BYTE **pHeader = (BYTE**)json_object_get_register(bp_info, regs, "struct");
	DWORD **key = (DWORD**)json_object_get_register(bp_info, regs, "key");
	// ----------
	if (!pHeader || !*pHeader || !key || !*key)
		return 1;

	struct FileHeader *header = (struct FileHeader*)(*pHeader + json_integer_value(json_object_get(bp_info, "struct_offset")));
	struct FileHeaderFull *full_header = register_file_header(header, *key);
	file_rep_t *fr = &full_header->fr;

	if (full_header->path[0]) {
		file_rep_init(fr, full_header->path);
	}

	// If the game loads a DDS file and we have no corresponding DDS file, try to replace it with a PNG file (the game will deal with it)
	if (fr->rep_buffer == NULL && strlen(fr->name) > 4 && strcmp(fr->name + strlen(fr->name) - 4, ".dds") == 0) {
		char png_path[MAX_PATH];
		strcpy(png_path, full_header->path);
		strcpy(png_path + strlen(full_header->path) - 3, "png");
		file_rep_clear(fr);
		file_rep_init(fr, png_path);
	}

	if (fr->rep_buffer != NULL) {
		full_header->size = fr->pre_json_size + fr->patch_size;
		header->size = full_header->size;

		fr->game_buffer = malloc(full_header->size);
		memcpy(fr->game_buffer, fr->rep_buffer, fr->pre_json_size);
		patchhooks_run( // TODO: replace with file_rep_hooks_run
			fr->hooks, fr->game_buffer, fr->pre_json_size + fr->patch_size, fr->pre_json_size, fr->patch
			);

		crypt_block(fr->game_buffer, full_header->size, full_header->key);
	}

	if (fr->rep_buffer == NULL && fr->patch != NULL) {
		// If we have no rep buffer but we have a patch buffer, we will patch the file later, so we need to keep the file_rep.
		header->size += fr->patch_size;
		full_header->size = header->size;
	}
	else {
		file_rep_clear(fr);
	}

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
	json_t *jBuffer = json_object_get(bp_info, "buffer");
	json_t *jSize = json_object_get(bp_info, "size");
	BYTE **file_struct = (BYTE**)json_object_get_register(bp_info, regs, "file_struct");
	BYTE **pBuffer = (BYTE**)reg(regs, json_string_value(jBuffer));
	DWORD *pSize = (DWORD*)reg(regs, json_string_value(jSize));
	// ----------

	static DWORD size = 0;
	static BYTE *buffer = NULL;

	if (pBuffer && *pBuffer) {
		buffer = *pBuffer;
	}
	else if (jBuffer) {
		buffer = (BYTE*)json_integer_value(jBuffer);
	}
	if (pSize) {
		size = *pSize;
	}
	else if (jSize) {
		size = (DWORD)json_integer_value(jSize);
	}

	if (!file_struct) {
		return 1;
	}

	struct FileHeaderFull *header = hash_to_file_header(*(DWORD*)(*file_struct + 0x1001c));
	if (!header || !header->path[0] || (!header->fr.game_buffer && !header->fr.patch)) {
		// Nothing to patch.
		return 1;
	}

	HANDLE hFile = *(HANDLE*)(*file_struct + 4);
	if (buffer == NULL) {
		buffer = *file_struct + 8;
	}
	if (size == 0) {
		size = 65536;
	}

	if (header->effective_offset == -1) {
		header->effective_offset = SetFilePointer(hFile, 0, NULL, FILE_CURRENT) - size;

		// We couldn't patch the file earlier, but now we have a hFile and an offset, so we should be able to.
		if (header->fr.game_buffer == NULL && header->fr.patch != NULL) {
			SetFilePointer(hFile, -(int)size, NULL, FILE_CURRENT);
			
			DWORD nbOfBytesRead;
			header->fr.game_buffer = malloc(header->size);
			ReadFile(hFile, header->fr.game_buffer, header->orig_size, &nbOfBytesRead, NULL);

			uncrypt_block(header->fr.game_buffer, header->orig_size, header->key);
			patchhooks_run(
				header->fr.hooks, header->fr.game_buffer, header->size, header->orig_size, header->fr.patch
				);
			crypt_block(header->fr.game_buffer, header->size, header->key);

			SetFilePointer(hFile, header->effective_offset + size, NULL, FILE_BEGIN);
			file_rep_clear(&header->fr);
		}
	}

	size_t offset = SetFilePointer(hFile, 0, NULL, FILE_CURRENT) - size - header->effective_offset;
	int copy_size = min(header->size - offset, size);

	log_printf("[replace_file]  known path: %s, hash %.8x, offset: %d, requested size %d, file_rep_size left: %d, chosen size: %d\n",
		header->path, *(DWORD*)(*file_struct + 0x1001c), offset, size, header->size - offset, copy_size);
	memcpy(buffer, (BYTE*)header->fr.game_buffer + offset, copy_size);

	buffer = NULL;
	size = 0;
	return 1;
}
