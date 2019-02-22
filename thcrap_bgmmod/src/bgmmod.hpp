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
/// ----------------

/// Sampling rate / bit depth / channel structure
/// ---------------------------------------------
#define PCM_FORMAT_DESC_FMT "%u Hz / %u Bit / %uch"

struct pcm_format_t {
	uint32_t samplingrate;
	uint16_t bitdepth;
	uint16_t channels;

	bool operator ==(const pcm_format_t &other) const {
		return samplingrate == other.samplingrate
			&& bitdepth == other.bitdepth
			&& channels == other.channels;
	}

	bool operator !=(const pcm_format_t &other) const {
		return !(*this == other);
	}

	// "Function returning array is not allowed", so...
	struct desc_t {
		char str[
			DECIMAL_DIGITS_BOUND(decltype(samplingrate)) - 2
			+ DECIMAL_DIGITS_BOUND(decltype(bitdepth)) - 2
			+ DECIMAL_DIGITS_BOUND(decltype(channels)) - 2
			+ sizeof(PCM_FORMAT_DESC_FMT)
		];
		size_t len;
	} to_string() const
	{
		desc_t ret;
		ret.len = snprintf(
			ret.str, sizeof(ret.str), PCM_FORMAT_DESC_FMT,
			samplingrate, bitdepth, channels
		);
		return ret;
	}

	// Writes this format to the given variety of a WAVEFORMAT* structure.
	void patch(WAVEFORMATEX &wfx) const;
	void patch(PCMWAVEFORMAT &pwf) const;
};
/// ---------------------------------------------

/// Track class
/// -----------
struct track_t {
	const pcm_format_t pcmf;
	// Sizes are in decoded PCM bytes according to [pcmf].
	const size_t intro_size;
	const size_t total_size;

	// Single decoding call that also handles looping. Should return
	// the number of bytes actually decoded, which can be less than
	// [size]. If an error occured, it should return -1, and show a
	// message box.
	virtual size_t decode_single(void *buf, size_t size) = 0;
	// Seeks to the raw decoded audio byte, according to [pcmf].
	// Can get arbitrarily large, therefore the looping section
	// should be treated as infinitely long.
	virtual void seek_to_byte(size_t byte) = 0;

	// *Always* fills [buf] entirely. Returns true if successful,
	// or false in case of an unrecoverable decoding error, in
	// which case the buffer is filled with zeroes.
	bool decode(void *buf, size_t size);

	track_t(
		const pcm_format_t &pcmf,
		const size_t &intro_size, const size_t &total_size
	)
		: pcmf(pcmf), intro_size(intro_size), total_size(total_size) {
	}

	virtual ~track_t() {}
};

struct pcm_part_info_t {
	const pcm_format_t pcmf;
	// Size of this part, in decoded PCM bytes according to [pcmf].
	const size_t part_bytes;
};

// Base class for an individual intro or loop file.
// Should be derived for each supported codec.
struct pcm_part_t : public pcm_part_info_t {
	// Single decoding call. Should return the number of bytes actually
	// decoded, which can be less than [size]. If an error occured, it
	// should return -1, and show a message box.
	virtual size_t part_decode_single(void *buf, size_t size) = 0;
	// Seeks to the raw decoded, non-interleaved audio sample (byte
	// divided by bitdepth and number of channels). Guaranteed to be
	// less than the total number of samples in the stream.
	virtual void part_seek_to_sample(size_t sample) = 0;

	pcm_part_t(pcm_part_info_t &&info)
		: pcm_part_info_t(info) {
	}
	virtual ~pcm_part_t() {}
};

// Generic implementation for PCM-based codecs, with separate intro and
// loop files.
struct track_pcm_t : public track_t {
	std::unique_ptr<pcm_part_t> intro;
	std::unique_ptr<pcm_part_t> loop;
	pcm_part_t *cur;

	/// track_t functions
	size_t decode_single(void *buf, size_t size);
	void seek_to_byte(size_t byte);

	track_pcm_t(
		std::unique_ptr<pcm_part_t> &&intro_in,
		std::unique_ptr<pcm_part_t> &&loop_in
	) : track_t(
		intro_in->pcmf,
		loop_in ? intro_in->part_bytes : 0,
		intro_in->part_bytes + (loop_in ? loop_in->part_bytes : 0)
	) {
		intro = std::move(intro_in);
		loop = std::move(loop_in);
		cur = intro.get();
	}
	virtual ~track_pcm_t() {}
};

// Opens [stream] as a part of a modded track, using a specific codec.
// Should show a message box if [stream] is not a valid file for this codec.
typedef std::unique_ptr<pcm_part_t> pcm_part_open_t(HANDLE &&stream);

std::unique_ptr<track_pcm_t> pcm_open(
	pcm_part_open_t &codec_open, HANDLE &&intro, HANDLE &&loop
);
/// -----------

/// Codecs
/// ------
struct codec_t {
	stringref_t ext;
	pcm_part_open_t &open;
};

extern const codec_t CODECS[3];
/// ------

/// Stack file resolution
/// ---------------------
std::unique_ptr<track_t> stack_bgm_resolve(const stringref_t &basename);
/// ---------------------

/// Streaming APIs
/// --------------
// MMIO implementation
// -------------------
namespace mmio
{
	// Initializes BGM modding for the MMIO API.
	// No module function, has to be called manually.
	int detour(void);

	bool is_modded_handle(HMMIO hmmio);
}
// -------------------
/// --------------

/// Error reporting and debugging
/// -----------------------------
extern logger_t bgmmod_log;

#define bgmmod_debugf(text, ...) \
	log_debugf("[BGM] " text, ##__VA_ARGS__)
// TODO: Filename?
#define bgmmod_format_log(format) \
	bgmmod_log.prefixed("(" format ") ")
/// -----------------------------
