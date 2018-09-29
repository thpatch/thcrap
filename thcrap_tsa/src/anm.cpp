/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * On-the-fly ANM patcher.
  *
  * Portions adapted from Touhou Toolkit
  * https://github.com/thpatch/thtk
  */

#include <thcrap.h>
#include <png.h>
#include "png_ex.h"
#include "thcrap_tsa.h"
#include "anm.hpp"

/// Blitting modes
/// --------------
void blit_overwrite(png_byte *dst, const png_byte *rep, unsigned int pixels, format_t format)
{
	memcpy(dst, rep, pixels * format_Bpp(format));
}

void blit_blend(png_byte *dst, const png_byte *rep, unsigned int pixels, format_t format)
{
	// Alpha values are added and clamped to the format's maximum. This avoids a
	// flaw in the blending algorithm, which may decrease the alpha value even if
	// both target and replacement pixels are fully opaque.
	// (This also seems to be what the default composition mode in GIMP does.)
	unsigned int i;
	if(format == FORMAT_BGRA8888) {
		for(i = 0; i < pixels; ++i, dst += 4, rep += 4) {
			const int new_alpha = dst[3] + rep[3];
			const int dst_alpha = 0xff - rep[3];

			dst[0] = (dst[0] * dst_alpha + rep[0] * rep[3]) >> 8;
			dst[1] = (dst[1] * dst_alpha + rep[1] * rep[3]) >> 8;
			dst[2] = (dst[2] * dst_alpha + rep[2] * rep[3]) >> 8;
			dst[3] = min(new_alpha, 0xff);
		}
	} else if(format == FORMAT_ARGB4444) {
		for(i = 0; i < pixels; ++i, dst += 2, rep += 2) {
			const unsigned char rep_a = (rep[1] & 0xf0) >> 4;
			const unsigned char rep_r = (rep[1] & 0x0f) >> 0;
			const unsigned char rep_g = (rep[0] & 0xf0) >> 4;
			const unsigned char rep_b = (rep[0] & 0x0f) >> 0;
			const unsigned char dst_a = (dst[1] & 0xf0) >> 4;
			const unsigned char dst_r = (dst[1] & 0x0f) >> 0;
			const unsigned char dst_g = (dst[0] & 0xf0) >> 4;
			const unsigned char dst_b = (dst[0] & 0x0f) >> 0;
			const int new_alpha = dst_a + rep_a;
			const int dst_alpha = 0xf - rep_a;

			dst[1] =
				(min(new_alpha, 0xf) << 4) |
				((dst_r * dst_alpha + rep_r * rep_a) >> 4);
			dst[0] =
				(dst_g * dst_alpha + rep_g * rep_a & 0xf0) |
				((dst_b * dst_alpha + rep_b * rep_a) >> 4);
		}
	} else {
		// Other formats have no alpha channel, so we can just do...
		blit_overwrite(dst, rep, pixels, format);
	}
}
/// --------------

/// Formats
/// -------
unsigned int format_Bpp(format_t format)
{
	switch(format) {
		case FORMAT_BGRA8888:
			return 4;
		case FORMAT_ARGB4444:
		case FORMAT_RGB565:
			return 2;
		case FORMAT_GRAY8:
			return 1;
		default:
			log_printf("unknown format: %u\n", format);
			return 0;
	}
}

png_uint_32 format_png_equiv(format_t format)
{
	switch(format) {
		case FORMAT_BGRA8888:
		case FORMAT_ARGB4444:
		case FORMAT_RGB565:
			return PNG_FORMAT_BGRA;
		case FORMAT_GRAY8:
			return PNG_FORMAT_GRAY;
		default:
			log_printf("unknown format: %u\n", format);
			return PNG_FORMAT_INVALID;
	}
}

png_byte format_alpha_max(format_t format)
{
	switch(format) {
		case FORMAT_BGRA8888:
			return 0xff;
		case FORMAT_ARGB4444:
			return 0xf;
		default:
			return 0;
	}
}

