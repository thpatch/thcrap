/**
  * Touhou Community Reliant Automatic Patcher
  * BGM modding library for games using uncompressed PCM music
  *
  * ----
  *
  * Generic implementation for the MMIO API.
  */

#include <thcrap.h>
#include "bgmmod.hpp"

static MMRESULT (WINAPI *chain_mmioAdvance)(HMMIO, LPMMIOINFO, UINT);
static MMRESULT (WINAPI *chain_mmioAscend)(HMMIO, LPMMCKINFO, UINT);
static MMRESULT (WINAPI *chain_mmioClose)(HMMIO, UINT);
static MMRESULT (WINAPI *chain_mmioDescend)(HMMIO, LPMMCKINFO, const MMCKINFO *, UINT);
static MMRESULT (WINAPI *chain_mmioGetInfo)(HMMIO, LPMMIOINFO, UINT);
static HMMIO (WINAPI *chain_mmioOpenA)(LPSTR, LPMMIOINFO, DWORD);
static LONG (WINAPI *chain_mmioRead)(HMMIO, HPSTR, LONG);
static LONG (WINAPI *chain_mmioSeek)(HMMIO, LONG, int);
static MMRESULT (WINAPI *chain_mmioSetInfo)(HMMIO, LPCMMIOINFO, UINT);

struct mmio_wrap_t
{
	std::unique_ptr<track_t> track;

	// Moving parts of the MMIOINFO structure
	uint32_t open_flags;
	char *next;
	char buffer[MMIO_DEFAULTBUFFER];
};

mmio_wrap_t mmio_wrap;

// Using a separate function to decide whether [handle] points to a MMIO
// wrapper structure makes it easier to later handle multiple files
// simultaneously.
mmio_wrap_t* find_mmio_wrap(void* handle)
{
	if(handle == &mmio_wrap) {
		return &mmio_wrap;
	}
	return nullptr;
}

#define FALLBACK_ON_SYSTEM_HANDLE(func, ...) \
auto *mod = find_mmio_wrap(hmmio); \
if(!mod) { \
	return chain_##func(hmmio, ##__VA_ARGS__); \
}

MMRESULT WINAPI bgmmod_mmioAdvance(HMMIO hmmio, LPMMIOINFO pmmioinfo, UINT fuAdvance)
{
	FALLBACK_ON_SYSTEM_HANDLE(mmioAdvance, pmmioinfo, fuAdvance);

	// TODO: Currently a TH06 assumption, but what else can we possibly do?
	assert(fuAdvance == MMIO_READ);

	mod->track->decode((uint8_t*)pmmioinfo->pchBuffer, pmmioinfo->cchBuffer);
	pmmioinfo->pchNext = pmmioinfo->pchBuffer;

	return MMSYSERR_NOERROR;
}

MMRESULT WINAPI bgmmod_mmioAscend(HMMIO hmmio, LPMMCKINFO pmmcki, UINT fuAscend)
{
	FALLBACK_ON_SYSTEM_HANDLE(mmioAscend, pmmcki, fuAscend);
	return MMSYSERR_NOERROR;
}

MMRESULT WINAPI bgmmod_mmioClose(HMMIO hmmio, UINT fuClose)
{
	FALLBACK_ON_SYSTEM_HANDLE(mmioClose, fuClose);
	return 0;
}

MMRESULT WINAPI bgmmod_mmioDescend(HMMIO hmmio, LPMMCKINFO pmmcki, const MMCKINFO *pmmckiParent, UINT fuDescend)
{
	FALLBACK_ON_SYSTEM_HANDLE(mmioDescend, pmmcki, pmmckiParent, fuDescend);

	const FOURCC cRIFF = 0x46464952; // "RIFF"
	const FOURCC cWAVE = 0x45564157; // "WAVE"
	const FOURCC cFmt  = 0x20746D66; // "fmt "
	const FOURCC cData = 0x61746164; // "data"

	pmmcki->fccType = 0;

	if(fuDescend == MMIO_FINDCHUNK) {
		switch(pmmcki->ckid) {
		case cFmt:
			pmmcki->cksize = sizeof(PCMWAVEFORMAT);
			return MMSYSERR_NOERROR;
		case cData:
			pmmcki->cksize = mod->track->total_size;
			return MMSYSERR_NOERROR;
		}
	} else if(pmmckiParent == nullptr) {
		pmmcki->ckid = cRIFF;
		pmmcki->cksize =
			4 // "WAVE"
			+ 8 + sizeof(PCMWAVEFORMAT)   // "fmt " chunk header + data
			+ 8 + mod->track->total_size; // "data" chunk header + data
		pmmcki->fccType = cWAVE;
		return MMSYSERR_NOERROR;
	}
	return MMIOERR_CHUNKNOTFOUND;
}

