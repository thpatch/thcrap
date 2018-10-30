/**
  * Touhou Community Reliant Automatic Patcher
  * BGM modding library for games using uncompressed PCM music
  *
  * ----
  *
  * Implementations of public functions not related to specific codecs or
  * streaming APIs.
  */

#include <thcrap.h>
#include "bgmmod.hpp"

/// String constants
/// ----------------
const stringref_t LOOP_INFIX = ".loop";
const int LONGEST_CODEC_LEN = [] {
	int ret = 0;
	for(const auto &codec : CODECS) {
		ret = max(ret, codec.ext.len);
	}
	return ret;
}();
/// ----------------

/// Track class
/// -----------
void track_pcm_t::decode(void *_buf, size_t size)
{
	auto *buf = (uint8_t*)_buf;
	assert(buf);
	while(size > 0) {
		auto ret = cur->part_decode(buf, size);
		if(ret == (size_t)-1) {
			// TODO: Error?
			return;
		} else if(ret == 0) {
			if((cur == intro.get()) && (loop != nullptr)) {
				cur = loop.get();
			}
			cur->part_seek_to_sample(0);
		}
		buf += ret;
		size -= ret;
	}
}

void track_pcm_t::seek_to_byte(size_t byte)
{
	auto *actual_looping_segment = loop ? loop.get() : intro.get();
	if(byte < intro->part_bytes) {
		cur = intro.get();
	} else {
		cur = actual_looping_segment;
		auto loop_length = cur->part_bytes;
		byte -= intro->part_bytes;
		byte %= loop_length;
	}
	cur->part_seek_to_sample(byte / (pcmf.bitdepth / 8) / pcmf.channels);
}

std::unique_ptr<track_pcm_t> pcm_open(
	pcm_part_open_t &codec_open, HANDLE &&intro, HANDLE &&loop
)
{
	std::unique_ptr<pcm_part_t> intro_sgm;
	std::unique_ptr<pcm_part_t> loop_sgm;

	intro_sgm = codec_open(std::move(intro));
	if(!intro_sgm) {
		if(loop != INVALID_HANDLE_VALUE) {
			CloseHandle(loop);
		}
		return nullptr;
	}
	if(loop != INVALID_HANDLE_VALUE) {
		loop_sgm = codec_open(std::move(loop));
		if(!loop_sgm) {
			return nullptr;
		}
		if(intro_sgm->pcmf != loop_sgm->pcmf) {
			bgmmod_errorf(
				"PCM format mismatch between intro and loop parts!\n"
				"\xE2\x80\xA2 Intro: %s\n"
				"\xE2\x80\xA2 Loop: %s",
				intro_sgm->pcmf.to_string().str,
				loop_sgm->pcmf.to_string().str
			);
			return nullptr;
		}
	}
	return std::make_unique<track_pcm_t>(
		std::move(intro_sgm), std::move(loop_sgm)
	);
}
/// -----------

/// Codecs
/// ------
std::unique_ptr<pcm_part_t> vorbis_open(HANDLE &&stream);
const codec_t CODECS[1] = {
	".ogg", vorbis_open,
};
/// ------

/// Error reporting and debugging
/// -----------------------------
void bgmmod_verrorf(const char *text, va_list va)
{
	log_vmboxf("BGM modding error", MB_OK | MB_ICONERROR, text, va);
}

void bgmmod_errorf(const char *text, ...)
{
	va_list va;
	va_start(va, text);
	bgmmod_verrorf(text, va);
	va_end(va);
}
/// -----------------------------
