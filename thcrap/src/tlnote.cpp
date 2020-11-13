/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * On-screen display of translation notes.
  */

#include "thcrap.h"
#include <vector>
#include "minid3d.h"
#include "textdisp.h"
#include "tlnote.hpp"

#pragma comment(lib, "winmm.lib")

#define COM_ERR_WRAP(func, ...) { \
	HRESULT d3d_ret = func(__VA_ARGS__); \
	if(FAILED(d3d_ret)) { \
		return fail(#func, d3d_ret); \
	} \
}

/// Codepoints
/// ----------
/// All of these are UTF-8-encoded.

// Separates in-line TL note text (after) from regular text (before).
// This is the codepoint that should be used by patch authors.
#define TLNOTE_INLINE 0x14

// Indicates that the next index is a 1-based, UTF-8-encoded internal index of
// a previously rendered TL note. Can appear either at the beginning or the
// end of a string.
#define TLNOTE_INDEX 0x12
/// ----------

/// Error reporting and debugging
/// -----------------------------
logger_t tlnote_log("TL note error");
/// -----------------------------

/// Structures
/// ----------
tlnote_t::tlnote_t(const stringref_t str)
	: str((tlnote_string_t *)str.str), len(str.len - 1) {
	assert(str.str[0] == TLNOTE_INLINE || str.str[0] == TLNOTE_INDEX);
	assert(str.len > 1);
}
/// ----------

/// Rendering values
/// ----------------
Option<valign_t> tlnote_valign_value(const json_t *val)
{
	auto str = json_string_value(val);
	if(!str) {
		return {};
	}
	if(!strcmp(str, "top")) {
		return valign_t::top;
	} else if(!strcmp(str, "center")) {
		return valign_t::center;
	} else if(!strcmp(str, "bottom")) {
		return valign_t::bottom;
	}
	return {};
}

void font_delete(HFONT font)
{
	if(!font) {
		return;
	}
	typedef BOOL WINAPI DeleteObject_type(
		HGDIOBJ obj
	);
	((DeleteObject_type *)detour_top(
		"gdi32.dll", "DeleteObject", (FARPROC)DeleteObject
	))(font);
}

THCRAP_API bool tlnote_env_render_t::reference_resolution_set(const vector2_t &newval)
{
	if(newval.x <= 0 || newval.y <= 0) {
		tlnote_log.errorf(
			"Reference resolution must be positive and nonzero, got %f\xE2\x9C\x95%f",
			newval.x, newval.y
		);
		return false;
	}
	_reference_resolution = newval;
	return true;
}

THCRAP_API bool tlnote_env_render_t::font_set(const LOGFONTA &lf)
{
	font_delete(_font);
	// Must be CreateFont() rather than CreateFontIndirect() to
	// continue supporting the legacy "font" runconfig key.
	_font = ((CreateFontA_type *)detour_top(
		"gdi32.dll", "CreateFontA", (FARPROC)CreateFontU
	))(
		lf.lfHeight, lf.lfWidth, lf.lfEscapement, lf.lfOrientation,
		lf.lfWeight, lf.lfItalic, lf.lfUnderline, lf.lfStrikeOut,
		lf.lfCharSet, lf.lfOutPrecision, lf.lfClipPrecision,
		lf.lfQuality, lf.lfPitchAndFamily, lf.lfFaceName
	);
	return true;
}

THCRAP_API bool tlnote_env_t::region_size_set(const vector2_t &newval)
{
	const auto &rr = _reference_resolution;
	if(
		newval.x <= 0 || newval.y <= 0 || newval.x >= rr.x || newval.y >= rr.y
	) {
		tlnote_log.errorf(
			"Region size must be nonzero and smaller than the reference resolution (%f\xE2\x9C\x95%f), got %f\xE2\x9C\x95%f",
			rr.x, rr.y, newval.x, newval.y
		);
		return false;
	}
	_region_w = newval.x;
	region_h = newval.y;
	return true;
}

