/**
  * Touhou Community Reliant Automatic Patcher
  * BGM modding library for games using uncompressed PCM music
  *
  * ----
  *
  * Ogg Vorbis support.
  */

#include <thcrap.h>
#include "bgmmod.hpp"

#define OV_EXCLUDE_STATIC_CALLBACKS
#include <vorbis/vorbisfile.h>

#pragma comment(lib, "libogg" FILE_SUFFIX)
#pragma comment(lib, "libvorbis" FILE_SUFFIX)

#define VORBIS_ERROR(...) BGMMOD_BASE_ERROR("Ogg Vorbis ", __VA_ARGS__)

/// Callbacks
/// ---------
size_t ov_w32_read(void* dst, size_t elmsize, size_t count, HANDLE file)
{
	DWORD byte_ret;
	ReadFile(file, dst, elmsize * count, &byte_ret, nullptr);
	return byte_ret;
}

int ov_w32_seek(HANDLE file, ogg_int64_t bytes, int org)
{
	return SetFilePointer(file, (LONG)bytes, nullptr, org) != 0xFFFFFFFF;
}

int ov_w32_close(HANDLE file)
{
	return CloseHandle(file);
}

long ov_w32_tell(HANDLE file)
{
	return SetFilePointer(file, 0, 0, FILE_CURRENT);
}

const ov_callbacks OV_CALLBACKS_WIN32 = {
	ov_w32_read, ov_w32_seek, ov_w32_close, ov_w32_tell
};
/// ---------

/// libvorbis extensions
/// --------------------
const char* ov_strerror(int ret)
{
	// Adapted from doc/libvorbis/return.html.
	switch(ret) {
	// Despite what the documentation says, this one actually *can* be
	// unrecoverable.
	case OV_HOLE: return "Missing or corrupt data in the bitstream";
	case OV_EREAD: return "Read error while fetching compressed data for decode";
	case OV_EFAULT: return "Internal inconsistency in encode or decode state. Continuing is likely not possible.";
	case OV_EIMPL: return "Feature not implemented";
	case OV_EINVAL: return "Invalid argument was passed to a call";
	case OV_ENOTVORBIS: return "The given file was not recognized as Ogg Vorbis data.";
	case OV_EBADHEADER: return "The file is apparently an Ogg Vorbis stream, but contains a corrupted or undecipherable header.";
	case OV_EVERSION: return "The bitstream format revision of the given file is not supported.";
	case OV_EBADLINK:return "The given link exists in the Vorbis data stream, but is not decipherable due to garbage or corruption.";
	case OV_ENOSEEK: return "File is not seekable";
	default:
		return "";
	}
}
/// --------------------

struct vorbis_part_t : public pcm_part_t {
	OggVorbis_File vf;

	size_t part_decode_single(void *buf, size_t size);
	void part_seek_to_sample(size_t sample);

	vorbis_part_t(OggVorbis_File &&vf, pcm_part_info_t &&info)
		: vf(vf), pcm_part_t(std::move(info)) {
	};
	virtual ~vorbis_part_t();
};

size_t vorbis_part_t::part_decode_single(void *buf, size_t size)
{
	auto ret = ov_read(&vf, (char *)buf, size, 0, 2, 1, nullptr);
	if(ret < 0) {
		VORBIS_ERROR(
			"Error %d at sample %lld: %s",
			ret, ov_pcm_tell(&vf), ov_strerror(ret)
		);
		return -1;
	}
	return ret;
}

void vorbis_part_t::part_seek_to_sample(size_t sample)
{
	auto ret = ov_pcm_seek(&vf, sample);
	assert(ret == 0);
}

vorbis_part_t::~vorbis_part_t()
{
	ov_clear(&vf);
}

std::unique_ptr<pcm_part_t> vorbis_open(HANDLE &&stream)
{
	OggVorbis_File vf = { 0 };

	auto fail = [&] (int ret) {
		VORBIS_ERROR("%s", ov_strerror(ret));
		ov_clear(&vf);
		return nullptr;
	};

	auto ret = ov_open_callbacks(stream, &vf, nullptr, 0, OV_CALLBACKS_WIN32);
	if(ret) {
		return fail(ret);
	}

	auto sample_length = ov_pcm_total(&vf, -1);
	if(sample_length < 0) {
		// The regularly returned error would be OV_EINVAL...
		return fail(OV_ENOSEEK);
	}

	assert(vf.vi);

	pcm_format_t pcmf{ (uint32_t)vf.vi->rate, 16, (uint16_t)vf.vi->channels };
	auto bits_per_sample = (pcmf.bitdepth / 8) * pcmf.channels;
	auto byte_size = (size_t)(sample_length * bits_per_sample);

	return std::make_unique<vorbis_part_t>(std::move(vf), pcm_part_info_t{
		pcmf, byte_size
	});
}
