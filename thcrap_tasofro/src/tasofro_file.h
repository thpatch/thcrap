/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Helper functions for non-contiguous files
  */

#pragma once

#include <thcrap.h>
#include <mutex>
#include <bp_file.h>

// Pointer to the stack just before calling ReadFile.
// Used to read and modify the ReadFile parameters before the function call.
struct ReadFileStack
{
	HANDLE hFile;
	LPBYTE lpBuffer;
	DWORD nNumberOfBytesToRead;
	LPDWORD lpNumberOfBytesRead;
	LPOVERLAPPED lpOverlapped;
};

struct TasofroFile;

func_ptr_typedef(void, , crypt_func_t)(TasofroFile *fr, BYTE *buffer, size_t size);

struct TasofroFile : public file_rep_t
{
	// Offset of this file in the archive
	size_t offset;
	// True if the file have already been patched
	// and re-encrypted
	bool init_done;

	static TasofroFile* tls_get();
	static void tls_set(TasofroFile *file);

	TasofroFile();
	~TasofroFile();

	void init(const char *filename);
	// Updates internal state according to the game file's initial size.
	// Returns the maximum size that this file might need.
	size_t init_game_file_size(size_t game_file_size);
	void clear();

	// Return true if we need (or might need) to replace the file, false otherwise.
	bool need_replace() const;
	// Put a breakpoint over a ReadFile call, and call this function from
	// inside this breakpoint.
	// It will either replace the game's ReadFile call, loading a replacement file
	// and patching the file if needed, or let the game do its own ReadFile Call.
	// You must use the return value of this function as the return value for
	// your breakpoint.
	TH_NODISCARD int replace_ReadFile(x86_reg_t *regs,
									  crypt_func_t decrypt,
									  crypt_func_t crypt);

private:
	void read_from_ReadFile(HANDLE hFile, DWORD fileOffset, DWORD size);

	void replace_ReadFile_init(ReadFileStack *stack,
							   crypt_func_t decrypt,
							   crypt_func_t crypt);
	int replace_ReadFile_write(x86_reg_t *regs, ReadFileStack *stack);
};
