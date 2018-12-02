/**
  * Touhou Community Reliant Automatic Patcher
  * BGM modding library for games using uncompressed PCM music
  *
  * ----
  *
  * FLAC support.
  */

#include <thcrap.h>
#include "bgmmod.hpp"

#define DR_FLAC_NO_OGG
#define DR_FLAC_NO_STDIO
#define DR_FLAC_IMPLEMENTATION
#include <dr_flac.h>

#define flac_errorf(text, ...) \
	bgmmod_format_errorf("FLAC", text, ##__VA_ARGS__)

/// Callbacks
/// ---------
size_t flac_w32_read(HANDLE stream, void *pBufferOut, size_t bytesToRead)
{
	size_t ret = 0;
	DWORD ReadFile_byte_ret = 0;
	do {
		ReadFile(stream, pBufferOut, bytesToRead, &ReadFile_byte_ret, nullptr);
		bytesToRead -= ReadFile_byte_ret;
		ret += ReadFile_byte_ret;
	} while(bytesToRead > 0 && ReadFile_byte_ret > 0);
	return ret;
}

drflac_bool32 flac_w32_seek(HANDLE stream, int offset, drflac_seek_origin origin)
{
	auto ret = SetFilePointer(
		stream, offset, nullptr, (origin == drflac_seek_origin_current) ? FILE_CURRENT : FILE_BEGIN
	);
	return ret != INVALID_SET_FILE_POINTER;
}
/// ---------

/// dr_flac extensions
/// ------------------
uint8_t drflac_nearest_output_bitdepth(const drflac &pFlac)
{
	return (pFlac.bitsPerSample <= 16) ? 16 : 32;
}

typedef drflac_uint64 drflac_read_func_t(drflac *, drflac_uint64, void *);
/// ------------------

struct flac_part_t : public pcm_part_t {
	drflac *ff;

	size_t part_decode_single(void *buf, size_t size);
	void part_seek_to_sample(size_t sample);

	flac_part_t(drflac *ff, pcm_part_info_t &&info)
		: ff(ff), pcm_part_t(std::move(info)) {
	}
	virtual ~flac_part_t();
};

size_t flac_part_t::part_decode_single(void *buf, size_t size)
{
	drflac_read_func_t *read;
	auto bytedepth = pcmf.bitdepth / 8;
	switch(bytedepth) {
	case 2:
		read = (drflac_read_func_t *)drflac_read_s16;
		break;
	case 4:
		read = (drflac_read_func_t *)drflac_read_s32;
		break;
	default:
		assert(!"(FLAC) Unsupported bit depth?!");
	}

	// dr_flac doesn't return any specific errors in its public APIs, so
	// we'll just rewind in that case by returning 0 here... Fair enough.
	auto ret = (size_t)read(ff, size / bytedepth, (uint8_t *)buf);
	return (ret * bytedepth);
}

void flac_part_t::part_seek_to_sample(size_t sample)
{
	drflac_uint64 channeled_sample = sample * pcmf.channels;
	drflac_seek_to_sample(ff, channeled_sample);
}

flac_part_t::~flac_part_t()
{
	CloseHandle(ff->bs.pUserData);
	drflac_close(ff);
}

std::unique_ptr<pcm_part_t> flac_open(HANDLE &&stream)
{
	drflac *ff = drflac_open(flac_w32_read, flac_w32_seek, stream);

	auto fail = [&] (const char *text) {
		flac_errorf("%s", text);
		CloseHandle(stream);
		drflac_close(ff);
		return nullptr;
	};

	if(!ff) {
		return fail("Invalid FLAC file");
	} else if (ff->totalSampleCount == 0) {
		return fail("File not seekable?");
	}

	auto output_bitdepth = drflac_nearest_output_bitdepth(*ff);
	pcm_format_t pcmf = { ff->sampleRate, output_bitdepth, ff->channels };
	auto byte_size = (size_t)(ff->totalSampleCount * (output_bitdepth / 8));

	return std::make_unique<flac_part_t>(ff, pcm_part_info_t{
		pcmf, byte_size
	});
}