THCRAP_API xywh_t tlnote_env_t::scale_to(const vector2_t &resolution, const xywh_t &val)
{
	const auto &rr = _reference_resolution;
	vector2_t scale = { resolution.x / rr.x, resolution.y / rr.y };
	return {
		val.x * scale.x, val.y * scale.y,
		val.w * scale.x, val.h * scale.y
	};
}

bool tlnote_env_from_runconfig(tlnote_env_t &env)
{
	auto fail = [] (const char *context, const char *err) {
		tlnote_log.errorf("{\"tlnotes\": {\"%s\"}}: %s", context, err);
		return false;
	};

#define PARSE(var, func, fail_check, on_fail, on_success) { \
	auto json_value = json_object_get(cfg, #var); \
	if(json_value) { \
		auto parsed = func(json_value); \
		if(fail_check) on_fail else on_success \
	} \
}

#define PARSE_TUPLE(var, func, on_success) \
	PARSE(var, func, (!parsed.err.empty()), { \
		fail(#var, parsed.err.c_str()); \
	}, on_success)

#define PARSE_FLOAT_POSITIVE(var) \
	PARSE(var, json_number_value, (parsed <= 0.0f), { \
		fail(#var, "Must be a positive, nonzero real number."); \
	}, { \
		env.var = (float)parsed; \
	});

	auto cfg = json_object_get(runconfig_json_get(), "tlnotes");
	if(!cfg) {
		return true;
	}

	bool ret = true;

	PARSE_TUPLE(reference_resolution, json_vector2_value, {
		ret &= env.reference_resolution_set(parsed.v);
	});
	PARSE_TUPLE(region_topleft, json_vector2_value, {
		env.region_left = parsed.v.x;
		env.region_top = parsed.v.y;
	});
	PARSE_TUPLE(region_size, json_vector2_value, {
		ret &= env.region_size_set(parsed.v);
	});
	PARSE(font, json_string_value, (parsed == nullptr), {
		fail("font", "Must be a font rule string.");
	}, {
		LOGFONTA lf = {};
		fontrule_parse(&lf, parsed);
		ret &= env.font_set(lf);
	});

	PARSE(outline_radius, json_integer_value, (
		!json_is_integer(json_value)
		|| parsed < 0 || parsed > OUTLINE_RADIUS_MAX
	), {
		fail("outline_radius", "Must be an integer between 0 and " OUTLINE_RADIUS_MAX_STR ".");
	}, {
		env.outline_radius = (uint8_t)parsed;
	});

	PARSE_FLOAT_POSITIVE(fade_ms);
	PARSE_FLOAT_POSITIVE(read_speed);

	PARSE(valign, tlnote_valign_value, (parsed.is_none()), {
		fail("valign", TLNOTE_VALIGN_ERROR);
	}, {
		env.valign = parsed.unwrap();
	});

#undef PARSE_FLOAT_POSITIVE
#undef PARSE_TUPLE
#undef PARSE

	return ret;
}

tlnote_env_t& tlnote_env()
{
	static struct tlnote_env_static_wrap_t {
		tlnote_env_t env;

		// TODO: We might want to continuously nag the patch author once
		// we actually make the runconfig repatchable, but for now...
		tlnote_env_static_wrap_t() {
			tlnote_env_from_runconfig(env);
		}
		~tlnote_env_static_wrap_t() {
			font_delete(env.font());
		}
	} env_wrap;
	return env_wrap.env;
}
/// ----------------

/// Direct3D pointers
/// -----------------
// Game support modules might want to pre-render TL notes before the game
// created its IDirect3DDevice instance.
d3d_version_t d3d_ver = D3D_NONE;
IDirect3DDevice *d3dd = nullptr;
/// -----------------

/// Texture creation
/// ----------------
struct tlnote_rendered_t {
	// "Primary key" data
	// -------------------
	std::string note;
	tlnote_env_render_t render_env;
	// -------------------

	// Runtime stuff
	// -------------
	IDirect3DTexture *tex = nullptr;
	unsigned int tex_w;
	unsigned int tex_h;
	// -------------

	IDirect3DTexture* render(d3d_version_t ver, IDirect3DDevice *d3dd);

	bool matches(const stringref_t &n2, const tlnote_env_render_t &r2) const {
		return
			(note.size() == n2.len)
			&& (render_env == r2)
			&& (note.compare(n2.str) == 0);
	}

	tlnote_rendered_t(const stringref_t &note, tlnote_env_render_t render_env)
		: note({ note.str, (size_t)note.len }), render_env(render_env) {
	}
};

IDirect3DTexture* tlnote_rendered_t::render(d3d_version_t ver, IDirect3DDevice *d3dd)
{
	if(tex) {
		return tex;
	}

	const stringref_t formatted_note = { note.c_str(), note.length() };

	auto fail = [formatted_note] (const char *func, HRESULT ret) {
		log_printf(
			"(TL notes) Error rendering \"%.*s\": %s returned 0x%x\n",
			formatted_note.len, formatted_note.str, func, ret
		);
		return nullptr;
	};

	auto hDC = CreateCompatibleDC(GetDC(0));
	SelectObject(hDC, render_env.font());

	RECT gdi_rect = { 0, 0, (LONG)render_env.region_w(), 0 };
	DrawText(hDC,
		formatted_note.str, formatted_note.len,
		&gdi_rect, DT_CALCRECT | DT_WORDBREAK | DT_CENTER
	);

	// We obviously want 24-bit for ClearType in case the font supports it.
	// No need to waste the extra 8 bits for an "alpha channel" that GDI
	// doesn't support and sets to 0 on every rendering call anyway, though.
	BITMAPINFOHEADER bmi = {};
	bmi.biSize = sizeof(bmi);
	bmi.biBitCount = 24;
	bmi.biWidth = gdi_rect.right;
	bmi.biHeight = -gdi_rect.bottom;
	bmi.biPlanes = 1;
	bmi.biCompression = BI_RGB;

	struct dib_pixel_t {
		uint8_t r, g, b;
	};

	uint8_t *dib_bits = nullptr;
	auto hBitmap = CreateDIBSection(hDC,
		(BITMAPINFO *)&bmi, 0, (void**)&dib_bits, nullptr, 0
	);
	if(!hBitmap) {
		return fail("CreateDIBSection", GetLastError());
	}
	SelectObject(hDC, hBitmap);

	SetTextColor(hDC, 0xFFFFFF);
	SetBkColor(hDC, 0x000000);
	SetBkMode(hDC, TRANSPARENT);

	DrawText(hDC,
		formatted_note.str, formatted_note.len,
		&gdi_rect, DT_WORDBREAK | DT_CENTER
	);

	BITMAP bmp;
	GetObject(hBitmap, sizeof(bmp), &bmp);

	const auto &outline_radius = render_env.outline_radius;
	tex_w = gdi_rect.right + (outline_radius * 2);
	tex_h = gdi_rect.bottom + (outline_radius * 2);

	// TODO: Is there still 3dfx Voodoo-style limited 3D hardware
	// out there today that needs to be worked around?
	COM_ERR_WRAP(d3dd_CreateTexture, ver, d3dd,
		tex_w, tex_h, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex
	);

	struct d3d_pixel_t {
		uint8_t b, g, r, a;
	};

	D3DSURFACE_DESC tex_desc;
	D3DLOCKED_RECT lockedrect;
	COM_ERR_WRAP(d3dtex_GetLevelDesc, ver, tex, 0, &tex_desc);
	COM_ERR_WRAP(d3dtex_LockRect, ver, tex, 0, &lockedrect, nullptr, 0);

	auto tex_bits = (uint8_t *)lockedrect.pBits;

	// Shift the texture write pointer by the outline height
	tex_bits += lockedrect.Pitch * outline_radius;

	auto outside_circle = [] (int x, int y, int radius) {
		return (x * x) + (y * y) > (radius * radius);
	};

	for(LONG y = 0; y < gdi_rect.bottom; y++) {
		auto dib_col = ((dib_pixel_t *)dib_bits);
		// Shift the texture write pointer by the outline width
		auto tex_col = ((d3d_pixel_t *)tex_bits) + outline_radius;

		for(LONG x = 0; x < gdi_rect.right; x++) {
			auto alpha = MAX(MAX(dib_col->r, dib_col->g), dib_col->b);
			tex_col->r = dib_col->r;
			tex_col->g = dib_col->g;
			tex_col->b = dib_col->b;
			if(alpha > 0) {
				for(int oy = -outline_radius; oy <= outline_radius; oy++) {
					for(int ox = -outline_radius; ox <= outline_radius; ox++) {
						if(outside_circle(ox, oy, outline_radius)) {
							continue;
						}
						auto *outline_p = (d3d_pixel_t*)(
							((uint8_t*)tex_col) - (lockedrect.Pitch * oy)
						) + ox;
						outline_p->a = MAX(outline_p->a, alpha);
					}
				}
			}
			dib_col++;
			tex_col++;
		}
		tex_bits += lockedrect.Pitch;
		dib_bits += bmp.bmWidthBytes;
	}
	COM_ERR_WRAP(d3dtex_UnlockRect, ver, tex, 0);

	DeleteObject(hBitmap);
	DeleteDC(hDC);
	return tex;
}
/// ----------------

/// Indexing of rendered TL notes
/// -----------------------------
std::vector<tlnote_rendered_t> rendered;

// The encoded index of RENDERED_NONE, which can be used for timed removal of
// TL notes, will be (this number + RENDERED_OFFSET), which must be encodable
// in UTF-8. -1 for RENDERED_NONE and 2 for RENDERED_OFFSET means that
// RENDERED_NONE encodes to U+0001, the smallest possible encodable number
// (since U+0000 would immediately terminate the string), with rendered[0]
// mapping to 2.

#define RENDERED_NONE -1

#define SURROGATE_START (0xD800)
#define SURROGATE_END   (0xE000)
#define SURROGATE_COUNT (SURROGATE_END - SURROGATE_START)

// Encoded index that maps to the first entry in [rendered].
#define RENDERED_OFFSET 2
// Excluding '\0' as well as the UTF-16 surrogate halves (how nice of us!)
#define RENDERED_MAX (0x10FFFF - SURROGATE_COUNT)

// These basically follow the UTF-8 spec, except that the index can include
// the range of UTF-16 surrogate halves.
tlnote_encoded_index_t::tlnote_encoded_index_t(int32_t index)
{
	assert(index >= 1 && index <= RENDERED_MAX);
	type = TLNOTE_INDEX;
	if(index >= SURROGATE_START) {
		index += SURROGATE_COUNT;
	}

	if(index < 0x80) {
		index_as_utf8[0] = (char)index;
		index_len = 1;
	} else if(index < 0x800) {
		index_as_utf8[0] = 0xC0 + ((index & 0x7C0) >> 6);
		index_as_utf8[1] = 0x80 + ((index & 0x03F));
		index_len = 2;
	} else if(index < 0x10000) {
		index_as_utf8[0] = 0xE0 + ((index & 0xF000) >> 12);
		index_as_utf8[1] = 0x80 + ((index & 0x0FC0) >> 6);
		index_as_utf8[2] = 0x80 + ((index & 0x003F));
		index_len = 3;
	} else if(index <= 0x10FFFF) {
		index_as_utf8[0] = 0xF0 + ((index & 0x1C0000) >> 18);
		index_as_utf8[1] = 0x80 + ((index & 0x03F000) >> 12);
		index_as_utf8[2] = 0x80 + ((index & 0x000FC0) >> 6);
		index_as_utf8[3] = 0x80 + ((index & 0x00003F));
		index_len = 4;
	}
}

// Returns -1 and [byte_len] = 0 on failure.
int32_t index_from_utf8(unsigned char &byte_len, const char buf[4])
{
	auto fail = [&byte_len] () {
		byte_len = 0;
		return -1;
	};
	unsigned char codepoint_bits;
	unsigned char bits_seen;
	if((buf[0] & 0x80) == 0) {
		byte_len = 1;
		return buf[0];
	} else if((buf[0] & 0xe0) == 0xc0) {
		codepoint_bits = 11;
		bits_seen = 5;
		byte_len = 2;
	} else if((buf[0] & 0xf0) == 0xe0) {
		codepoint_bits = 16;
		bits_seen = 4;
		byte_len = 3;
	} else if((buf[0] & 0xf8) == 0xf0) {
		codepoint_bits = 21;
		bits_seen = 3;
		byte_len = 4;
	} else {
		return fail();
	}
	int32_t bits = buf[0] & (0xff >> (8 - bits_seen));
	int32_t index = bits << (codepoint_bits - bits_seen);
	unsigned char i = 1;
	for(; i < byte_len; i++) {
		if((buf[i] & 0xc0) != 0x80) {
			return fail();
		}
		bits_seen += 6;
		bits = buf[i] & 0x3f;
		index |= bits << (codepoint_bits - bits_seen);
	}
	if(index >= SURROGATE_START) {
		index -= SURROGATE_COUNT;
	}
	assert(index >= 1 && index <= RENDERED_MAX);
	return index;
}

bool operator ==(const std::string &a, const stringref_t &b)
{
	return (a.size() == b.len) && (a.compare(b.str) == 0);
}

int32_t tlnote_render(const stringref_t &note)
{
	const auto &env = tlnote_env();
	if(!env.complete()) {
		log_printf("(TL notes) No region defined\n");
		return RENDERED_NONE;
	}
	for(int32_t i = 0; i < (int32_t)rendered.size(); i++) {
		if(rendered[i].matches(note, env)) {
			return i;
		}
	}

	tlnote_rendered_t r{ note, env };
	auto index = rendered.size();
	if(index >= RENDERED_MAX) {
		// You absolute madman.
		static int32_t madness_index = 0;
		index = madness_index;
		madness_index = (madness_index + 1) % RENDERED_MAX;
		rendered[index] = r;
	}
	rendered.emplace_back(r);

	// Render any outstanding TL notes that were created
	// while we didn't have an IDirect3DDevice.
	static bool got_unrendered_notes = false;
	if(d3d_ver == D3D_NONE || d3dd == nullptr) {
		got_unrendered_notes = true;
	} else if(got_unrendered_notes) {
		for(auto &v : rendered) {
			v.render(d3d_ver, d3dd);
		}
		got_unrendered_notes = false;
	} else {
		rendered.back().render(d3d_ver, d3dd);
	}
	return index;
}
/// -----------------------------

/// Render calls and detoured functions
/// -----------------------------------
d3dd_EndScene_type *chain_d3dd8_EndScene;
d3dd_EndScene_type *chain_d3dd9_EndScene;

int32_t id_active = RENDERED_NONE;

bool tlnote_frame(d3d_version_t ver, IDirect3DDevice *d3dd)
{
	d3d_ver = ver;
	::d3dd = d3dd;

	// What do we render?
	// ------------------
	static decltype(id_active) id_last = RENDERED_NONE;
	static DWORD time_birth = 0;
	defer({ id_last = id_active; });
	if(id_active == RENDERED_NONE) {
		return true;
	}
	auto now = timeGetTime();
	if(id_last != id_active) {
		time_birth = now;
	}
	// OK, *something.*
	// ------------------

	auto fail = [] (const char *func, HRESULT ret) {
		static const char *func_last = nullptr;
		if(func_last != func) {
			log_printf(
				"(TL notes) Rendering error: %s returned 0x%x\n",
				func, ret
			);
			func_last = func;
		}
		return false;
	};

	// IDirect3DDevice9::Reset() would fail with D3DERR_INVALIDCALL if any
	// state blocks still exist, so we couldn't just have a single static
	// one that we create once. It's fast enough to do this every frame
	// anyway.
	IDirect3DStateBlock sb_game;
	d3dd_CreateStateBlock(ver, d3dd, D3DSBT_ALL, &sb_game);
	d3dd_CaptureStateBlock(ver, d3dd, sb_game);

	// Retrieve back buffer and set viewport
	// -------------------------------------
	IDirect3DSurface *backbuffer = nullptr;
	D3DSURFACE_DESC backbuffer_desc;
	D3DVIEWPORT viewport;

	COM_ERR_WRAP(d3dd_GetBackBuffer, ver, d3dd,
		0, D3DBACKBUFFER_TYPE_MONO, &backbuffer
	);
	defer({ backbuffer->Release(); });

	COM_ERR_WRAP(d3ds_GetDesc, ver, backbuffer, &backbuffer_desc);
	viewport.X = 0;
	viewport.Y = 0;
	viewport.Width = backbuffer_desc.Width;
	viewport.Height = backbuffer_desc.Height;
	viewport.MinZ = 0.0f;
	viewport.MaxZ = 1.0f;

	d3dd_SetViewport(ver, d3dd, &viewport);
	// -------------------------------------

	// Shared render states
	// --------------------
	d3dd_SetRenderState(ver, d3dd, D3DRS_ALPHATESTENABLE, false);
	d3dd_SetRenderState(ver, d3dd, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	d3dd_SetRenderState(ver, d3dd, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	d3dd_SetTextureStageState(ver, d3dd, 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
	d3dd_SetTextureStageState(ver, d3dd, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	// --------------------

	// Render calls
	// ------------
	struct quad_t {
		float left, top, right, bottom;

		quad_t(const xywh_t &xywh)
			: left(xywh.x), top(xywh.y),
			  right(xywh.x + xywh.w), bottom(xywh.y + xywh.h) {
		}
	};
	auto render_textured_quad = [&] (
		const quad_t &q, uint32_t col_diffuse, float texcoord_y, float texcoord_h
	) {
		d3dd_SetFVF(ver, d3dd, D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
		d3dd_SetRenderState(ver, d3dd, D3DRS_ALPHABLENDENABLE, true);
		d3dd_SetTextureStageState(ver, d3dd, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		d3dd_SetTextureStageState(ver, d3dd, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		struct fvf_xyzrhw_diffuse_tex1_t {
			vector3_t pos;
			float rhw;
			uint32_t col_diffuse;
			vector2_t texcoords;
		};
		auto texcoord_top = texcoord_y;
		auto texcoord_bottom = texcoord_y + texcoord_h;
		fvf_xyzrhw_diffuse_tex1_t verts[] = {
			{ {q.left, q.top, 0.0f}, 1.0f, col_diffuse, {0.0f, texcoord_top} },
			{ {q.right, q.top, 0.0f}, 1.0f, col_diffuse, {1.0f, texcoord_top} },
			{ {q.left, q.bottom, 0.0f}, 1.0f, col_diffuse, {0.0f, texcoord_bottom} },
			{ {q.right, q.bottom, 0.0f}, 1.0f, col_diffuse, {1.0f, texcoord_bottom} },
		};
		d3dd_DrawPrimitiveUP(ver, d3dd, D3DPT_TRIANGLESTRIP,
			elementsof(verts) - 2, verts, sizeof(verts[0])
		);
	};

#ifdef _DEBUG
	auto render_colored_quad = [&] (quad_t q, uint32_t col_diffuse) {
		d3dd_SetFVF(ver, d3dd, D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
		d3dd_SetRenderState(ver, d3dd, D3DRS_ALPHABLENDENABLE, false);
		d3dd_SetTextureStageState(ver, d3dd, 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
		d3dd_SetTextureStageState(ver, d3dd, 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
		struct fvf_xyzrwh_diffuse_t {
			vector3_t pos;
			float rhw;
			uint32_t col_diffuse;
		};
		fvf_xyzrwh_diffuse_t verts[] = {
			{ {q.left, q.top, 0.0f}, 1.0f, col_diffuse },
			{ {q.right, q.top, 0.0f}, 1.0f, col_diffuse },
			{ {q.right, q.bottom, 0.0f}, 1.0f, col_diffuse },
			{ {q.left, q.bottom, 0.0f}, 1.0f, col_diffuse },
			{ {q.left, q.top, 0.0f}, 1.0f, col_diffuse },
		};
		d3dd_DrawPrimitiveUP(ver, d3dd, D3DPT_LINESTRIP,
			elementsof(verts) - 1, verts, sizeof(verts[0])
		);
	};
#endif
	// ------------

	auto &env = tlnote_env();
	auto tlr = &rendered[id_active];
	auto region_unscaled = env.region();
	float margin_x = (region_unscaled.w - tlr->tex_w);
	region_unscaled.x += margin_x / 2.0f;
	region_unscaled.w -= margin_x;

	d3dd_SetTexture(ver, d3dd, 0, tlr->render(ver, d3dd));

	auto age = now - time_birth;
	float texcoord_y = 0.0f;
	float texcoord_h = 1.0f;
	if(tlr->tex_h > region_unscaled.h) {
		auto ms_per_byte = (1000.0f / env.read_speed);
		auto readtime_ms = (unsigned long)(ms_per_byte * tlr->note.length());
		auto time_per_y = (float)(readtime_ms / tlr->tex_h);
		auto y_cur = (age % readtime_ms) / time_per_y;

		auto region_h_half = (region_unscaled.h / 2);
		auto y_cur_center = y_cur - region_h_half;
		auto scroll = y_cur_center / (tlr->tex_h - region_unscaled.h);
		scroll = MIN(scroll, 1.0f);
		scroll = MAX(scroll, 0.0f);
		texcoord_h = (float)region_unscaled.h / tlr->tex_h;
		texcoord_y = scroll * (1.0f - texcoord_h);
	} else {
		switch(env.valign) {
		case valign_t::top:
			break;
		case valign_t::center:
			region_unscaled.y += (region_unscaled.h / 2.0f) - (tlr->tex_h / 2.0f);
			break;
		case valign_t::bottom:
			region_unscaled.y += region_unscaled.h - tlr->tex_h;
			break;
		}
		region_unscaled.h = (float)tlr->tex_h;
	}

	float alpha = MIN((age / env.fade_ms), 1.0f);

	vector2_t res = { (float)viewport.Width, (float)viewport.Height };
#ifdef _DEBUG
	auto bounds_shadow_col = D3DCOLOR_ARGB(0xFF, 0, 0, 0);
	auto bounds = env.scale_to(res, env.region());
	bounds.w--;
	bounds.h--;

	render_colored_quad(bounds.scaled_by(+1), bounds_shadow_col);
	render_colored_quad(bounds.scaled_by(-1), bounds_shadow_col);
	render_colored_quad(bounds, D3DCOLOR_ARGB(0xFF, 0xFF, 0, 0));
#endif
	auto tlr_quad = env.scale_to(res, region_unscaled);
	auto tlr_col = D3DCOLOR_ARGB((uint8_t)(alpha * 255.0f), 0xFF, 0xFF, 0xFF);
	render_textured_quad(tlr_quad, tlr_col, texcoord_y, texcoord_h);
	d3dd_ApplyStateBlock(ver, d3dd, sb_game);
	d3dd_DeleteStateBlock(ver, d3dd, sb_game);
	return true;
}

HRESULT __stdcall tlnote_d3dd8_EndScene(IDirect3DDevice *that)
{
	tlnote_frame(D3D8, that);
	return chain_d3dd8_EndScene(that);
}

HRESULT __stdcall tlnote_d3dd9_EndScene(IDirect3DDevice *that)
{
	tlnote_frame(D3D9, that);
	return chain_d3dd9_EndScene(that);
}
/// -----------------------------------

THCRAP_API void tlnote_show(const tlnote_t tlnote)
{
	if(!tlnote.str) {
		return;
	}
	int32_t index;
	if(tlnote.str->type == TLNOTE_INLINE) {
		index = tlnote_render(tlnote.str->note);
	} else if(tlnote.str->type == TLNOTE_INDEX) {
		unsigned char byte_len;
		index = index_from_utf8(byte_len, tlnote.str->note);
		index -= RENDERED_OFFSET;
		assert(index < (int32_t)rendered.size());
		assert(byte_len >= 1 && byte_len <= 4);
	} else {
		tlnote_log.errorf("Illegal TL note type? (U+%04X)", tlnote.str->type);
		return;
	}
	id_active = index;
}

THCRAP_API void tlnote_remove()
{
	id_active = RENDERED_NONE;
}

THCRAP_API tlnote_split_t tlnote_find(stringref_t text, bool inline_only)
{
	const char *p = text.str;
	const char *sepchar_ptr = nullptr;
	for(decltype(text.len) i = 0; i < text.len; i++) {
		if(*p == TLNOTE_INLINE) {
			if(sepchar_ptr) {
				tlnote_log.errorf(
					"Duplicate TL note separator character (U+%04X) in\n\n%s",
					*p, text
				);
				break;
			} else {
				sepchar_ptr = p;
			}
		} else if(*p == TLNOTE_INDEX) {
			auto fail = [text] () -> tlnote_split_t {
				tlnote_log.errorf(
					"U+%04X is reserved for internal TL note handling:\n\n%s",
					TLNOTE_INDEX, text
				);
				return { text, {} };
			};
#define FAIL_IF(cond) \
	if(cond) { \
		return fail(); \
	}
			FAIL_IF(inline_only);
			FAIL_IF(sepchar_ptr);
			FAIL_IF(i + 1 >= text.len);
			unsigned char byte_len;
			auto index = index_from_utf8(byte_len, p + 1);
			FAIL_IF(index < 0);
			size_t tlp_len = byte_len + 1;
			FAIL_IF(text.len < i + tlp_len);
			size_t at_end = text.len - (i + tlp_len);
			FAIL_IF(index - RENDERED_OFFSET >= (int)rendered.size());

			tlnote_t tlp = { { p, tlp_len } };
			if(i == 0) {
				return { { p + tlp_len, text.len - tlp_len }, tlp };
			} else if(at_end == 0) {
				return { { text.str, (size_t)(p - text.str) }, tlp };
			} else {
				FAIL_IF(1);
			}
#undef FAIL_IF
		}
		p++;
	}
	if(sepchar_ptr) {
		tlnote_t tlnote = { { sepchar_ptr, (size_t)(p - sepchar_ptr) } };
		return { { text.str, (size_t)(sepchar_ptr - text.str) }, tlnote };
	}
	return { text, {} };
}

THCRAP_API tlnote_encoded_index_t tlnote_prerender(const tlnote_t tlnote)
{
	if(tlnote.str == nullptr || tlnote.str->type != TLNOTE_INLINE) {
		return {};
	}
	stringref_t note = { tlnote.str->note, tlnote.len };
	return { tlnote_render(note) + RENDERED_OFFSET };
}

THCRAP_API tlnote_encoded_index_t tlnote_removal_index()
{
	return { RENDERED_NONE + RENDERED_OFFSET };
}

/// Module functions
/// ----------------
extern "C" __declspec(dllexport) void tlnote_mod_detour(void)
{
	vtable_detour_t d3d8[] = {
		{ 35, (void*)tlnote_d3dd8_EndScene, (void**)&chain_d3dd8_EndScene },
	};
	vtable_detour_t d3d9[] = {
		{ 42, (void*)tlnote_d3dd9_EndScene, (void**)&chain_d3dd9_EndScene },
	};
	d3d8_device_detour(d3d8, elementsof(d3d8));
	d3d9_device_detour(d3d9, elementsof(d3d9));
}
/// ----------------
