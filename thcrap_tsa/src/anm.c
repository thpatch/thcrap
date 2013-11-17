/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * On-the-fly ANM patcher.
  *
  * Portions adapted from xarnonymous' Touhou Toolkit
  * http://code.google.com/p/thtk/
  */

#include <thcrap.h>
#include <png.h>
#include "thcrap_tsa.h"
#include "anm.h"

/// JSON-based structure data access
/// --------------------------------
int struct_get(void *dest, size_t dest_size, void *src, json_t *spec)
{
	if(!dest || !dest_size || !src || !spec) {
		return -1;
	}
	{
		size_t offset = json_object_get_hex(spec, "offset");
		size_t size = json_object_get_hex(spec, "size");
		// Default to architecture word size
		if(!size) {
			size = sizeof(size_t);
		};
		if(size > dest_size) {
			return -2;
		}
		ZeroMemory(dest, dest_size);
		memcpy(dest, (char*)src + offset, size);
	}
	return 0;
}

#define STRUCT_GET(type, val, src, spec_obj) \
	struct_get(&(val), sizeof(type), src, json_object_get(spec_obj, #val))
/// --------------------------------

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

unsigned int format_png_equiv(format_t format)
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
			return 0;
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

void format_blend(png_bytep dst, png_bytep rep, unsigned int pixels, format_t format)
{
	unsigned int i;
	if(format == FORMAT_BGRA8888) {
		for(i = 0; i < pixels; ++i, dst += 4, rep += 4) {
			const int dst_alpha = 0xff - rep[3];

			dst[0] = (dst[0] * dst_alpha + rep[0] * rep[3]) >> 8;
			dst[1] = (dst[1] * dst_alpha + rep[1] * rep[3]) >> 8;
			dst[2] = (dst[2] * dst_alpha + rep[2] * rep[3]) >> 8;
			dst[3] = (dst[3] * dst_alpha + rep[3] * rep[3]) >> 8;
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
			const int dst_alpha = 0xf - rep_a;

			dst[1] =
				(dst_a * dst_alpha + rep_a * rep_a & 0xf0) |
				((dst_r * dst_alpha + rep_r * rep_a) >> 4);
			dst[0] =
				(dst_g * dst_alpha + rep_g * rep_a & 0xf0) |
				((dst_b * dst_alpha + rep_b * rep_a) >> 4);
		}
	} else {
		// Other formats have no alpha channel, so we can just do...
		memcpy(dst, rep, pixels * format_Bpp(format));
	}
}
/// -------

/// Sprite-level patching
/// ---------------------
int sprite_patch_set(
	sprite_patch_t *sp,
	const size_t thtx_x,
	const size_t thtx_y,
	thtx_header_t *thtx,
	const sprite_t *sprite,
	const png_image_exp image
)
{
	if(!sp || !thtx || !sprite || !image || !image->buf) {
		return -1;
	}
	ZeroMemory(sp, sizeof(*sp));

	// Note that we don't use the PNG_IMAGE_* macros here - the actual bit depth
	// after format_from_bgra() may no longer be equal to the one in the PNG header.
	sp->format = thtx->format;
	sp->bpp = format_Bpp(sp->format);

	sp->dst_x = (png_uint_32)sprite->x;
	sp->dst_y = (png_uint_32)sprite->y;

	sp->rep_x = thtx_x + sp->dst_x;
	sp->rep_y = thtx_y + sp->dst_y;

	if(sp->rep_x >= image->img.width || sp->rep_y >= image->img.height) {
		return 2;
	}

	sp->rep_stride = image->img.width * sp->bpp;
	sp->dst_stride = thtx->w * sp->bpp;

	sp->copy_w = min((png_uint_32)sprite->w, (image->img.width - sp->rep_x));
	sp->copy_h = min((png_uint_32)sprite->h, (image->img.height - sp->rep_y));

	sp->dst_buf = thtx->data + (sp->dst_y * sp->dst_stride) + (sp->dst_x * sp->bpp);
	sp->rep_buf = image->buf + (sp->rep_y * sp->rep_stride) + (sp->rep_x * sp->bpp);
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

sprite_alpha_t sprite_alpha_analyze_rep(const sprite_patch_t *sp)
{
	if(sp) {
		return sprite_alpha_analyze(sp->rep_buf, sp->format, sp->rep_stride, sp->copy_w, sp->copy_h);
	} else {
		return SPRITE_ALPHA_EMPTY;
	}
}

sprite_alpha_t sprite_alpha_analyze_dst(const sprite_patch_t *sp)
{
	if(sp) {
		return sprite_alpha_analyze(sp->dst_buf, sp->format, sp->dst_stride, sp->copy_w, sp->copy_h);
	} else {
		return SPRITE_ALPHA_EMPTY;
	}
}

int sprite_replace(const sprite_patch_t *sp)
{
	if(sp) {
		png_uint_32 row;
		png_bytep dst_buf = sp->dst_buf;
		png_bytep rep_buf = sp->rep_buf;
		size_t copy_stride = sp->copy_w * sp->bpp;
		for(row = 0; row < sp->copy_h; row++) {
			memcpy(dst_buf, rep_buf, copy_stride);

			dst_buf += sp->dst_stride;
			rep_buf += sp->rep_stride;
		}
		return 0;
	}
	return -1;
}

int sprite_blend(const sprite_patch_t *sp)
{
	if(sp) {
		png_uint_32 row;
		png_bytep dst_p = sp->dst_buf;
		png_bytep rep_p = sp->rep_buf;
		for(row = 0; row < sp->copy_h; row++) {
			format_blend(dst_p, rep_p, sp->copy_w, sp->format);
			dst_p += sp->dst_stride;
			rep_p += sp->rep_stride;
		}
		return 0;
	}
	return -1;
}

sprite_alpha_t sprite_patch(const sprite_patch_t *sp)
{
	sprite_alpha_t rep_alpha = sprite_alpha_analyze_rep(sp);
	if(rep_alpha != SPRITE_ALPHA_EMPTY) {
		sprite_alpha_t dst_alpha = sprite_alpha_analyze_dst(sp);
		if(dst_alpha == SPRITE_ALPHA_OPAQUE) {
			sprite_blend(sp);
		} else {
			sprite_replace(sp);
		}
	}
	return rep_alpha;
}
/// ---------------------

int png_load_for_thtx(png_image_exp image, const char *fn, thtx_header_t *thtx)
{
	void *file_buffer = NULL;
	size_t file_size;

	if(!image || !thtx) {
		return -1;
	}

	SAFE_FREE(image->buf);
	png_image_free(&image->img);
	ZeroMemory(&image->img, sizeof(png_image));
	image->img.version = PNG_IMAGE_VERSION;

	if(strncmp(thtx->magic, "THTX", sizeof(thtx->magic))) {
		return 1;
	}

	file_buffer = stack_game_file_resolve(fn, &file_size);
	if(!file_buffer) {
		return 2;
	}

	if(png_image_begin_read_from_memory(&image->img, file_buffer, file_size)) {
		image->img.format = format_png_equiv(thtx->format);
		if(image->img.format) {
			size_t png_size = PNG_IMAGE_SIZE(image->img);
			image->buf = (png_bytep)malloc(png_size);

			if(image->buf) {
				png_image_finish_read(&image->img, 0, image->buf, 0, NULL);
			}
		}
	}
	SAFE_FREE(file_buffer);
	if(image->buf) {
		format_from_bgra(image->buf, image->img.width * image->img.height, thtx->format);
	}
	return 0;
}

// Patches an [image] prepared by <png_load_for_thtx> into [thtx], starting at [x],[y].
// [png] is assumed to have the same bit depth as [thtx].
int patch_thtx(thtx_header_t *thtx, const size_t x, const size_t y, png_image_exp image)
{
	if(thtx) {
		sprite_t sprite = {0};
		sprite_patch_t sp = {0};

		// Construct a fake sprite covering the entire texture
		sprite.w = thtx->w;
		sprite.h = thtx->h;
		if(!sprite_patch_set(&sp, x, y, thtx, &sprite, image)) {
			return sprite_patch(&sp);
		}
		return 0;
	}
	return -1;
}

int patch_anm(BYTE *file_inout, size_t size_out, size_t size_in, json_t *patch, json_t *run_cfg)
{
	json_t *format = runconfig_format_get("anm");
	json_t *dat_dump = json_object_get(run_cfg, "dat_dump");
	size_t headersize = json_object_get_hex(format, "headersize");

	// Some ANMs reference the same file name multiple times in a row
	const char *name_prev = NULL;

	png_image_ex png = {0};
	png_image_ex bounds = {0};

	BYTE *anm_entry_out = file_inout;

	if(!format) {
		return 1;
	}

	log_printf("---- ANM ----\n");

	if(!headersize) {
		log_printf("(no ANM header size given, sprite-local patching disabled)\n");
	}

	while(anm_entry_out < file_inout + size_in) {
		size_t x;
		size_t y;
		size_t nameoffset;
		size_t thtxoffset;
		size_t hasdata;
		size_t nextoffset;
		size_t sprites;

		if(
			STRUCT_GET(size_t, x, anm_entry_out, format) ||
			STRUCT_GET(size_t, y, anm_entry_out, format) ||
			STRUCT_GET(size_t, nameoffset, anm_entry_out, format) ||
			STRUCT_GET(size_t, thtxoffset, anm_entry_out, format) ||
			STRUCT_GET(size_t, hasdata, anm_entry_out, format) ||
			STRUCT_GET(size_t, nextoffset, anm_entry_out, format) ||
			STRUCT_GET(size_t, sprites, anm_entry_out, format)
		) {
			log_printf("Corrupt ANM file or format definition, aborting ...\n");
			break;
		}
		if(hasdata && thtxoffset) {
			const char *name = (const char*)(anm_entry_out + nameoffset);
			thtx_header_t *thtx = (thtx_header_t*)(anm_entry_out + thtxoffset);

			// Load a new replacement image, if necessary.
			if(!name_prev || strcmp(name, name_prev)) {
				png_load_for_thtx(&png, name, thtx);

				if(!json_is_false(dat_dump)) {
					bounds_store(name_prev, &bounds);
					bounds_init(&bounds, thtx, name);
				}
				name_prev = name;
			}
			bounds_resize(&bounds, x + thtx->w, y + thtx->h);

			// Perform sprite-level ("advanced") patching if we have a header size
			// and fall back to basic patching otherwise.
			if(headersize) {
				size_t i;
				DWORD *sprite_ptr = (DWORD*)(anm_entry_out + headersize);
				for(i = 0; i < sprites; i++, sprite_ptr++) {
					sprite_patch_t sp;
					sprite_t *sprite = (sprite_t*)(anm_entry_out + sprite_ptr[0]);
					bounds_draw_rect(&bounds, x, y, sprite);
					if(!sprite_patch_set(&sp, x, y, thtx, sprite, &png)) {
						sprite_patch(&sp);
					}
				}
			} else {
				patch_thtx(thtx, x, y, &png);
			}
		}
		if(!nextoffset) {
			bounds_store(name_prev, &bounds);
			break;
		}
		anm_entry_out += nextoffset;
	}
	SAFE_FREE(bounds.buf);
	SAFE_FREE(png.buf);
	log_printf("-------------\n");
	return 0;
}
