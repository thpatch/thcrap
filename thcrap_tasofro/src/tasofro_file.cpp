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
	: init_done(false)
{
	memset(this, 0, sizeof(file_rep_t));
	file_rep_clear(this);
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
		SAFE_FREE(this->game_buffer);
	}
	file_rep_clear(this);
	this->offset = 0;
	this->init_done = false;
}

bool TasofroFile::need_orig_file()
{
	// If dat_dump is enabled, we *always* need the original buffer
	const char *dat_dump = runconfig_dat_dump_get();
	if (dat_dump) {
		return true;
	}

	// We have a replacement file, no need to read the original one (even if we have
	// hook callbacks, they will be called on the replacement file)
	if (this->rep_buffer) {
		return false;
	}

	// We have a hook callback. It will need the original file.
	if (this->hooks) {
		return true;
	}

	// Nothing to do, we just let the game read its original file
	// with its own code.
	return false;
}

bool TasofroFile::need_replace()
{
	// If dat_dump is enabled, we always want to run the patching code
	// so that we can dump the files in it
	const char *dat_dump = runconfig_dat_dump_get();
	if (dat_dump) {
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

	SAFE_FREE(this->game_buffer);
	this->game_buffer = malloc(size);
	this->pre_json_size = size;

	DWORD oldOffset = SetFilePointer(hFile, fileOffset, NULL, FILE_BEGIN);
	ReadFile(hFile, this->game_buffer, size, &nbOfBytesRead, NULL);
	SetFilePointer(hFile, oldOffset, NULL, FILE_BEGIN);
}

void TasofroFile::init_buffer()
{
	const char *dat_dump = runconfig_dat_dump_get();
	if (dat_dump) {
		DumpDatFile(dat_dump, this->name, this->game_buffer, this->pre_json_size);
	}

	if (!this->hooks) {
		// No patches to apply
		SAFE_FREE(this->game_buffer);
		return ;
	}

	if (!this->rep_buffer) {
		this->rep_buffer = malloc(POST_JSON_SIZE(this));
		memcpy(this->rep_buffer, this->game_buffer, this->pre_json_size);
	}
	// We filled rep_buffer, we won't need this one anymore
	SAFE_FREE(this->game_buffer);

	// Patch the game
	if (!patchhooks_run(this->hooks, this->rep_buffer, POST_JSON_SIZE(this), this->pre_json_size, this->name, this->patch)) {
		// Remove the hooks so that next calls won't try to patch the file.
		// This is only an optimization, avoiding to re-read the file, but this
		// optimization is required as long as we don't detect open/close calls
		// and try to re-open a file on every read call.
		SAFE_FREE(this->hooks);
	}
}

void TasofroFile::replace_ReadFile_init(ReadFileStack *stack,
	std::function<void (TasofroFile *fr, BYTE *buffer, DWORD size)>& decrypt,
	std::function<void (TasofroFile *fr, BYTE *buffer, DWORD size)>& crypt)
{
	// In order to get the correct offset, we wait until the 1st read
	// of a file by the game, then remember this offset for future calls.
	this->offset = SetFilePointer(stack->hFile, 0, nullptr, FILE_CURRENT);

	if (this->need_orig_file()) {
		this->read_from_ReadFile(stack->hFile, this->offset, this->pre_json_size);
		decrypt(this, (BYTE*)this->game_buffer, this->pre_json_size);
	}
	this->init_buffer();
	if (this->rep_buffer) {
		crypt(this, (BYTE*)this->rep_buffer, POST_JSON_SIZE(this));
	}
	this->init_done = true;
}

int TasofroFile::replace_ReadFile_write(x86_reg_t *regs, ReadFileStack *stack)
{
	DWORD offset = SetFilePointer(stack->hFile, 0, NULL, FILE_CURRENT) - this->offset;
	DWORD offset_ = SetFilePointer(stack->hFile, 0, NULL, FILE_CURRENT);
	DWORD size;
	if (offset <= POST_JSON_SIZE(this)) {
		size = MIN(POST_JSON_SIZE(this) - offset, stack->nNumberOfBytesToRead);
	}
	else {
		size = 0;
	}

	memcpy(stack->lpBuffer, (BYTE*)this->rep_buffer + offset, size);
	SetFilePointer(stack->hFile, size, NULL, FILE_CURRENT);
	*stack->lpNumberOfBytesRead = size;

	regs->eax = 1;
	regs->esp += 5 * sizeof(DWORD);
	// Skip the codecave
	return 0;
}

int TasofroFile::replace_ReadFile(x86_reg_t *regs,
	std::function<void (TasofroFile *fr, BYTE *buffer, DWORD size)> decrypt,
	std::function<void (TasofroFile *fr, BYTE *buffer, DWORD size)> crypt)
{
	ReadFileStack *stack = (ReadFileStack*)(regs->esp + sizeof(void*));

	if (stack->lpOverlapped) {
		// Overlapped operations are not supported.
		// We'd better leave that file alone rather than ignoring that.
		return 1;
	}

	if (!this->init_done) {
		this->replace_ReadFile_init(stack, decrypt, crypt);
	}

	int ret = 1;
	if (this->rep_buffer) {
		ret = this->replace_ReadFile_write(regs, stack);
	}
	return ret;
}
