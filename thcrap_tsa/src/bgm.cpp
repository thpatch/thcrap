/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * BGM patching and related bugfixes.
  */

#include <thcrap.h>
extern "C" {
# include <thtypes/bgm_types.h>
# include "thcrap_tsa.h"
}
#include <thcrap_bgmmod/src/bgmmod.hpp>
#include <algorithm>
#include <vector>

#pragma comment(lib, "thcrap_bgmmod" DEBUG_OR_RELEASE)

const stringref_t LOOPMOD_FN = "loops.js";

// The game's own thbgm.fmt structure. Necessary for mapping seek positions
// back to file names for BGM modding, and repatchability of loop point
// modding.
bgm_fmt_t* bgm_fmt = nullptr;

// Indices are more useful for BGM modding, since we also need to index into
// thbgm_mods[]. Only finds a track that starts exactly at [offset]; to
// support TH13-style "trance" seeks, see the `bgmmod_tranceseek_byte_offset`
// breakpoint.
template<typename F> int _bgm_find(F condition)
{
	assert(bgm_fmt);
	auto track = bgm_fmt;
	while(track->fn[0] != '\0') {
		if(condition(*track)) {
			return track - bgm_fmt;
		}
		track++;
	}
	return -1;
}

int bgm_find(uint32_t offset)
{
	return _bgm_find([offset] (const bgm_fmt_t &track) {
		return offset == track.track_offset;
	});
}

int bgm_find(stringref_t fn)
{
	assert(bgm_fmt);
	if(fn.len > sizeof(bgm_fmt->fn)) {
		return -1;
	}
	return _bgm_find([fn] (const bgm_fmt_t &track) {
		return !strncmp(track.fn, fn.str, fn.len);
	});
};

/// BGM modding weirdness for TH06
/// ==============================
// When setting up new BGM, the game always creates new streaming threads and
// events with fitting intervals calculated from the PCM format of the BGM
// file. Which is good and correct (and later games should have done it too),
// *except* that TH06 retrieves the format by creating a new instance of its
// CWaveFile class, whose constructor immediately fills the streaming buffer
// with the first 4 seconds of the BGM... only to throw away that instance at
// the end of the function and later create a new one, used for the actual
// streaming. Since this needlessly decodes 4 seconds of sound, let's abuse
// the fact that the corresponding .pos file is only loaded immediately before
// actual streaming begins, and lock decoding from mmioOpen() up to our .pos
// hook. Much nicer than what the Touhou Vorbis Compressor had to do.
bool th06_decode_lock = true;
bool th06_bgmmod_active = true;

static MMRESULT (WINAPI *chain_mmioAdvance)(HMMIO, LPMMIOINFO, UINT);
static HMMIO (WINAPI *chain_mmioOpenA)(LPSTR, LPMMIOINFO, DWORD);

MMRESULT WINAPI th06_mmioAdvance(HMMIO hmmio, LPMMIOINFO pmmioinfo, UINT fuAdvance)
{
	static bool th06_decode_lock_prev = th06_decode_lock;
	// It might seem that unmodded .wav playback would benefit from
	// this too, but if we do this, one of the game's mmioDescend()
	// calls in CWaveFile::ResetFile() will fail...
	if(th06_bgmmod_active && th06_decode_lock) {
		pmmioinfo->pchNext = pmmioinfo->pchBuffer;
		if(th06_decode_lock_prev == false) {
			ZeroMemory(pmmioinfo->pchBuffer, pmmioinfo->cchBuffer);
		}
		th06_decode_lock_prev = true;
		return MMSYSERR_NOERROR;
	};
	th06_decode_lock_prev = th06_decode_lock;
	return chain_mmioAdvance(hmmio, pmmioinfo, fuAdvance);
}

HMMIO WINAPI th06_mmioOpenA(LPSTR pszFileName, LPMMIOINFO pmmioinfo, DWORD fdwOpen)
{
	th06_decode_lock = true;
	auto ret = chain_mmioOpenA(pszFileName, pmmioinfo, fdwOpen);
	th06_bgmmod_active = mmio::is_modded_handle(ret);
	return ret;
}
/// ==============================

/// BGM modding for ≥TH07
/// =====================
// Let's be correct, and support more than one handle to thbgm.dat.
std::vector<HANDLE> thbgm_handles;
std::unique_ptr<std::unique_ptr<track_t>[]> thbgm_mods;
// Index into both [thbgm_mods] and the bgm_fmt_t array.
// Negative if no track is playing.
int thbgm_cur_bgmid = -1;
size_t thbgm_modtrack_bytes_read = 0;

