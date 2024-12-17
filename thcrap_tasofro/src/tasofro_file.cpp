/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Helper functions for non-contiguous files
  */

#include <thcrap.h>
#include "tasofro_file.h"

THREAD_LOCAL(TasofroFile*, TasofroFile_tls, nullptr, nullptr);

TasofroFile *TasofroFile::tls_get()
{
	return *TasofroFile_tls_get();
}

void TasofroFile::tls_set(TasofroFile *file)
{
	*TasofroFile_tls_get() = file;
}

TasofroFile::TasofroFile()
{
	// Without this the compiler insists
	// on generating individual stores 
	// to dodge around padding bytes, even
	// when default initializing.
	memset(this, 0, sizeof(TasofroFile));
}

TasofroFile::~TasofroFile()
{
	this->clear();
}

void TasofroFile::init(const char *filename)
{
	file_rep_init(this, filename);

	if (this->rep_buffer && this->patch_size) {
		this->rep_buffer = realloc(this->rep_buffer, POST_JSON_SIZE(this));
	}
}

size_t TasofroFile::init_game_file_size(size_t game_file_size)
{
	if (game_file_size > this->pre_json_size) {
		// The original file is bigger than our replacement file,
		// we might need a bigger buffer.
		this->pre_json_size = game_file_size;
		if (this->rep_buffer) {
			this->rep_buffer = realloc(this->rep_buffer, POST_JSON_SIZE(this));
		}
	}
	return POST_JSON_SIZE(this);
}

void TasofroFile::clear()
{
	if (this->game_buffer) {
		free(this->game_buffer);
	}
	file_rep_clear(this);
	this->offset = 0;
	this->init_done = false;
}

bool TasofroFile::need_replace() const
{
	// If dat_dump is enabled, we always want to run the patching code
	// so that we can dump the files in it
	if unexpected(runconfig_dat_dump_get()) {
		return true;
	}

	// We have a replacement file, we want this one instead of the original one
	if (this->rep_buffer) {
		return true;
	}

	// We have a hook callback, we might return a patched file
	if (this->hooks) {
		return true;
	}

	// Nothing to do, we just let the game read its original file
	// with its own code.
	return false;
}

void TasofroFile::read_from_ReadFile(HANDLE hFile, DWORD fileOffset, DWORD size)
{
	DWORD nbOfBytesRead;

	if (this->game_buffer) {
		free(this->game_buffer);
	}
	this->game_buffer = malloc(size);
	this->pre_json_size = size;

	DWORD oldOffset = SetFilePointer(hFile, fileOffset, NULL, FILE_BEGIN);
	ReadFile(hFile, this->game_buffer, size, &nbOfBytesRead, NULL);
	SetFilePointer(hFile, oldOffset, NULL, FILE_BEGIN);
}

void TasofroFile::replace_ReadFile_init(ReadFileStack *stack,
										crypt_func_t decrypt,
										crypt_func_t crypt)
{
	// In order to get the correct offset, we wait until the 1st read
	// of a file by the game, then remember this offset for future calls.
	this->offset = SetFilePointer(stack->hFile, 0, NULL, FILE_CURRENT);

	// Local variables used to get MSVC to store
	// things in stack locals and registers
	const char* dat_dump = runconfig_dat_dump_get();
	void* rep_buffer = this->rep_buffer;
	size_t pre_json_size = this->pre_json_size;

	// Return true if we need to read the original file, false otherwise.
	auto need_orig_file = [=]() {
		// If dat_dump is enabled, we *always* need the original buffer
		if unexpected(dat_dump) {
			return true;
		}
		// We have a replacement file, no need to read the original one (even if we have
		// hook callbacks, they will be called on the replacement file)
		if (rep_buffer) {
			return false;
		}
		// replace_ReadFile won't be run if the other conditions are false,
		// so have a hook callback. It will need the original file.
		return true;
	};

	if (need_orig_file()) {
		// This buffer is slightly over-allocated
		// to simplify the decrypt code
		void* game_buffer = malloc(AlignUpToMultipleOf2(pre_json_size + this->patch_size, 16));

		// This variable is static just to avoid
		// allocating stack space
		static DWORD nbOfBytesRead;
		ReadFile(stack->hFile, game_buffer, pre_json_size, &nbOfBytesRead, NULL);
		// Rewind the file pointer
		SetFilePointer(stack->hFile, this->offset, NULL, FILE_BEGIN);

		decrypt(this, (BYTE*)game_buffer, pre_json_size);

		if unexpected(dat_dump) {
			DumpDatFile(dat_dump, this->name, game_buffer, pre_json_size, false);
		}
		// If there are hooks and no replacement file
		// then just pass the original file to the
		// hook functions.
		if (!rep_buffer && this->hooks) {
			rep_buffer = this->rep_buffer = game_buffer;
			// MSVC is too dumb to jump over the duplicate
			// condition checks without this goto
			goto run_hooks;
		} else {
			// No patches to apply or rep_buffer is already
			// filled, we won't need this one anymore
			free(game_buffer);
		}
	}
	if (rep_buffer) {
		if (this->hooks) {
run_hooks:
			// Patch the game
			patchhooks_run(this->hooks, rep_buffer, pre_json_size + this->patch_size, pre_json_size, this->name, this->patch);
			// Free the hook array now since they
			// won't be run again anyway
			free(this->hooks);
			this->hooks = NULL;
		}

		crypt(this, (BYTE*)rep_buffer, pre_json_size + this->patch_size);
	}
	this->init_done = true;
}

int TasofroFile::replace_ReadFile_write(x86_reg_t *regs, ReadFileStack *stack)
{
	DWORD offset = SetFilePointer(stack->hFile, 0, NULL, FILE_CURRENT) - this->offset;
	DWORD size = 0;
	ptrdiff_t remaining_size = POST_JSON_SIZE(this) - offset;
	if (remaining_size >= 0) {
		size = stack->nNumberOfBytesToRead;
		size = __min((DWORD)remaining_size, size);
		memcpy(stack->lpBuffer, (BYTE*)this->rep_buffer + offset, size);
		SetFilePointer(stack->hFile, size, NULL, FILE_CURRENT);
	}
	*stack->lpNumberOfBytesRead = size;

	regs->eax = 1;
	// Skip the codecave
	return 0;
}

int TasofroFile::replace_ReadFile(x86_reg_t *regs,
								  crypt_func_t decrypt,
								  crypt_func_t crypt)
{
	ReadFileStack *stack = (ReadFileStack*)(regs->esp + sizeof(void*));

	if unexpected(stack->lpOverlapped) {
		// Overlapped operations are not supported.
		// We'd better leave that file alone rather than ignoring that.
		return 1;
	}

	if (!this->init_done) {
		this->replace_ReadFile_init(stack, decrypt, crypt);
	}

	if unexpected(this->rep_buffer) {
		return this->replace_ReadFile_write(regs, stack);
	}
	return 1;
}