size_t format_alpha_sum(png_bytep data, unsigned int pixels, format_t format)
{
	size_t ret = 0;
	unsigned int i;
	if(format == FORMAT_BGRA8888) {
		for(i = 0; i < pixels; ++i, data += 4) {
			ret += data[3];
		}
	} else if(format == FORMAT_ARGB4444) {
		for(i = 0; i < pixels; ++i, data += 2) {
			ret += (data[1] & 0xf0) >> 4;
		}
	}
	return ret;
}

void format_from_bgra(png_bytep data, unsigned int pixels, format_t format)
{
	unsigned int i;
	png_bytep in = data;

	if(format == FORMAT_ARGB4444) {
		png_bytep out = data;
		for(i = 0; i < pixels; ++i, in += 4, out += 2) {
			// I don't see the point in doing any "rounding" here. Let's rather focus on
			// writing understandable code independent of endianness assumptions.
			const unsigned char b = in[0] >> 4;
			const unsigned char g = in[1] >> 4;
			const unsigned char r = in[2] >> 4;
			const unsigned char a = in[3] >> 4;
			// Yes, we start with the second byte. "Little-endian ARGB", mind you.
			out[1] = (a << 4) | r;
			out[0] = (g << 4) | b;
		}
	} else if(format == FORMAT_RGB565) {
		png_uint_16p out16 = (png_uint_16p)data;
		for(i = 0; i < pixels; ++i, in += 4, ++out16) {
			const unsigned char b = in[0] >> 3;
			const unsigned char g = in[1] >> 2;
			const unsigned char r = in[2] >> 3;

			out16[0] = (r << 11) | (g << 5) | b;
		}
	}
	// FORMAT_GRAY8 is fully handled by libpng
}
/// -------

/// Sprite-level patching
/// ---------------------
int sprite_patch_set(
	sprite_patch_t &sp,
	const anm_entry_t &entry,
	const sprite_local_t *sprite,
	const png_image_ex &image
)
{
	if(!entry.thtx || !sprite || !image.buf) {
		return -1;
	}
	ZeroMemory(&sp, sizeof(sp));

	// Note that we don't use the PNG_IMAGE_* macros here - the actual bit depth
	// after format_from_bgra() may no longer be equal to the one in the PNG header.
	sp.format = (format_t)entry.thtx->format;
	sp.bpp = format_Bpp(sp.format);

	sp.dst_x = sprite->x;
	sp.dst_y = sprite->y;

	sp.rep_x = entry.x + sp.dst_x;
	sp.rep_y = entry.y + sp.dst_y;

	if(
		sp.dst_x >= entry.w || sp.dst_y >= entry.h ||
		sp.rep_x >= image.img.width || sp.rep_y >= image.img.height
	) {
		return 2;
	}

	sp.rep_stride = image.img.width * sp.bpp;
	sp.dst_stride = entry.w * sp.bpp;

	// See th11's face/enemy5/face05lo.png (the dummy 8x8 one from
	// stgenm06.anm, not stgenm05.anm) for an example where the sprite
	// width and height actually exceed the dimensions of the THTX.
	png_uint_32 sprite_clamped_w = min(sprite->w, (entry.w - sprite->x));
	png_uint_32 sprite_clamped_h = min(sprite->h, (entry.h - sprite->y));

	sp.copy_w = min(sprite_clamped_w, (image.img.width - sp.rep_x));
	sp.copy_h = min(sprite_clamped_h, (image.img.height - sp.rep_y));

	sp.dst_buf = entry.thtx->data + (sp.dst_y * sp.dst_stride) + (sp.dst_x * sp.bpp);
	sp.rep_buf = image.buf + (sp.rep_y * sp.rep_stride) + (sp.rep_x * sp.bpp);
	return 0;
}