static auto chain_CreateFileA = CreateFileU;
static auto chain_CloseHandle = CloseHandle;
static auto chain_ReadFile = ReadFile;
static auto chain_SetFilePointer = SetFilePointer;

track_t* thbgm_modtrack_for_current_bgmid()
{
	return (thbgm_cur_bgmid >= 0)
		? thbgm_mods[thbgm_cur_bgmid].get()
		: nullptr;
}

// "Trance seeking" support
// ------------------------
// Pretty much the entire challenge of ≥TH07: For the BGM switches that occur
// when entering and leaving trance mode in TH13, SetFilePointer() only ever
// sees the *sum* of the track start position and the offset into the track,
// coming from the game's own internal timer.
// Since we obviously want BGM to be moddable on a per-track basis, and allow
// modded BGM to be longer (and larger) than the originals, this would
// necessitate mapping the start positions of modded BGM in the thbgm.fmt
// structure to the "empty space after the end of thbgm.dat", thereby imposing
// a theoretical limit on BGM length. Sure, not too nice, but also not to big
// of a deal in practice.
//
// The real problem occurs when non-trance and trance tracks have different
// lengths. In that case, the aforementioned sum still ends up pointing into
// an unintended, different track. What should we do then? Calculate the
// maximum track length among the modded tracks and make sure that all of them
// occupy that virtual amount of space after the end of thbgm.dat? Then add
// further game-specific knowledge to only build the maximum for each non-
// trance / trance track pair? Sure, but… eh, that's limits on top of limits.
//
// So, let's just capture that offset into the track separately, and subtract
// it for identifying the track. And with the game keeping its own timer and
// the decoding layer being supposed to extend the looping section infinitely,
// we can have our different lengths too.
//
// And sure, breakpoints always seem like a maintenance burden at first, but
// this is way cleaner than any alternative I could come up with. Requires no
// game-specific branches in this module, and doesn't introduce limits where
// there previously weren't any.
size_t tranceseek_offset;

int BP_bgmmod_tranceseek_byte_offset(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	auto offset_j = json_object_get(bp_info, "offset");
	assert(offset_j);
	// ----------
	tranceseek_offset = json_immediate_value(offset_j, regs);
	return 1;
}
// ------------------------

bool is_bgm_handle(HANDLE hFile)
{
	for(const auto &h : thbgm_handles) {
		if(h == hFile) {
			return true;
		}
	}
	return false;
}

HANDLE WINAPI thbgm_CreateFileA(
	LPCSTR lpFileName,
	DWORD dwDesiredAccess,
	DWORD dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	DWORD dwCreationDisposition,
	DWORD dwFlagsAndAttributes,
	HANDLE hTemplateFile
)
{
	auto ret = chain_CreateFileA(
		lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
		dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile
	);
	if(PathMatchSpecU(PathFindFileNameU(lpFileName), "*bgm*.dat")) {
		thbgm_handles.emplace_back(ret);
		bgmmod_debugf("CreateFileA(%s) -> %p\n", lpFileName, ret);
	}
	return ret;
}

BOOL WINAPI thbgm_CloseHandle(
	HANDLE hObject
)
{
	auto elm = std::find(thbgm_handles.begin(), thbgm_handles.end(), hObject);
	if(elm != thbgm_handles.end()) {
		thbgm_handles.erase(elm);
		bgmmod_debugf("CloseHandle(%p)\n", hObject);
	}
	return chain_CloseHandle(hObject);
}

BOOL WINAPI thbgm_ReadFile(
	HANDLE hFile,
	LPVOID lpBuffer,
	DWORD nNumberOfBytesToRead,
	LPDWORD lpNumberOfBytesRead,
	LPOVERLAPPED lpOverlapped
)
{
	if(is_bgm_handle(hFile)) {
		auto *modtrack = thbgm_modtrack_for_current_bgmid();
		if(modtrack) {
			bgmmod_debugf("ReadFile(%p, %u)\n", hFile, nNumberOfBytesToRead);
			if(!modtrack->decode(lpBuffer, nNumberOfBytesToRead)) {
				thbgm_mods[thbgm_cur_bgmid] = nullptr;
			}
			if(lpNumberOfBytesRead) {
				*lpNumberOfBytesRead = nNumberOfBytesToRead;
			}
			thbgm_modtrack_bytes_read += nNumberOfBytesToRead;
			return true;
		}
	}
	return chain_ReadFile(
		hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped
	);
}

