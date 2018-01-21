/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * BGM patching.
  */

#include <thcrap.h>
extern "C" {
# include <thtypes/bgm_types.h>
# include "thcrap_tsa.h"
}

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
		if(!strncmp(track->fn, fn.str, sizeof(track->fn))) {
			return track;
		}
		track++;
	}
	return nullptr;
};

int loopmod_fmt(json_t *fmt_jdiff, const char *jdiff_fn)
{
	int modded = 0;
	auto mod = [jdiff_fn] (stringref_t fn_to_patch, const json_t* track_mod) {
		auto track = bgm_find(fn_to_patch);
		if(!track) {
			log_mboxf(nullptr, MB_OK | MB_ICONEXCLAMATION,
				"Error applying %s: Track \"%s\" does not exist.",
				jdiff_fn, fn_to_patch.str
			);
			return false;
		}

		auto sample_size = track->wfx.nChannels * (track->wfx.wBitsPerSample / 8);
		auto *loop_start = json_object_get(track_mod, "loop_start");
		auto *loop_end = json_object_get(track_mod, "loop_end");

		if(loop_start || loop_end) {
			log_printf("[BGM] [Loopmod] Changing %s\n", fn_to_patch.str);
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
	const char *key;
	const json_t *track_mod;
	json_object_foreach(fmt_jdiff, key, track_mod) {
		modded += mod(key, track_mod);
	}
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
	return loopmod_fmt(patch, fn);
}

int patch_pos(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch)
{
	auto *pos = (bgm_pos_t*)file_inout;
	auto *loop_start = json_object_get(patch, "loop_start");
	auto *loop_end = json_object_get(patch, "loop_end");

	if(loop_start || loop_end) {
		log_printf("[BGM] [Loopmod] Changing %s\n", fn);
	}
	if(loop_start) {
		pos->loop_start = (uint32_t)json_integer_value(loop_start);
	}
	if(loop_end) {
		pos->loop_end = (uint32_t)json_integer_value(loop_end);
	}
	return true;
}

extern "C" __declspec(dllexport) void bgm_mod_init(void)
{
	// Kioh Gyoku is a thing...
	if(game_id < TH07) {
		patchhook_register("*.pos", patch_pos, keep_original_size);
	} else {
		// albgm.fmt and trial versions are also a thing...
		patchhook_register("*bgm*.fmt", patch_fmt, keep_original_size);
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
		if(strstr(fn, ".fmt.jdiff")) {
			json_t *patch = stack_json_resolve(fn, nullptr);
			loopmod_fmt(patch, fn);
			json_decref(patch);
		}
	}
}
/// =====
