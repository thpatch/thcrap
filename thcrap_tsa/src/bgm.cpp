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

#pragma comment(lib, "thcrap_bgmmod" DEBUG_OR_RELEASE)

const stringref_t LOOPMOD_FN = "loops.js";

// For repatchability. Pointing into game memory.
bgm_fmt_t* bgm_fmt = nullptr;

bgm_fmt_t* bgm_find(stringref_t fn)
{
	assert(bgm_fmt);
	if(fn.len > sizeof(bgm_fmt->fn)) {
		return nullptr;
	}
	auto track = bgm_fmt;
	while(track->fn[0] != '\0') {
		if(!strncmp(track->fn, fn.str, fn.len)) {
			return track;
		}
		track++;
	}
	return nullptr;
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

int loopmod_fmt()
{
	int modded = 0;
	auto mod = [] (stringref_t track_name, const json_t* track_mod) {
		auto track = bgm_find(track_name);
		if(!track) {
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
		}

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
	return loopmod_fmt();
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