DWORD WINAPI thbgm_SetFilePointer(
	HANDLE hFile,
	LONG lDistanceToMove,
	PLONG lpDistanceToMoveHigh,
	DWORD dwMoveMethod
)
{
	static bool seekblock = false;

	auto fallback = [&] () {
		return chain_SetFilePointer(
			hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod
		);
	};

	if(!is_bgm_handle(hFile) || !bgm_fmt) {
		return fallback();
	}
	bgmmod_debugf("SetFilePointer(%p, %d, %u) -> ", hFile, lDistanceToMove, dwMoveMethod);

	bool is_tell = (lDistanceToMove == 0 && dwMoveMethod == FILE_CURRENT);
	if(is_tell) {
		log_debugf("Tell\n");
	}

	// Make sure to always consume the trance seek offset!
	auto track_pos = lDistanceToMove - tranceseek_offset;
	tranceseek_offset = 0;

	// When pausing ≥TH11, those games retrieve the current BGM file
	// position in order to then CloseHandle() thbgm.dat, re-open it once
	// the player resumes the game, then call SetFilePointer() twice,
	// first to seek to the *beginning* of the track, then to seek to the
	// previously retrieved position.
	// The games actually update their internal track progress variable
	// used for looping with the value returned here, so we do have to
	// wrap it correctly to avoid loop glitches.
	// So, let's block that first, rewinding seek, as well as the
	// following attempt to re-seek the file to the same position, before
	// handling anything else.
	// Luckily, TH13's trance seeks are already handled by capturing the
	// offset, so this causes no further problems even if if our modded
	// BGM as longer than the original, and we can still differentiate
	// that seek from a "switch" to somewhere inside one of the following
	// tracks in thbgm.dat.
	auto *modtrack = thbgm_modtrack_for_current_bgmid();
	if(modtrack != nullptr) {
		const auto &fmt = bgm_fmt[thbgm_cur_bgmid];
		auto cur_thbgm_pos = fmt.track_offset + thbgm_modtrack_bytes_read;
		if(is_tell) {
			seekblock = true;
			return cur_thbgm_pos;
		} else if(
			dwMoveMethod == FILE_BEGIN
			&& lDistanceToMove == cur_thbgm_pos
		) {
			log_debugf("Ignored seek-after-tell\n");
			return lDistanceToMove;
		}
	}

	auto new_bgmid = bgm_find(track_pos);
	if(new_bgmid < 0) {
		new_bgmid = thbgm_cur_bgmid;
	}
	defer({ thbgm_cur_bgmid = new_bgmid; });

	auto *new_modtrack = thbgm_mods[new_bgmid].get();
	auto &new_fmt = bgm_fmt[new_bgmid];
	if(new_modtrack) {
		auto seek_offset = lDistanceToMove - new_fmt.track_offset;
		if(new_bgmid != thbgm_cur_bgmid) {
			log_debugf("Track switch, seek to byte #%u\n", seek_offset);
		} else if(seekblock) {
			log_debugf("Blocked seek attempt to byte #%u\n", seek_offset);
			seekblock = false;
			return lDistanceToMove;
		} else if(seek_offset == 0) {
			log_debugf("Rewind\n");
		} else if(seek_offset == new_modtrack->intro_size) {
			log_debugf("Loop\n");
		} else {
			log_debugf("Seek to byte #%u\n", seek_offset);
		}
		thbgm_modtrack_bytes_read = seek_offset;
		new_modtrack->seek_to_byte(seek_offset);
		return lDistanceToMove;
	}
	log_debugf("Unmodded seek\n");
	return fallback();
}
/// =====================

int loopmod_fmt()
{
	int modded = 0;
	auto mod = [] (stringref_t track_name, const json_t* track_mod) {
		auto track_id = bgm_find(track_name);
		if(track_id == -1) {
			// Trials don't have all the tracks, better to not show a
			// message box for them.
			if(!game_is_trial()) {
				log_mboxf(nullptr, MB_OK | MB_ICONEXCLAMATION,
					"Error applying %s: Track \"%s\" does not exist.",
					LOOPMOD_FN.str, track_name.str
				);
			} else {
				log_printf(
					"Error applying %s: Track \"%s\" does not exist.",
					LOOPMOD_FN.str, track_name.str
				);
			}
			return false;
		} else if(thbgm_mods && thbgm_mods[track_id]) {
			log_printf(
				"[BGM] [Loopmod] Ignoring %s due to active BGM mod for the same track\n",
				track_name
			);
			return false;
		};

		auto *track = bgm_fmt + track_id;
		auto sample_size = track->wfx.nChannels * (track->wfx.wBitsPerSample / 8);
		auto *loop_start = json_object_get(track_mod, "loop_start");
		auto *loop_end = json_object_get(track_mod, "loop_end");

		if(loop_start || loop_end) {
			log_printf("[BGM] [Loopmod] Changing %s\n", track_name.str);
		}
		if(loop_start) {
			uint32_t val = (uint32_t)json_integer_value(loop_start);
			track->intro_size = val * sample_size;
		}
		if(loop_end) {
			uint32_t val = (uint32_t)json_integer_value(loop_end);
			track->track_size = val * sample_size;
		}
		return true;
	};

	// Looping this way makes error reporting a bit easier.
	json_t *loops = jsondata_game_get(LOOPMOD_FN.str);
	const char *key;
	const json_t *track_mod;
	json_object_foreach(loops, key, track_mod) {
		modded += mod(key, track_mod);
	}
	json_decref(loops);
	return modded;
}