sprite_alpha_t sprite_alpha_analyze(
	const png_bytep buf,
	const format_t format,
	const size_t stride,
	const png_uint_32 w,
	const png_uint_32 h
)
{
	const size_t opaque_sum = format_alpha_max(format) * w;
	if(!buf) {
		return SPRITE_ALPHA_EMPTY;
	} else if(!opaque_sum) {
		return SPRITE_ALPHA_OPAQUE;
	} else {
		sprite_alpha_t ret = SPRITE_ALPHA_FULL;
		png_uint_32 row;
		png_bytep p = buf;
		for(row = 0; row < h; row++) {
			size_t sum = format_alpha_sum(p, w, format);
			if(sum == 0x00 && ret != SPRITE_ALPHA_OPAQUE) {
				ret = SPRITE_ALPHA_EMPTY;
			} else if(sum == opaque_sum && ret != SPRITE_ALPHA_EMPTY) {
				ret = SPRITE_ALPHA_OPAQUE;
			} else {
				return SPRITE_ALPHA_FULL;
			}
			p += stride;
		}
		return ret;
	}
}

sprite_alpha_t sprite_alpha_analyze_rep(const sprite_patch_t &sp)
{
	return sprite_alpha_analyze(sp.rep_buf, sp.format, sp.rep_stride, sp.copy_w, sp.copy_h);
}

sprite_alpha_t sprite_alpha_analyze_dst(const sprite_patch_t &sp)
{
	return sprite_alpha_analyze(sp.dst_buf, sp.format, sp.dst_stride, sp.copy_w, sp.copy_h);
}

int sprite_blit(const sprite_patch_t &sp, const BlitFunc_t func)
{
	assert(func);

	png_uint_32 row;
	png_bytep dst_p = sp.dst_buf;
	png_bytep rep_p = sp.rep_buf;
	for(row = 0; row < sp.copy_h; row++) {
		func(dst_p, rep_p, sp.copy_w, sp.format);
		dst_p += sp.dst_stride;
		rep_p += sp.rep_stride;
	}
	return 0;
}

sprite_alpha_t sprite_patch(const sprite_patch_t &sp)
{
	sprite_alpha_t rep_alpha = sprite_alpha_analyze_rep(sp);
	if(rep_alpha != SPRITE_ALPHA_EMPTY) {
		BlitFunc_t func = NULL;
		sprite_alpha_t dst_alpha = sprite_alpha_analyze_dst(sp);
		if(dst_alpha == SPRITE_ALPHA_OPAQUE) {
			func = blit_blend;
		} else {
			func = blit_overwrite;
		}
		sprite_blit(sp, func);
	}
	return rep_alpha;
}
/// ---------------------

/// ANM header patching
/// -------------------
bool header_mod_error(const char *text, ...)
{
	va_list va;
	va_start(va, text);
	log_vmboxf("ANM header patching error", MB_ICONERROR, text, va);
	va_end(va);
	return false;
}

entry_mods_t header_mods_t::entry_mods()
{
	entry_mods_t ret;
	if(!entries) {
		return ret;
	}

	ret.num = entries_seen++;

#define FAIL(context, text) \
	entries = nullptr; \
	header_mod_error( \
		"\"entries\"{\"%u\"%s}: %s%s", ret.num, context, text, \
		"\nIgnoring remaining entry modifications for this file..." \
	); \
	return ret;

	auto mod_j = json_object_numkey_get(entries, ret.num);
	if(!mod_j) {
		return ret;
	}
	if(!json_is_object(mod_j)) {
		FAIL("", "Must be a JSON object.");
	}

	// File name
	auto name_j = json_object_get(mod_j, "name");
	if(name_j && !json_is_string(name_j)) {
		FAIL(": {\"name\"}", "Must be a JSON string.");
	}
	ret.name = json_string_value(name_j);
	return ret;

#undef FAIL
}

void entry_mods_t::apply_ourdata(anm_entry_t &entry)
{
	if(name) {
		log_printf(
			"(Header) Entry #%u: %s \xE2\x86\x92 %s\n",
			num, entry.name, name
		);
		entry.name = name;
	}
}

