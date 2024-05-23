#include "compiler_support.h"

/*
 * This file is compiled in GCC in C mode - where __seg_fs/__seg_gs works.
 * So we'll juts use our C macro.
 */

uint8_t read_fs_byte_gcc(size_t offset)
{
	return read_fs_byte(offset);
}

uint16_t read_fs_word_gcc(size_t offset)
{
	return read_fs_word(offset);
}

uint32_t read_fs_dword_gcc(size_t offset)
{
	return read_fs_dword(offset);
}

uint64_t read_fs_qword_gcc(size_t offset) {
	return read_fs_qword(offset);
}

void write_fs_byte_gcc(size_t offset, uint8_t data)
{
	write_fs_byte(offset, data);
}

void write_fs_word_gcc(size_t offset, uint16_t data)
{
	write_fs_word(offset, data);
}

void write_fs_dword_gcc(size_t offset, uint32_t data)
{
	write_fs_dword(offset, data);
}

void write_fs_qword_gcc(size_t offset, uint64_t data) {
	write_fs_qword(offset, data);
}

uint8_t read_gs_byte_gcc(size_t offset)
{
	return read_gs_byte(offset);
}

uint16_t read_gs_word_gcc(size_t offset)
{
	return read_gs_word(offset);
}

uint32_t read_gs_dword_gcc(size_t offset)
{
	return read_gs_dword(offset);
}

uint64_t read_gs_qword_gcc(size_t offset)
{
	return read_gs_qword(offset);
}

void write_gs_byte_gcc(size_t offset, uint8_t data)
{
	write_gs_byte(offset, data);
}

void write_gs_word_gcc(size_t offset, uint16_t data)
{
	write_gs_word(offset, data);
}

void write_gs_dword_gcc(size_t offset, uint32_t data)
{
	write_gs_dword(offset, data);
}

void write_gs_qword_gcc(size_t offset, uint64_t data)
{
	write_gs_qword(offset, data);
}