/// Hooks
/// =====
size_t keep_original_size(const char *fn, json_t *patch, size_t patch_size)
{
	return 0;
}

int patch_fmt(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch)
{
	bgm_fmt = (bgm_fmt_t*)file_inout;
	loopmod_fmt();

	auto bgm_count = (size_out / sizeof(bgm_fmt_t));

	// On the surface, the presence of a WAVEFORMATEX structure in
	// thbgm.fmt, and TH13's trance tracks, suggest that the games can
	// work with any PCM format. *However*:
	// • TH07-TH12.8 only create a single BGM streaming buffer for all
	//   tracks, meaning that the PCM format is limited to the one used
	//   by the title screen theme
	// • ≥TH13 do create separate buffers for each track, but always
	//   use the buffer *size* calcuated from the PCM format used by
	//   the title screen theme. Normally, this wouldn't be much of a
	//   problem if it weren't for the fact that the notification byte
	//   positions might no longer lie on sample boundaries, which
	//   might cause issues with certain codecs.
	// Therefore, the only safe formats are the one that the game
	// itself is known to work with. That's two formats for TH13, and
	// one format for all other games
	// Yes, TH06 is the only original game that actually does none of
	// these.
	std::vector<pcm_format_t> allowed_pcmfs;

	auto is_allowed = [&allowed_pcmfs] (const pcm_format_t &pcmf) {
		return std::find(
			allowed_pcmfs.begin(), allowed_pcmfs.end(), pcmf
		) != allowed_pcmfs.end();
	};

	for(decltype(bgm_count) i = 0; i < bgm_count; i++) {
		const auto &wfx = bgm_fmt[i].wfx;
		pcm_format_t pcmf{
			wfx.nSamplesPerSec, wfx.wBitsPerSample, wfx.nChannels
		};
		if(!is_allowed(pcmf)) {
			allowed_pcmfs.emplace_back(pcmf);
		}
	}

	thbgm_mods = std::make_unique<std::unique_ptr<track_t>[]>(bgm_count);
	for(decltype(bgm_count) i = 0; i < bgm_count; i++) {
		auto &fmt_ingame = bgm_fmt[i];
		auto basename_len = strchr(fmt_ingame.fn, '.') - fmt_ingame.fn;
		const stringref_t basename = { fmt_ingame.fn, basename_len };

		auto mod = stack_bgm_resolve(basename);
		if(mod) {
			if(!is_allowed(mod->pcmf)) {
				stringref_t PCMF_LINE_FMT = "\n\xE2\x80\xA2 ";
				size_t supported_len = 0;
				size_t desc_len = sizeof(pcm_format_t::desc_t);
				for(const auto &pcmf : allowed_pcmfs) {
					supported_len += PCMF_LINE_FMT.len + desc_len;
				}
				VLA(char, supported, supported_len + 1);
				char *p = supported;
				for(const auto &pcmf : allowed_pcmfs) {
					const auto desc = pcmf.to_string();
					p = stringref_copy_advance_dst(p, PCMF_LINE_FMT);
					p = strncpy_advance_dst(p, desc.str, desc.len);
				}

				bgmmod_log.errorf(
					"Format error in BGM mod for %s.\n"
					"Modded BGM is %s, but the game only supports:\n"
					"%s",
					fmt_ingame.fn, mod->pcmf.to_string().str, supported
				);
				VLA_FREE(supported);
				continue;
			}
			mod->pcmf.patch(fmt_ingame.wfx);

			// The BGM streaming buffer is 4 seconds long, and until TH13, the
			// games would just rewind the track if it was shorter than that.
			// So, we simply "extend" the looping part to be longer.
			auto track_size_corrected = mod->total_size;
			auto loop_size = track_size_corrected - mod->intro_size;
			while(track_size_corrected < fmt_ingame.wfx.nAvgBytesPerSec * 4) {
				track_size_corrected += loop_size;
			}
			fmt_ingame.intro_size = mod->intro_size;
			fmt_ingame.track_size = track_size_corrected;

			thbgm_mods[i] = std::move(mod);
		}
		// Because why should we load anything else than exactly that,
		// even for original tracks?
		fmt_ingame.preload_size = fmt_ingame.track_size;
	}
	return 1;
}