sprite_mods_t header_mods_t::sprite_mods()
{
	sprite_mods_t ret;
	if(!sprites) {
		return ret;
	}

	ret.num = sprites_seen++;

#define FAIL(text, ...) \
	sprites = nullptr; \
	return header_mod_error( \
		"\"sprites\"{\"%u\"}: " text "%s", \
		ret.num, ##__VA_ARGS__, \
		"\nIgnoring remaining sprite boundary mods for this file..." \
	);

	auto bounds_parse = [&](const json_t *bounds_j) {
		const char *COORD_NAMES[4] = { "X", "Y", "width", "height" };

		if(json_array_size(bounds_j) != 4) {
			FAIL("Must be a JSON array in [X, Y, width, height] format.");
		}
		for(unsigned int i = 0; i < 4; i++) {
			auto coord_j = json_array_get(bounds_j, i);
			bool failed = !json_is_integer(coord_j);
			if(!failed) {
				ret.bounds.val[i] = (float)json_integer_value(coord_j);
				failed = (ret.bounds.val[i] < 0.0f);
			}
			if(failed) {
				FAIL(
					"Coordinate #%u (%s) must be a positive JSON integer.",
					i + 1, COORD_NAMES[i]
				);
			}
		}
		return (ret.bounds.valid = true);
	};

	auto mod_j = json_object_numkey_get(sprites, ret.num);
	if(mod_j) {
		bounds_parse(mod_j);
	}
	return ret;

#undef FAIL
}

void sprite_mods_t::apply_orig(sprite_t &orig)
{
	if(bounds.valid) {
		log_printf(
			"(Header) Sprite #%u: [%.0f, %.0f, %.0f, %.0f] \xE2\x86\x92 [%.0f, %.0f, %.0f, %.0f]\n",
			num,
			orig.x, orig.y, orig.w, orig.h,
			bounds.val[0], bounds.val[1], bounds.val[2], bounds.val[3]
		);
		orig.x = bounds.val[0];
		orig.y = bounds.val[1];
		orig.w = bounds.val[2];
		orig.h = bounds.val[3];
	}
}

header_mods_t::header_mods_t(json_t *patch)
{
	auto object_get = [this, patch] (const char *key) -> json_t* {
		auto ret = json_object_get(patch, key);
		if(ret && !json_is_object(ret)) {
			header_mod_error("\"%s\" must be a JSON object.", key);
			return nullptr;
		}
		return ret;
	};

	entries = object_get("entries");
	sprites = object_get("sprites");
}
/// -------------------

/// ANM structure
/// -------------
sprite_local_t *sprite_split_new(anm_entry_t &entry)
{
	sprite_local_t *sprites_new = (sprite_local_t*)realloc(
		entry.sprites, (entry.sprite_num + 1) * sizeof(sprite_local_t)
	);
	if(!sprites_new) {
		return NULL;
	}
	entry.sprites = sprites_new;
	return &sprites_new[entry.sprite_num++];
}

int sprite_split_x(anm_entry_t &entry, sprite_local_t *sprite)
{
	if(entry.thtx && entry.w > 0) {
		png_uint_32 split_w = sprite->x + sprite->w;
		if(split_w > entry.w) {
			sprite_local_t *sprite_new = sprite_split_new(entry);
			if(!sprite_new) {
				return 1;
			}
			sprite_new->x = 0;
			sprite_new->y = sprite->y;
			sprite_new->w = min(split_w - entry.w, sprite->x);
			sprite_new->h = sprite->h;
			return sprite_split_y(entry, sprite_new);
		}
		return 0;
	}
	return -1;
}

int sprite_split_y(anm_entry_t &entry, sprite_local_t *sprite)
{
	if(entry.thtx && entry.h > 0) {
		png_uint_32 split_h = sprite->y + sprite->h;
		if(split_h > entry.h) {
			sprite_local_t *sprite_new = sprite_split_new(entry);
			if(!sprite_new) {
				return 1;
			}
			sprite_new->x = sprite->x;
			sprite_new->y = 0;
			sprite_new->w = sprite->w;
			sprite_new->h = min(split_h - entry.h, sprite->h);
			return sprite_split_x(entry, sprite_new);
		}
		return 0;
	}
	return -1;
}