MMRESULT WINAPI bgmmod_mmioGetInfo(HMMIO hmmio, LPMMIOINFO pmmioinfo, UINT fuInfo)
{
	FALLBACK_ON_SYSTEM_HANDLE(mmioGetInfo, pmmioinfo, fuInfo);

	ZeroMemory(pmmioinfo, sizeof(*pmmioinfo));
	pmmioinfo->dwFlags = mod->open_flags;
	pmmioinfo->pchNext = mod->next;
	if(pmmioinfo->dwFlags & MMIO_ALLOCBUF) {
		pmmioinfo->cchBuffer = sizeof(mod->buffer);
		pmmioinfo->pchBuffer = mod->buffer;
		pmmioinfo->pchEndRead = mod->buffer + sizeof(mod->buffer);
		pmmioinfo->pchEndWrite = mod->buffer + sizeof(mod->buffer);
	}
	return MMSYSERR_NOERROR;
}

HMMIO WINAPI bgmmod_mmioOpenA(LPSTR pszFileName, LPMMIOINFO pmmioinfo, DWORD fdwOpen)
{
	auto fallback = [&] {
		mmio_wrap.track.reset(nullptr);
		return chain_mmioOpenA(pszFileName, pmmioinfo, fdwOpen);
	};

	if(!pszFileName) {
		return fallback();
	}

	const char *p = pszFileName;
	const char *basename = pszFileName;
	const char *ext = nullptr;
	while(p[0] != '\0') {
		switch(p[0]) {
		case '/':
		case '\\':
			basename = p + 1;
			break;
		case '.':
			ext = p;
			break;
		}
		p++;
	}
	if(!ext) {
		return fallback();
	}

	auto basename_len = (ext - basename);
	auto modtrack = stack_bgm_resolve({ basename, basename_len });
	if(modtrack == nullptr) {
		return fallback();
	}

	mmio_wrap.track = std::move(modtrack);
	mmio_wrap.open_flags = fdwOpen;
	mmio_wrap.next = (fdwOpen & MMIO_ALLOCBUF)
		? mmio_wrap.buffer
		: pmmioinfo->pchBuffer;
	return (HMMIO)&mmio_wrap;
}

LONG WINAPI bgmmod_mmioRead(HMMIO hmmio, HPSTR pch, LONG cch)
{
	FALLBACK_ON_SYSTEM_HANDLE(mmioRead, pch, cch);
	// TODO: Both TH06 and Kioh Gyoku only use this
	// function for reading that structure, so...?
	assert(cch == sizeof(PCMWAVEFORMAT));
	mod->track->pcmf.patch(*(PCMWAVEFORMAT *)pch);
	return cch;
}

LONG WINAPI bgmmod_mmioSeek(HMMIO hmmio, LONG lOffset, int iOrigin)
{
	FALLBACK_ON_SYSTEM_HANDLE(mmioSeek, lOffset, iOrigin);
	return 0;
}

MMRESULT WINAPI bgmmod_mmioSetInfo(HMMIO hmmio, LPCMMIOINFO pmmioinfo, UINT fuInfo)
{
	FALLBACK_ON_SYSTEM_HANDLE(mmioSetInfo, pmmioinfo, fuInfo);
	mod->next = pmmioinfo->pchNext;
	return MMSYSERR_NOERROR;
}

namespace mmio
{
	int detour(void)
	{
		auto winmm = GetModuleHandleA("winmm.dll");
		if(!winmm) {
			return -1;
		}
		*(void**)&chain_mmioAdvance = GetProcAddress(winmm, "mmioAdvance");
		*(void**)&chain_mmioAscend = GetProcAddress(winmm, "mmioAscend");
		*(void**)&chain_mmioDescend = GetProcAddress(winmm, "mmioDescend");
		*(void**)&chain_mmioClose = GetProcAddress(winmm, "mmioClose");
		*(void**)&chain_mmioGetInfo = GetProcAddress(winmm, "mmioGetInfo");
		*(void**)&chain_mmioOpenA = GetProcAddress(winmm, "mmioOpenA");
		*(void**)&chain_mmioRead = GetProcAddress(winmm, "mmioRead");
		*(void**)&chain_mmioSeek = GetProcAddress(winmm, "mmioSeek");
		*(void**)&chain_mmioSetInfo = GetProcAddress(winmm, "mmioSetInfo");
		return detour_chain("winmm.dll", 1,
			"mmioAdvance", bgmmod_mmioAdvance, &chain_mmioAdvance,
			"mmioAscend", bgmmod_mmioAscend, &chain_mmioAscend,
			"mmioClose", bgmmod_mmioClose, &chain_mmioClose,
			"mmioDescend", bgmmod_mmioDescend, &chain_mmioDescend,
			"mmioGetInfo", bgmmod_mmioGetInfo, &chain_mmioGetInfo,
			"mmioOpenA", bgmmod_mmioOpenA, &chain_mmioOpenA,
			"mmioRead", bgmmod_mmioRead, &chain_mmioRead,
			"mmioSeek", bgmmod_mmioSeek, &chain_mmioSeek,
			"mmioSetInfo", bgmmod_mmioSetInfo, &chain_mmioSetInfo,
			nullptr
		);
	}

	bool is_modded_handle(HMMIO hmmio)
	{
		return find_mmio_wrap(hmmio) != nullptr;
	}
}