int patch_pos(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch)
{
	auto *pos = (bgm_pos_t*)file_inout;
	th06_decode_lock = false;
	if(th06_bgmmod_active) {
		// TH06 handles looping by rewinding the file and incrementally
		// reading it using mmioAdvance() until the loop point is reached,
		// rather than just seeking there. (See CWaveFile::ResetFile().)
		// Since "reading" means "decoding" means "another potential
		// performance drop" in the case of a BGM mod, this is another
		// oddity we'd like to get rid of, since looping is handled by
		// the decoding layer anyway.
		// Setting the loop point to 0 only leaves the rewinding calls
		// consisting of mmioSeek() and mmioDescend(), which the decoding
		// layer can simply ignore.
		pos->loop_start = 0;
		return true;
	}

	auto fn_base_len = strchr(fn, '.') - fn;
	VLA(char, fn_base, fn_base_len + 1);
	defer( VLA_FREE(fn_base) );
	memcpy(fn_base, fn, fn_base_len);
	fn_base[fn_base_len] = '\0';
	patch = json_object_get(jsondata_game_get(LOOPMOD_FN.str), fn_base);

	if(!json_is_object(patch)) {
		return false;
	}

	auto *loop_start = json_object_get(patch, "loop_start");
	auto *loop_end = json_object_get(patch, "loop_end");

	if(loop_start || loop_end) {
		log_printf("[BGM] [Loopmod] Changing %s\n", fn_base);
	}
	if(loop_start) {
		pos->loop_start = (uint32_t)json_integer_value(loop_start);
	}
	if(loop_end) {
		pos->loop_end = (uint32_t)json_integer_value(loop_end);
	}
	return true;
}
/// =====

/// DirectSound stub
/// ================
/**
  * Calling DirectSoundCreate8(), with no sound devices installed, fails and
  * returns DSERR_NODRIVER. TH09.5+ then immediately stops initializing its
  * sound system... but still waits for some sound-related variable being
  * changed while loading a stage, which never happens. As a result, the game
  * deadlocks, meaning that you can't play TH09.5+ without a sound device.
  * Working around this bug without binary hacks means that we have to hook...
  * most of DirectSound to fool the game into thinking that everything is
  * fine. In particular, these functions *must* succeed to eliminate any
  * crashes or deadlocks:
  *
  * • DirectSoundCreate8()
  * • IDirectSound8::SetCooperativeLevel()
  * • IDirectSound8::CreateSoundBuffer()
  * • IDirectSound8::DuplicateSoundBuffer()
  * • IDirectSoundBuffer8::Lock(), apparently only needed for TH12.8?
  *   (And yes, the game then fills the returned buffer with zeros, of course
  *   without verifying the returned buffer lock size. Which means that we
  *   also have to actually allocate the required amount of bytes. -.-)
  * • IDirectSoundBuffer8::QueryInterface() with IID_IDirectSoundNotify;
  * • IDirectSoundNotify::SetNotificationPositions()
  *
  * So, here we go...
  */

#define FAIL_IF_NULL(func, ...) \
	!pOrig ? DSERR_UNINITIALIZED : pOrig->func(__VA_ARGS__)

const GUID IID_IDirectSoundBuffer  = { 0x279afa85, 0x4981, 0x11ce, 0xa5, 0x21, 0x00, 0x20, 0xaf, 0x0b, 0xe5, 0x60 };
const GUID IID_IDirectSoundBuffer8 = { 0x6825a449, 0x7524, 0x4d82, 0x92, 0x0f, 0x50, 0xe3, 0x6a, 0xb3, 0xab, 0x1e };
const GUID IID_IDirectSoundNotify  = { 0xb0210783, 0x89cd, 0x11d0, 0xaf, 0x08, 0x00, 0xa0, 0xc9, 0x25, 0xcd, 0x16 };

HRESULT (WINAPI *chain_DirectSoundCreate8)(const GUID *, IDirectSound8 **, IUnknown*) = nullptr;