#define ANM_ENTRY_FILTER(in, type) \
	type *header = (type *)in; \
	entry.w = header->w; \
	entry.h = header->h; \
	entry.nextoffset = header->nextoffset; \
	entry.sprite_num = header->sprites; \
	entry.name = (const char*)(header->nameoffset + (size_t)header); \
	thtxoffset = header->thtxoffset; \
	entry.hasdata = (header->hasdata != 0); \
	headersize = sizeof(type);

int anm_entry_init(header_mods_t &hdr_m, anm_entry_t &entry, BYTE *in, json_t *patch)
{
	if(!in) {
		return -1;
	}

	size_t thtxoffset = 0;
	size_t headersize = 0;

	anm_entry_clear(entry);
	auto ent_m = hdr_m.entry_mods();
	if(game_id >= TH11) {
		ANM_ENTRY_FILTER(in, anm_header11_t);
		entry.x = header->x;
		entry.y = header->y;
	} else {
		ANM_ENTRY_FILTER(in, anm_header06_t);
	}

	assert((entry.hasdata == false) == (thtxoffset == 0));
	assert(headersize);

	entry.hasdata |= (entry.name[0] != '@');
	if(thtxoffset) {
		entry.thtx = (thtx_header_t*)(thtxoffset + (size_t)in);
		if(memcmp(entry.thtx->magic, "THTX", sizeof(entry.thtx->magic))) {
			return 1;
		}
		entry.w = entry.thtx->w;
		entry.h = entry.thtx->h;
	}

	// This will change with splits being appended...
	auto sprite_orig_num = entry.sprite_num;
	auto *sprite_in = (uint32_t*)(in + headersize);

	entry.sprites = new sprite_local_t[sprite_orig_num];
	for(size_t i = 0; i < sprite_orig_num; i++, sprite_in++) {
		auto *s_orig = (sprite_t*)(in + *sprite_in);
		auto *s_local = &entry.sprites[i];

		auto spr_m = hdr_m.sprite_mods();
		spr_m.apply_orig(*s_orig);

		s_local->x = (png_uint_32)s_orig->x;
		s_local->y = (png_uint_32)s_orig->y;
		s_local->w = (png_uint_32)s_orig->w;
		s_local->h = (png_uint_32)s_orig->h;
		sprite_split_x(entry, s_local);
		sprite_split_y(entry, s_local);
	}
	ent_m.apply_ourdata(entry);
	return 0;
}

#undef ANM_ENTRY_FILTER

void anm_entry_clear(anm_entry_t &entry)
{
	SAFE_DELETE_ARRAY(entry.sprites);
	ZeroMemory(&entry, sizeof(entry));
}
/// -------------

int patch_png_load_for_thtx(png_image_ex &image, const json_t *patch_info, const char *fn, thtx_header_t *thtx)
{
	void *file_buffer = NULL;
	size_t file_size;

	if(!thtx) {
		return -1;
	}

	SAFE_FREE(image.buf);
	png_image_free(&image.img);
	ZeroMemory(&image.img, sizeof(png_image));
	image.img.version = PNG_IMAGE_VERSION;

	file_buffer = patch_file_load(patch_info, fn, &file_size);
	if(!file_buffer) {
		return 2;
	}

	if(png_image_begin_read_from_memory(&image.img, file_buffer, file_size)) {
		image.img.format = format_png_equiv((format_t)thtx->format);
		if(image.img.format != PNG_FORMAT_INVALID) {
			size_t png_size = PNG_IMAGE_SIZE(image.img);
			image.buf = (png_bytep)malloc(png_size);

			if(image.buf) {
				png_image_finish_read(&image.img, 0, image.buf, 0, NULL);
			}
		}
	}
	SAFE_FREE(file_buffer);
	if(image.buf) {
		unsigned int pixels = image.img.width * image.img.height;
		format_from_bgra(image.buf, pixels, (format_t)thtx->format);
	}
	return !image.buf;
}

