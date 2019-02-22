/**
  * Touhou Community Reliant Automatic Patcher
  * BGM modding library for games using uncompressed PCM music
  *
  * ----
  *
  * MP3 support.
  */

#include <thcrap.h>
#include "bgmmod.hpp"

#include <libmpg123/ports/MSVC++/mpg123.h>

#pragma comment(lib, "libmpg123" DEBUG_OR_RELEASE)

auto mp3_l = bgmmod_format_log("MP3");

/// Callbacks
/// ---------
ssize_t mpg123_w32_read(HANDLE file, void *buf, size_t size)
{
	DWORD byte_ret;
	ReadFile(file, buf, size, &byte_ret, nullptr);
	return byte_ret;
}

off_t mpg123_w32_lseek(HANDLE file, off_t offset, int whence)
{
	return SetFilePointer(file, offset, nullptr, whence);
}

void mpg123_w32_cleanup(HANDLE file)
{
	CloseHandle(file);
}
/// ---------

struct mp3_part_t : public pcm_part_t {
	bool decode_done = false;
	mpg123_handle *mh;

	size_t part_decode_single(void *buf, size_t size);
	void part_seek_to_sample(size_t sample);

	mp3_part_t(mpg123_handle *mh, pcm_part_info_t &&info)
		: mh(mh), pcm_part_t(std::move(info)) {
	}
	virtual ~mp3_part_t();
};

size_t mp3_part_t::part_decode_single(void *buf, size_t size)
{
	if(decode_done) {
		decode_done = false;
		return 0;
	}
	size_t byte_ret;
	auto ret = mpg123_read(mh, (uint8_t *)buf, size, &byte_ret);
	if(ret == MPG123_OK) {
	} else if(ret == MPG123_DONE) {
		decode_done = true;
	} else {
		mp3_l.errorf("%s", mpg123_plain_strerror(ret));
		return -1;
	}
	return byte_ret;
}

void mp3_part_t::part_seek_to_sample(size_t sample)
{
	auto seek_ret = mpg123_seek(mh, sample, SEEK_SET);
	assert(seek_ret >= 0);
}

mp3_part_t::~mp3_part_t()
{
	mpg123_delete(mh);
}

std::unique_ptr<pcm_part_t> mp3_open(HANDLE &&stream)
{
	mpg123_handle *mh = nullptr;
	int err;

	auto fail = [&] (const char* text, int err) {
		va_list va;
		va_start(va, text);
		if(err != MPG123_OK) {
			auto err_str = mpg123_plain_strerror(err);
			mp3_l.errorf("%s: %s", text, err_str);
		} else {
			mp3_l.errorf("%s", text);
		}
		va_end(va);
		if(mh) {
			mpg123_delete(mh);
		} else {
			CloseHandle(stream);
		}
		return nullptr;
	};

	auto init_ret = mpg123_init();
	if(init_ret != MPG123_OK) {
		static bool mpg123_init_failure_shown = false;
		if(!mpg123_init_failure_shown) {
			fail("Error initializing libmpg123", init_ret);
			mpg123_init_failure_shown = true;
		}
		return nullptr;
	}

	mh = mpg123_new(nullptr, &err);
	if(err != MPG123_OK) {
		return fail("Error creating mpg123 handle", err);
	}
	mpg123_replace_reader_handle(mh,
		mpg123_w32_read, mpg123_w32_lseek, mpg123_w32_cleanup
	);
	err = mpg123_open_handle(mh, stream);
	if(err != MPG123_OK) {
		return fail("Error opening file", err);
	}

	long samplerate = 0;
	int channels = 0;
	int encoding = 0;
	err = mpg123_getformat(mh, &samplerate, &channels, &encoding);
	if(err != MPG123_OK) {
		return fail("Error initializing file", err);
	}

	auto sample_length = mpg123_length(mh);
	if(sample_length <= 0) {
		return fail("File not seekable?", MPG123_OK);
	}

	auto bytedepth = MPG123_SAMPLESIZE(encoding);
	size_t byte_size = sample_length * bytedepth * channels;
	pcm_format_t pcmf = { (uint32_t)samplerate, bytedepth * 8, channels };

	return std::make_unique<mp3_part_t>(mh, pcm_part_info_t{
		pcmf, byte_size
	});
}