// IDirectSoundNotify
// ------------------
class bgm_fake_IDirectSoundNotify : public IDirectSoundNotify
{
protected:
	// Sneakily bypassing the need to pre-declare bgm_IDirectSoundBuffer8
	IUnknown *parent;

public:
	bgm_fake_IDirectSoundNotify(IUnknown *parent) : parent(parent) {}

	HRESULT __stdcall QueryInterface(REFIID riid, LPVOID * ppvObj) {
		return parent->QueryInterface(riid, ppvObj);
	}
	ULONG __stdcall AddRef() {
		return parent->AddRef();
	}
	ULONG __stdcall Release() {
		return parent->Release();
	}

	STDMETHOD(SetNotificationPositions)(
		DWORD dwPositionNotifies,
		LPCDSBPOSITIONNOTIFY pcPositionNotifies
	) {
		return DS_OK;
	}
};
// ------------------

// IDirectSoundBuffer8
// -------------------
class bgm_IDirectSoundBuffer8 : public IDirectSoundBuffer8
{
	friend class bgm_IDirectSound8;

protected:
	// Wine also simply includes this one as part of this class.
	bgm_fake_IDirectSoundNotify DSNotify;
	uint8_t *fake_buffer = nullptr;
	size_t fake_buffer_size = 0;

	IDirectSoundBuffer8 *pOrig;
	ULONG fallback_ref = 1;

public:
	bgm_IDirectSoundBuffer8(IDirectSoundBuffer8 *pOrig, size_t buffer_size)
		: pOrig(pOrig), DSNotify(this)
	{
		if(!pOrig) {
			fake_buffer = new uint8_t[buffer_size];
			fake_buffer_size = buffer_size;
		}
	}
	~bgm_IDirectSoundBuffer8() {
		delete[] fake_buffer;
	}

	HRESULT __stdcall QueryInterface(REFIID riid, LPVOID * ppvObj);
	ULONG __stdcall AddRef();
	ULONG __stdcall Release();

	// IDirectSoundBuffer methods
	STDMETHOD(GetCaps)(LPDSBCAPS pDSBufferCaps) {
		return FAIL_IF_NULL(GetCaps, pDSBufferCaps);
	}
	STDMETHOD(GetCurrentPosition)(LPDWORD pdwCurrentPlayCursor, LPDWORD pdwCurrentWriteCursor) {
		return FAIL_IF_NULL(GetCurrentPosition, pdwCurrentPlayCursor, pdwCurrentWriteCursor);
	}
	STDMETHOD(GetFormat)(LPWAVEFORMATEX pwfxFormat, DWORD dwSizeAllocated, LPDWORD pdwSizeWritten) {
		return FAIL_IF_NULL(GetFormat, pwfxFormat, dwSizeAllocated, pdwSizeWritten);
	}
	STDMETHOD(GetVolume)(LPLONG plVolume) {
		return FAIL_IF_NULL(GetVolume, plVolume);
	}
	STDMETHOD(GetPan)(LPLONG plPan) {
		return FAIL_IF_NULL(GetPan, plPan);
	}
	STDMETHOD(GetFrequency)(LPDWORD pdwFrequency) {
		return FAIL_IF_NULL(GetFrequency, pdwFrequency);
	}
	STDMETHOD(GetStatus)(LPDWORD pdwStatus) {
		return FAIL_IF_NULL(GetStatus, pdwStatus);
	}
	STDMETHOD(Initialize)(LPDIRECTSOUND pDirectSound, LPCDSBUFFERDESC pcDSBufferDesc) {
		return FAIL_IF_NULL(Initialize, pDirectSound, pcDSBufferDesc);
	}
	STDMETHOD(Lock)(DWORD, DWORD, LPVOID *, LPDWORD, LPVOID *, LPDWORD, DWORD);
		STDMETHOD(Play)(DWORD dwReserved1, DWORD dwPriority, DWORD dwFlags) {
		return FAIL_IF_NULL(Play, dwReserved1, dwPriority, dwFlags);
	}
	STDMETHOD(SetCurrentPosition)(DWORD dwNewPosition) {
		return FAIL_IF_NULL(SetCurrentPosition, dwNewPosition);
	}
	STDMETHOD(SetFormat)(LPCWAVEFORMATEX pcfxFormat) {
		return FAIL_IF_NULL(SetFormat, pcfxFormat);
	}
	STDMETHOD(SetVolume)(LONG lVolume) {
		return FAIL_IF_NULL(SetVolume, lVolume);
	}
	STDMETHOD(SetPan)(LONG lPan) {
		return FAIL_IF_NULL(SetPan, lPan);
	}
	STDMETHOD(SetFrequency)(DWORD dwFrequency) {
		return FAIL_IF_NULL(SetFrequency, dwFrequency);
	}
	STDMETHOD(Stop)() {
		return FAIL_IF_NULL(Stop);
	}
	STDMETHOD(Unlock)(LPVOID pvAudioPtr1, DWORD dwAudioBytes1, LPVOID pvAudioPtr2, DWORD dwAudioBytes2) {
		return FAIL_IF_NULL(Unlock, pvAudioPtr1, dwAudioBytes1, pvAudioPtr2, dwAudioBytes2);
	}
	STDMETHOD(Restore)() {
		return FAIL_IF_NULL(Restore);
	}