// Patches an [image] prepared by png_load_for_thtx() into [entry].
// Patching will be performed on sprite level if [entry.sprites] and
// [entry.sprite_num] are valid.
// [png] is assumed to have the same bit depth as the texture in [entry].
int patch_thtx(anm_entry_t &entry, png_image_ex &image)
{
	if(!image.buf) {
		return -1;
	}
	if(entry.sprites && entry.sprite_num > 1) {
		size_t i;
		for(i = 0; i < entry.sprite_num; i++) {
			sprite_patch_t sp;
			if(!sprite_patch_set(sp, entry, &entry.sprites[i], image)) {
				sprite_patch(sp);
			}
		}
	} else {
		// Construct a fake sprite covering the entire texture
		sprite_local_t sprite = {0};
		sprite_patch_t sp = {0};

		sprite.w = entry.w;
		sprite.h = entry.h;
		if(!sprite_patch_set(sp, entry, &sprite, image)) {
			return sprite_patch(sp);
		}
	}
	return 0;
}

// Helper function for stack_game_png_apply.
int patch_png_apply(anm_entry_t &entry, const json_t *patch_info, const char *fn)
{
	int ret = -1;
	if(patch_info && fn) {
		png_image_ex png = {0};
		ret = patch_png_load_for_thtx(png, patch_info, fn, entry.thtx);
		if(!ret) {
			patch_thtx(entry, png);
			patch_print_fn(patch_info, fn);
		}
		SAFE_FREE(png.buf);
	}
	return ret;
}

int stack_game_png_apply(anm_entry_t &entry)
{
	int ret = -1;
	if(entry.thtx && entry.name) {
		stack_chain_iterate_t sci = {0};
		json_t *chain = resolve_chain_game(entry.name);
		ret = 0;
		if(json_array_size(chain)) {
			log_printf("(PNG) Resolving %s... ", json_array_get_string(chain, 0));
		}
		while(stack_chain_iterate(&sci, chain, SCI_FORWARDS, NULL)) {
			if(!patch_png_apply(entry, sci.patch_info, sci.fn)) {
				ret = 1;
			}
		}
		log_printf(ret ? "\n" : "not found\n");
		json_decref(chain);
	}
	return ret;
}

int patch_anm(void *file_inout, size_t size_out, size_t size_in, const char *fn, json_t *patch)
{
	(void)fn;
	json_t *dat_dump = json_object_get(runconfig_get(), "dat_dump");

	// Some ANMs reference the same file name multiple times in a row
	const char *name_prev = NULL;

	header_mods_t hdr_m(patch);
	anm_entry_t entry = {0};
	png_image_ex png = {0};
	png_image_ex bounds = {0};

	auto *anm_entry_out = (uint8_t *)file_inout;
	auto *endptr = (uint8_t *)(file_inout) + size_in;

	log_printf("---- ANM ----\n");

	while(anm_entry_out && anm_entry_out < endptr) {
		if(anm_entry_init(hdr_m, entry, anm_entry_out, patch)) {
			log_printf("Corrupt ANM file or format definition, aborting ...\n");
			break;
		}
		if(entry.hasdata) {
			if(!name_prev || strcmp(entry.name, name_prev)) {
				if(!json_is_false(dat_dump)) {
					bounds_store(name_prev, bounds);
					bounds_init(bounds, entry.w, entry.h, entry.name);
				}
				name_prev = entry.name;
			}
			png_image_resize(bounds, entry.x + entry.w, entry.y + entry.h);
			if(entry.sprites) {
				size_t i;
				for(i = 0; i < entry.sprite_num; i++) {
					bounds_draw_rect(bounds, entry.x, entry.y, &entry.sprites[i]);
				}
			}
			// Do the patching
			stack_game_png_apply(entry);
		}
		if(!entry.nextoffset) {
			bounds_store(name_prev, bounds);
			anm_entry_out = NULL;
		}
		anm_entry_out += entry.nextoffset;
		anm_entry_clear(entry);
	}
	png_image_clear(bounds);
	png_image_clear(png);

	log_printf("-------------\n");
	return 1;
}
