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
	auto fn_base_len = strchr(fn, '.') - fn;
	VLA(char, fn_base, fn_base_len + 1);
	defer( VLA_FREE(fn_base) );
	memcpy(fn_base, fn, fn_base_len);
	fn_base[fn_base_len] = '\0';
	patch = json_object_get(jsondata_game_get(LOOPMOD_FN.str), fn_base);

	if(!json_is_object(patch)) {
		return false;
	}

	auto *pos = (bgm_pos_t*)file_inout;
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
/// =====