	// IDirectSoundBuffer8 methods
	STDMETHOD(SetFX)(DWORD dwEffectsCount, LPDSEFFECTDESC pDSFXDesc, LPDWORD pdwResultCodes) {
		return FAIL_IF_NULL(SetFX, dwEffectsCount, pDSFXDesc, pdwResultCodes);
	}
	STDMETHOD(AcquireResources)(DWORD dwFlags, DWORD dwEffectsCount, LPDWORD pdwResultCodes) {
		return FAIL_IF_NULL(AcquireResources, dwFlags, dwEffectsCount, pdwResultCodes);
	}
	STDMETHOD(GetObjectInPath)(REFGUID rguidObject, DWORD dwIndex, REFGUID rguidInterface, LPVOID *ppObject) {
		return FAIL_IF_NULL(GetObjectInPath, rguidObject, dwIndex, rguidInterface, ppObject);
	}
};

HRESULT bgm_IDirectSoundBuffer8::QueryInterface(REFIID riid, void** ppvObj)
{
	if(!ppvObj) {
		return E_POINTER;
	}
	if(!pOrig) {
		if(riid == IID_IDirectSoundNotify) {
			AddRef(); // IMPORTANT
			*ppvObj = &DSNotify;
			return S_OK;
		}
		return E_NOINTERFACE;
	}
	*ppvObj = NULL;
	HRESULT hRes = pOrig->QueryInterface(riid, ppvObj);
	if(hRes == NOERROR) {
		*ppvObj = this;
	}
	return hRes;
}

ULONG bgm_IDirectSoundBuffer8::AddRef()
{
	return !pOrig ? ++fallback_ref : pOrig->AddRef();
}

ULONG bgm_IDirectSoundBuffer8::Release()
{
	ULONG count = !pOrig ? --fallback_ref : pOrig->Release();
	if(count == 0) {
		delete this;
	}
	return count;
}

HRESULT bgm_IDirectSoundBuffer8::Lock(
	DWORD dwOffset,
	DWORD dwBytes,
	LPVOID *ppvAudioPtr1,
	LPDWORD pdwAudioBytes1,
	LPVOID *ppvAudioPtr2,
	LPDWORD pdwAudioBytes2,
	DWORD dwFlags
) {
	if(!pOrig) {
		*ppvAudioPtr1 = fake_buffer;
		*ppvAudioPtr2 = nullptr;
		*pdwAudioBytes1 = 0;
		*pdwAudioBytes2 = 0;
		return DS_OK;
	}
	return Lock(dwOffset, dwBytes, ppvAudioPtr1, pdwAudioBytes1, ppvAudioPtr2, pdwAudioBytes2, dwFlags);
}
// -------------------

// IDirectSound8
// -------------
class bgm_IDirectSound8 : public IDirectSound8
{
	IUNKNOWN_DEC(bgm, IDirectSound8);

	// IDirectSound methods
	STDMETHOD(CreateSoundBuffer)(LPCDSBUFFERDESC, LPDIRECTSOUNDBUFFER *, LPUNKNOWN);
	STDMETHOD(GetCaps)(LPDSCAPS pDSCaps) {
		return FAIL_IF_NULL(GetCaps, pDSCaps);
	}
	STDMETHOD(DuplicateSoundBuffer)(LPDIRECTSOUNDBUFFER, LPDIRECTSOUNDBUFFER *);
	STDMETHOD(SetCooperativeLevel)(HWND, DWORD);
	STDMETHOD(Compact)() {
		return FAIL_IF_NULL(Compact);
	}
	STDMETHOD(GetSpeakerConfig)(LPDWORD pdwSpeakerConfig) {
		return FAIL_IF_NULL(GetSpeakerConfig, pdwSpeakerConfig);
	}
	STDMETHOD(SetSpeakerConfig)(DWORD dwSpeakerConfig) {
		return FAIL_IF_NULL(SetSpeakerConfig, dwSpeakerConfig);
	}
	STDMETHOD(Initialize)(LPCGUID pcGuidDevice) {
		return FAIL_IF_NULL(Initialize, pcGuidDevice);
	}

