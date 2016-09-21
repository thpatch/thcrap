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

	if (full_header->path[0]) {
		full_header->file_rep = stack_game_file_resolve(full_header->path, &full_header->file_rep_size);
	}

	// If the game loads a DDS file and we have no corresponding DDS file, try to replace it with a PNG file (the game will deal with it)
	if (full_header->file_rep == NULL && full_header->path[0] && strlen(full_header->path) > 4 && strcmp(full_header->path + strlen(full_header->path) - 4, ".dds") == 0) {
		char png_path[MAX_PATH];
		strcpy(png_path, full_header->path);
		strcpy(png_path + strlen(full_header->path) - 3, "png");
		full_header->file_rep = stack_game_file_resolve(png_path, &full_header->file_rep_size);
	}

	if (full_header->file_rep != NULL) {
		crypt_block(full_header->file_rep, full_header->file_rep_size, full_header->key);
		(*header)->size = full_header->file_rep_size;
		full_header->size = full_header->file_rep_size;
	}
	return 1;
}

/**
  * So, the game seem to call ReadFile in a quite random way. I'll guess some things:
  * - First, it always do a 1st read of size 0x10000, and uses it to check the magic bytes.
  * - Then, it reads the file from the beginning (yeah, it always re-read the 0x10000 first bytes), in one of more calls.
  *   + With an ogg file, it reads the file by blocks of 0x10000 bytes. title.ogg (2377849 bytes) is read with the first read of 0x10000, then 4 read of 0x10000.
  *   + With a dds  file, it reads the file entirely in one call (after the magic check).
  *   + With a TFBM file, it reads a first block of 0x10000, then the remaining bytes.
  *   + With a png  file, it re-reads the first 0x10000 bytes 2 more times, then reads the file like an ogg file (so the game ends up reading the first bytes 4 times).
  * - For some file extensions (csv, dll, nut, maybe some others), the game don't do another read to check the magic bytes. It reads the entire file in one go.
  *
  * From these guessings, the following may work for all files:
  * - For the first read, give the number requested bytes from the beginning (repeat this step 3 times for a png file).
  * - If there is a second read, give again the numbre of requested bytes from the beginning.
  * - For all the following reads, give the number of requested bytes from the end of the last read.
  */
int BP_replace_file(x86_reg_t *regs, json_t *bp_info)
{
	static struct FileHeaderFull *header_cache = NULL;
	static int stage = 1;
	static int repeat_stage_1 = 0;
	static size_t offset = 0;

	// Parameters
	// ----------
	char **path      = (char**)json_object_get_register(bp_info, regs, "path");
	char **buffer    = (char**)json_object_get_register(bp_info, regs, "buffer");
	DWORD *size      = (DWORD*)json_object_get_register(bp_info, regs, "size");
	DWORD **ret_size = (DWORD**)json_object_get_register(bp_info, regs, "ret_size");
	// ----------

	BOOL skip_call = FALSE;

	log_print("[replace_file] ");

	if (path && *path) {
		log_printf("path: %s", *path);

		const char* local_path = *path;
		if ((*path)[1] == ':') { // The game uses a full path for whatever reason
			local_path = strstr(local_path, "data/"); // TODO: use a reverse strstr
			if (local_path == NULL)
				return 1;
		}
		header_cache = hash_to_file_header(filename_to_hash(local_path));

		if (header_cache) {
			log_printf(";   known path: %s", header_cache->path);
		}

		// Reset the reader
		stage = 1;
		repeat_stage_1 = 0;
		offset = 0;
	}


	if (buffer && *buffer && size && ret_size && ret_size && header_cache) {
		log_printf("buffer: %p, size: %d", *buffer, *size);
		if (header_cache->file_rep) {
			int copy_size;
			if (stage == 1) {
				// Skipping
				log_print("\nStage 1");

				copy_size = min(header_cache->file_rep_size, *size);
				log_printf(": file_rep_size: %d, input size: %d, chosen size: %d", header_cache->file_rep_size, *size, copy_size);
				memcpy(*buffer, (BYTE*)header_cache->file_rep, copy_size);
				**ret_size = copy_size;

				if (repeat_stage_1 == 0 && **(DWORD**)buffer == 0x474E5089 /* \x89PNG */) {
					repeat_stage_1 = 3;
				}
				if (repeat_stage_1 > 0) {
					repeat_stage_1--;
				}
				if (repeat_stage_1 <= 0) {
					stage = 2;
				}
			}
			else if (stage == 2) {
				// Reading the first bytes
				log_print("\nStage 2");

				copy_size = min(header_cache->file_rep_size - offset, *size);
				log_printf(": offset: %d, file_rep_size left: %d, input size: %d, chosen size: %d", offset, header_cache->file_rep_size - offset, *size, copy_size);
				memcpy(*buffer, (BYTE*)header_cache->file_rep + offset, copy_size);
				**ret_size = copy_size;
				offset += copy_size;
			}
			skip_call = TRUE;
		}
	}
	log_print("\n");

	if (skip_call == TRUE) {
		// Skipping the original call
		regs->esp += 20;
		return 0;
	}
	return 1;
}
