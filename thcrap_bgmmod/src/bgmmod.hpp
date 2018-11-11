/**
  * Touhou Community Reliant Automatic Patcher
  * BGM modding library for games using uncompressed PCM music
  *
  * ----
  *
  * Header file, covering all source files.
  */

#pragma once

#include <memory>

/// String constants
/// ----------------
extern const stringref_t LOOP_INFIX;
extern const int LONGEST_CODEC_LEN;
/// ----------------

/// Sampling rate / bit depth / channel structure
/// ---------------------------------------------
struct pcm_format_t {
	uint32_t samplingrate;
	uint16_t bitdepth;
	uint16_t channels;
};
/// ---------------------------------------------

/// Track class
/// -----------
struct track_t {
	const pcm_format_t pcmf;
	// Total size of the track, in decoded PCM bytes according to [pcmf].
	const size_t total_size;

	// *Always* has to fill the buffer entirely, if necessary by looping back.
	virtual void decode(void *buf, size_t size) = 0;
	// Seeks to the raw decoded audio byte, according to [pcmf].
	// Can get arbitrarily large, therefore the looping section
	// should be treated as infinitely long.
	virtual void seek_to_byte(size_t byte) = 0;

	track_t(const pcm_format_t &pcmf, const size_t &total_size)
		: pcmf(pcmf), total_size(total_size) {
	}

	virtual ~track_t() {}
};
/// -----------

/// Codecs
/// ------
struct codec_t {
	stringref_t ext;
	std::unique_ptr<track_t> (*open)(HANDLE &&intro, HANDLE &&loop);
};
/// ------

/// Error reporting and debugging
/// -----------------------------
void bgmmod_verrorf(const char *text, va_list va);
void bgmmod_errorf(const char *text, ...);
#define bgmmod_debugf(text, ...) \
	log_debugf("[BGM] " text, ##__VA_ARGS__)
// TODO: Filename?
#define bgmmod_format_errorf(format, text, ...) \
	bgmmod_errorf("%s" text, "(" format ") ", ##__VA_ARGS__)
/// -----------------------------