	// IDirectSound8 methods
	STDMETHOD(VerifyCertification)(LPDWORD pdwCertified) {
		return FAIL_IF_NULL(VerifyCertification, pdwCertified);
	}
};

IUNKNOWN_DEF(bgm_IDirectSound8, true);

HRESULT bgm_IDirectSound8::CreateSoundBuffer(
	LPCDSBUFFERDESC pcDSBufferDesc,
	LPDIRECTSOUNDBUFFER *ppDSBuffer,
	LPUNKNOWN pUnkOuter
)
{
	if(!pOrig) {
		if(!pcDSBufferDesc) {
			return DSERR_INVALIDPARAM;
		}
		*ppDSBuffer = new bgm_IDirectSoundBuffer8(nullptr, pcDSBufferDesc->dwBufferBytes);
		return DS_OK;
	}
	return pOrig->CreateSoundBuffer(pcDSBufferDesc, ppDSBuffer, pUnkOuter);
}

HRESULT bgm_IDirectSound8::DuplicateSoundBuffer(
	LPDIRECTSOUNDBUFFER pDSBufferOriginal,
	LPDIRECTSOUNDBUFFER *ppDSBufferDuplicate
)
{
	if(!pOrig) {
		if(!pDSBufferOriginal) {
			return DSERR_INVALIDPARAM;
		}
		auto size = ((bgm_IDirectSoundBuffer8*)pDSBufferOriginal)->fake_buffer_size;
		*ppDSBufferDuplicate = new bgm_IDirectSoundBuffer8(nullptr, size);
		return DS_OK;
	}
	return FAIL_IF_NULL(DuplicateSoundBuffer, pDSBufferOriginal, ppDSBufferDuplicate);
}

HRESULT bgm_IDirectSound8::SetCooperativeLevel(
	HWND hwnd,
	DWORD dwLevel
)
{
	if(!pOrig) {
		return DS_OK;
	}
	return pOrig->SetCooperativeLevel(hwnd, dwLevel);
}

HRESULT WINAPI bgm_DirectSoundCreate8(
	const GUID *pcGuidDevice, IDirectSound8 **ppDS8, IUnknown* pUnkOuter
)
{
	auto ret = chain_DirectSoundCreate8(pcGuidDevice, ppDS8, pUnkOuter);
	if(FAILED(ret)) {
		*ppDS8 = new bgm_IDirectSound8(*ppDS8);
		return DS_OK;
	}
	return ret;
}
// -------------
/// ================

/// Module functions
/// ================
extern "C" __declspec(dllexport) void bgm_mod_init(void)
{
	jsondata_game_add(LOOPMOD_FN.str);
	// Kioh Gyoku is a thing...
	if(game_id < TH07) {
		patchhook_register("*.pos", patch_pos, keep_original_size);
	} else {
		// albgm.fmt and trial versions are also a thing...
		patchhook_register("*bgm*.fmt", patch_fmt, keep_original_size);
	}
}

extern "C" __declspec(dllexport) void bgm_mod_detour(void)
{
	auto dsound = GetModuleHandleA("dsound.dll");
	if(dsound) {
		*(void**)&chain_DirectSoundCreate8 = GetProcAddress(dsound, "DirectSoundCreate8");
		detour_chain("dsound.dll", 1,
			"DirectSoundCreate8", bgm_DirectSoundCreate8, &chain_DirectSoundCreate8,
			nullptr
		);
	}
	if(game_id == TH06) {
		mmio::detour();
		detour_chain("winmm.dll", 1,
			"mmioAdvance", th06_mmioAdvance, &chain_mmioAdvance,
			"mmioOpenA", th06_mmioOpenA, &chain_mmioOpenA,
			nullptr
		);
	} else {
		detour_chain("kernel32.dll", 1,
			"CreateFileA", thbgm_CreateFileA, &chain_CreateFileA,
			"CloseHandle", thbgm_CloseHandle, &chain_CloseHandle,
			"ReadFile", thbgm_ReadFile, &chain_ReadFile,
			"SetFilePointer", thbgm_SetFilePointer, &chain_SetFilePointer,
			nullptr
		);
	}
}

extern "C" __declspec(dllexport) void bgm_mod_repatch(json_t *files_changed)
{
	if(!bgm_fmt) {
		return;
	}
	const char *fn;
	json_t *val;
	json_object_foreach(files_changed, fn, val) {
		if(strstr(fn, LOOPMOD_FN.str)) {
			loopmod_fmt();
		}
	}
}
/// ================
