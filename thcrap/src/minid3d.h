/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Mini Direct3D combination header, including only what we need so far,
  * with the subset that works for every supported Direct3D version.
  */

#pragma once

/// Calling undetoured, identical functions at different vtable offsets
/// -------------------------------------------------------------------
typedef enum {
	D3D_NONE = 0, D3D8, D3D9
} d3d_version_t;

#define FULLDEC(type, varname) type varname
#define VARNAMES(type, varname) varname

#define MINID3D_TYPEDEF(rettype, func) \
	typedef rettype __stdcall func##_type(func##_(FULLDEC));

#define MINID3D_VTABLE_FUNC_DEF(rettype, func, vtable_index_d3d8, vtable_index_d3d9) \
	MINID3D_TYPEDEF(rettype, func) \
	\
	__inline rettype func(d3d_version_t ver, func##_(FULLDEC)) \
	{ \
		FARPROC *vt = *(FARPROC**)that; \
		func##_type *fp = (func##_type*)( \
			(ver == D3D8) ? vt[vtable_index_d3d8] : \
			(ver == D3D9) ? vt[vtable_index_d3d9] : \
			NULL \
		); \
		assert(fp); \
		return fp(func##_(VARNAMES)); \
	}
/// -------------------------------------------------------------------

// Interfaces
typedef IUnknown IDirect3D;
typedef IUnknown IDirect3DDevice;
// This is just a DWORD token value in Direct3D 8, so let's not make it an
// IUnknown and not tempt anyone to try calling Release() on this.
typedef DWORD IDirect3DStateBlock;
typedef IUnknown IDirect3DSurface;
typedef IUnknown IDirect3DSwapChain;
typedef IUnknown IDirect3DTexture;

// We currently don't care about any of these, but it's nice to have the
// correct types.
typedef uint32_t D3DDEVTYPE;
typedef struct {} D3DPRESENT_PARAMETERS;
typedef uint32_t D3DRESOURCETYPE;

typedef enum _D3DBACKBUFFER_TYPE {
	D3DBACKBUFFER_TYPE_MONO = 0,
	D3DBACKBUFFER_TYPE_LEFT = 1,
	D3DBACKBUFFER_TYPE_RIGHT = 2,

	D3DBACKBUFFER_TYPE_FORCE_DWORD = 0x7fffffff
} D3DBACKBUFFER_TYPE;

typedef enum _D3DBLEND {
	D3DBLEND_ZERO = 1,
	D3DBLEND_ONE = 2,
	D3DBLEND_SRCCOLOR = 3,
	D3DBLEND_INVSRCCOLOR = 4,
	D3DBLEND_SRCALPHA = 5,
	D3DBLEND_INVSRCALPHA = 6,
	D3DBLEND_DESTALPHA = 7,
	D3DBLEND_INVDESTALPHA = 8,
	D3DBLEND_DESTCOLOR = 9,
	D3DBLEND_INVDESTCOLOR = 10,
	D3DBLEND_SRCALPHASAT = 11,
	D3DBLEND_BOTHSRCALPHA = 12,
	D3DBLEND_BOTHINVSRCALPHA = 13,

	D3DBLEND_FORCE_DWORD = 0x7fffffff, /* force 32-bit size enum */
} D3DBLEND;

#define D3DCOLOR_ARGB(a,r,g,b) \
    ((D3DCOLOR)((((a)&0xff)<<24)|(((r)&0xff)<<16)|(((g)&0xff)<<8)|((b)&0xff)))

typedef enum _D3DFORMAT {
	D3DFMT_UNKNOWN = 0,

	D3DFMT_R8G8B8 = 20,
	D3DFMT_A8R8G8B8 = 21,
	D3DFMT_X8R8G8B8 = 22,
	D3DFMT_R5G6B5 = 23,
	D3DFMT_X1R5G5B5 = 24,
	D3DFMT_A1R5G5B5 = 25,
	D3DFMT_A4R4G4B4 = 26,
	D3DFMT_R3G3B2 = 27,
	D3DFMT_A8 = 28,
	D3DFMT_A8R3G3B2 = 29,
	D3DFMT_X4R4G4B4 = 30,
	D3DFMT_A2B10G10R10 = 31,
	D3DFMT_G16R16 = 34,

	D3DFMT_A8P8 = 40,
	D3DFMT_P8 = 41,

	D3DFMT_L8 = 50,
	D3DFMT_A8L8 = 51,
	D3DFMT_A4L4 = 52,

	D3DFMT_V8U8 = 60,
	D3DFMT_L6V5U5 = 61,
	D3DFMT_X8L8V8U8 = 62,
	D3DFMT_Q8W8V8U8 = 63,
	D3DFMT_V16U16 = 64,
	D3DFMT_A2W10V10U10 = 67,

	D3DFMT_UYVY = MAKEFOURCC('U', 'Y', 'V', 'Y'),
	D3DFMT_YUY2 = MAKEFOURCC('Y', 'U', 'Y', '2'),
	D3DFMT_DXT1 = MAKEFOURCC('D', 'X', 'T', '1'),
	D3DFMT_DXT2 = MAKEFOURCC('D', 'X', 'T', '2'),
	D3DFMT_DXT3 = MAKEFOURCC('D', 'X', 'T', '3'),
	D3DFMT_DXT4 = MAKEFOURCC('D', 'X', 'T', '4'),
	D3DFMT_DXT5 = MAKEFOURCC('D', 'X', 'T', '5'),

	D3DFMT_D16_LOCKABLE = 70,
	D3DFMT_D32 = 71,
	D3DFMT_D15S1 = 73,
	D3DFMT_D24S8 = 75,
	D3DFMT_D24X8 = 77,
	D3DFMT_D24X4S4 = 79,


	D3DFMT_VERTEXDATA = 100,
	D3DFMT_INDEX16 = 101,
	D3DFMT_INDEX32 = 102,

	D3DFMT_FORCE_DWORD = 0x7fffffff
} D3DFORMAT;

// Flexible vertex format bits
// ---------------------------
#define D3DFVF_RESERVED0        0x001
#define D3DFVF_POSITION_MASK    0x00E
#define D3DFVF_XYZ              0x002
#define D3DFVF_XYZRHW           0x004
#define D3DFVF_XYZB1            0x006
#define D3DFVF_XYZB2            0x008
#define D3DFVF_XYZB3            0x00a
#define D3DFVF_XYZB4            0x00c
#define D3DFVF_XYZB5            0x00e

#define D3DFVF_NORMAL           0x010
#define D3DFVF_PSIZE            0x020
#define D3DFVF_DIFFUSE          0x040
#define D3DFVF_SPECULAR         0x080

#define D3DFVF_TEXCOUNT_MASK    0xf00
#define D3DFVF_TEXCOUNT_SHIFT   8
#define D3DFVF_TEX0             0x000
#define D3DFVF_TEX1             0x100
#define D3DFVF_TEX2             0x200
#define D3DFVF_TEX3             0x300
#define D3DFVF_TEX4             0x400
#define D3DFVF_TEX5             0x500
#define D3DFVF_TEX6             0x600
#define D3DFVF_TEX7             0x700
#define D3DFVF_TEX8             0x800

#define D3DFVF_LASTBETA_UBYTE4  0x1000

#define D3DFVF_RESERVED2        0xE000  // 4 reserved bits
// ---------------------------

typedef enum _D3DPOOL {
	D3DPOOL_DEFAULT = 0,
	D3DPOOL_MANAGED = 1,
	D3DPOOL_SYSTEMMEM = 2,
	D3DPOOL_SCRATCH = 3,

	D3DPOOL_FORCE_DWORD = 0x7fffffff
} D3DPOOL;

typedef enum _D3DPRIMITIVETYPE {
	D3DPT_POINTLIST = 1,
	D3DPT_LINELIST = 2,
	D3DPT_LINESTRIP = 3,
	D3DPT_TRIANGLELIST = 4,
	D3DPT_TRIANGLESTRIP = 5,
	D3DPT_TRIANGLEFAN = 6,
	D3DPT_FORCE_DWORD = 0x7fffffff, /* force 32-bit size enum */
} D3DPRIMITIVETYPE;

typedef enum _D3DRENDERSTATETYPE {
	D3DRS_ZENABLE = 7,    /* D3DZBUFFERTYPE (or TRUE/FALSE for legacy) */
	D3DRS_FILLMODE = 8,    /* D3DFILLMODE */
	D3DRS_SHADEMODE = 9,    /* D3DSHADEMODE */
	D3DRS_ZWRITEENABLE = 14,   /* TRUE to enable z writes */
	D3DRS_ALPHATESTENABLE = 15,   /* TRUE to enable alpha tests */
	D3DRS_LASTPIXEL = 16,   /* TRUE for last-pixel on lines */
	D3DRS_SRCBLEND = 19,   /* D3DBLEND */
	D3DRS_DESTBLEND = 20,   /* D3DBLEND */
	D3DRS_CULLMODE = 22,   /* D3DCULL */
	D3DRS_ZFUNC = 23,   /* D3DCMPFUNC */
	D3DRS_ALPHAREF = 24,   /* D3DFIXED */
	D3DRS_ALPHAFUNC = 25,   /* D3DCMPFUNC */
	D3DRS_DITHERENABLE = 26,   /* TRUE to enable dithering */
	D3DRS_ALPHABLENDENABLE = 27,   /* TRUE to enable alpha blending */
	D3DRS_FOGENABLE = 28,   /* TRUE to enable fog blending */
	D3DRS_SPECULARENABLE = 29,   /* TRUE to enable specular */
	D3DRS_FOGCOLOR = 34,   /* D3DCOLOR */
	D3DRS_FOGTABLEMODE = 35,   /* D3DFOGMODE */
	D3DRS_FOGSTART = 36,   /* Fog start (for both vertex and pixel fog) */
	D3DRS_FOGEND = 37,   /* Fog end      */
	D3DRS_FOGDENSITY = 38,   /* Fog density  */
	D3DRS_RANGEFOGENABLE = 48,   /* Enables range-based fog */
	D3DRS_STENCILENABLE = 52,   /* BOOL enable/disable stenciling */
	D3DRS_STENCILFAIL = 53,   /* D3DSTENCILOP to do if stencil test fails */
	D3DRS_STENCILZFAIL = 54,   /* D3DSTENCILOP to do if stencil test passes and Z test fails */
	D3DRS_STENCILPASS = 55,   /* D3DSTENCILOP to do if both stencil and Z tests pass */
	D3DRS_STENCILFUNC = 56,   /* D3DCMPFUNC fn.  Stencil Test passes if ((ref & mask) stencilfn (stencil & mask)) is true */
	D3DRS_STENCILREF = 57,   /* Reference value used in stencil test */
	D3DRS_STENCILMASK = 58,   /* Mask value used in stencil test */
	D3DRS_STENCILWRITEMASK = 59,   /* Write mask applied to values written to stencil buffer */
	D3DRS_TEXTUREFACTOR = 60,   /* D3DCOLOR used for multi-texture blend */
	D3DRS_WRAP0 = 128,  /* wrap for 1st texture coord. set */
	D3DRS_WRAP1 = 129,  /* wrap for 2nd texture coord. set */
	D3DRS_WRAP2 = 130,  /* wrap for 3rd texture coord. set */
	D3DRS_WRAP3 = 131,  /* wrap for 4th texture coord. set */
	D3DRS_WRAP4 = 132,  /* wrap for 5th texture coord. set */
	D3DRS_WRAP5 = 133,  /* wrap for 6th texture coord. set */
	D3DRS_WRAP6 = 134,  /* wrap for 7th texture coord. set */
	D3DRS_WRAP7 = 135,  /* wrap for 8th texture coord. set */
	D3DRS_CLIPPING = 136,
	D3DRS_LIGHTING = 137,
	D3DRS_AMBIENT = 139,
	D3DRS_FOGVERTEXMODE = 140,
	D3DRS_COLORVERTEX = 141,
	D3DRS_LOCALVIEWER = 142,
	D3DRS_NORMALIZENORMALS = 143,
	D3DRS_DIFFUSEMATERIALSOURCE = 145,
	D3DRS_SPECULARMATERIALSOURCE = 146,
	D3DRS_AMBIENTMATERIALSOURCE = 147,
	D3DRS_EMISSIVEMATERIALSOURCE = 148,
	D3DRS_VERTEXBLEND = 151,
	D3DRS_CLIPPLANEENABLE = 152,
	D3DRS_POINTSIZE = 154,   /* float point size */
	D3DRS_POINTSIZE_MIN = 155,   /* float point size min threshold */
	D3DRS_POINTSPRITEENABLE = 156,   /* BOOL point texture coord control */
	D3DRS_POINTSCALEENABLE = 157,   /* BOOL point size scale enable */
	D3DRS_POINTSCALE_A = 158,   /* float point attenuation A value */
	D3DRS_POINTSCALE_B = 159,   /* float point attenuation B value */
	D3DRS_POINTSCALE_C = 160,   /* float point attenuation C value */
	D3DRS_MULTISAMPLEANTIALIAS = 161,  // BOOL - set to do FSAA with multisample buffer
	D3DRS_MULTISAMPLEMASK = 162,  // DWORD - per-sample enable/disable
	D3DRS_PATCHEDGESTYLE = 163,  // Sets whether patch edges will use float style tessellation
	D3DRS_DEBUGMONITORTOKEN = 165,  // DEBUG ONLY - token to debug monitor
	D3DRS_POINTSIZE_MAX = 166,   /* float point size max threshold */
	D3DRS_INDEXEDVERTEXBLENDENABLE = 167,
	D3DRS_COLORWRITEENABLE = 168,  // per-channel write enable
	D3DRS_TWEENFACTOR = 170,   // float tween factor
	D3DRS_BLENDOP = 171,   // D3DBLENDOP setting

	D3DRS_FORCE_DWORD = 0x7fffffff, /* force 32-bit size enum */
} D3DRENDERSTATETYPE;

typedef enum _D3DSTATEBLOCKTYPE {
	D3DSBT_ALL = 1, // capture all state
	D3DSBT_PIXELSTATE = 2, // capture pixel state
	D3DSBT_VERTEXSTATE = 3, // capture vertex state
	D3DSBT_FORCE_DWORD = 0x7fffffff,
} D3DSTATEBLOCKTYPE;

typedef struct _D3DSURFACE_DESC {
	D3DFORMAT Format;
	D3DRESOURCETYPE Type;
	DWORD Usage;
	D3DPOOL Pool;
	// Size in Direct3D 8, MultiSampleType in Direct3D 9
	uint32_t Reserved1;
	// MultiSampleType in Direct3D 8, MultiSampleQuality in Direct3D 9
	uint32_t Reserved2;
	UINT Width;
	UINT Height;
} D3DSURFACE_DESC;

/*
 * Values for COLORARG0,1,2, ALPHAARG0,1,2, and RESULTARG texture blending
 * operations set in texture processing stage controls in D3DRENDERSTATE.
 */
#define D3DTA_SELECTMASK        0x0000000f  // mask for arg selector
#define D3DTA_DIFFUSE           0x00000000  // select diffuse color (read only)
#define D3DTA_CURRENT           0x00000001  // select stage destination register (read/write)
#define D3DTA_TEXTURE           0x00000002  // select texture color (read only)
#define D3DTA_TFACTOR           0x00000003  // select D3DRS_TEXTUREFACTOR (read only)
#define D3DTA_SPECULAR          0x00000004  // select specular color (read only)
#define D3DTA_TEMP              0x00000005  // select temporary register color (read/write)
#define D3DTA_COMPLEMENT        0x00000010  // take 1.0 - x (read modifier)
#define D3DTA_ALPHAREPLICATE    0x00000020  // replicate alpha to color components (read modifier)

typedef enum _D3DTEXTURESTAGESTATETYPE {
	D3DTSS_COLOROP = 1, /* D3DTEXTUREOP - per-stage blending controls for color channels */
	D3DTSS_COLORARG1 = 2, /* D3DTA_* (texture arg) */
	D3DTSS_COLORARG2 = 3, /* D3DTA_* (texture arg) */
	D3DTSS_ALPHAOP = 4, /* D3DTEXTUREOP - per-stage blending controls for alpha channel */
	D3DTSS_ALPHAARG1 = 5, /* D3DTA_* (texture arg) */
	D3DTSS_ALPHAARG2 = 6, /* D3DTA_* (texture arg) */
	D3DTSS_BUMPENVMAT00 = 7, /* float (bump mapping matrix) */
	D3DTSS_BUMPENVMAT01 = 8, /* float (bump mapping matrix) */
	D3DTSS_BUMPENVMAT10 = 9, /* float (bump mapping matrix) */
	D3DTSS_BUMPENVMAT11 = 10, /* float (bump mapping matrix) */
	D3DTSS_TEXCOORDINDEX = 11, /* identifies which set of texture coordinates index this texture */
	D3DTSS_BUMPENVLSCALE = 22, /* float scale for bump map luminance */
	D3DTSS_BUMPENVLOFFSET = 23, /* float offset for bump map luminance */
	D3DTSS_TEXTURETRANSFORMFLAGS = 24, /* D3DTEXTURETRANSFORMFLAGS controls texture transform */
	D3DTSS_COLORARG0 = 26, /* D3DTA_* third arg for triadic ops */
	D3DTSS_ALPHAARG0 = 27, /* D3DTA_* third arg for triadic ops */
	D3DTSS_RESULTARG = 28, /* D3DTA_* arg for result (CURRENT or TEMP) */
	D3DTSS_CONSTANT = 32, /* Per-stage constant D3DTA_CONSTANT */

	D3DTSS_FORCE_DWORD = 0x7fffffff, /* force 32-bit size enum */
} D3DTEXTURESTAGESTATETYPE;

typedef struct _D3DLOCKED_RECT {
	INT Pitch;
	void* pBits;
} D3DLOCKED_RECT;

typedef struct _D3DVIEWPORT {
	DWORD X;
	DWORD Y;
	DWORD Width;
	DWORD Height;
	float MinZ;
	float MaxZ;
} D3DVIEWPORT;

/// Errors
/// ------
#define FACILITY_D3D 0x88760000
#define D3D_OK 0
#define D3DERR_DRIVERINTERNALERROR	(FACILITY_D3D|2087)
#define D3DERR_DEVICELOST         	(FACILITY_D3D|2152)
#define D3DERR_DEVICENOTRESET     	(FACILITY_D3D|2153)
/// ------

/// IDirect3D
/// ---------
#define d3d_CreateDevice_(x) \
	x(IDirect3D*, that), \
	x(UINT, Adapter), \
	x(D3DDEVTYPE, DeviceType), \
	x(HWND, hFocusWindow), \
	x(DWORD, BehaviourFlags), \
	x(D3DPRESENT_PARAMETERS*, pPresentationParameters), \
	x(void****, ppReturnedDeviceInterface)
MINID3D_TYPEDEF(HRESULT, d3d_CreateDevice)
/// ---------

/// IDirect3DStateBlock9
/// --------------------
#define d3dsb9_Capture_(x) \
	x(IDirect3DStateBlock, pSB)

#define d3dsb9_Apply_(x) \
	x(IDirect3DStateBlock, pSB)
/// --------------------

/// IDirect3DSurface
/// ----------------
#define d3ds_GetDesc_(x) \
	x(IDirect3DSurface*, that), \
	x(D3DSURFACE_DESC*, pDesc)

MINID3D_VTABLE_FUNC_DEF(HRESULT, d3ds_GetDesc, 8, 12)
/// ----------------

/// IDirect3DDevice
/// ---------------
#define d3dd8_GetBackBuffer_(x) \
	x(IDirect3DDevice*, that), \
	x(UINT, BackBuffer), \
	x(D3DBACKBUFFER_TYPE, Type), \
	x(IDirect3DSurface**, ppBackBuffer)

#define d3dd9_GetBackBuffer_(x) \
	x(IDirect3DDevice*, that), \
	x(UINT, iSwapChain), \
	x(UINT, BackBuffer), \
	x(D3DBACKBUFFER_TYPE, Type), \
	x(IDirect3DSurface**, ppBackBuffer)

#define d3dd8_CreateTexture_(x) \
	x(IDirect3DDevice*, that), \
	x(UINT, Width), \
	x(UINT, Height), \
	x(UINT, Levels), \
	x(DWORD, Usage), \
	x(D3DFORMAT, Format), \
	x(D3DPOOL, Pool), \
	x(IDirect3DTexture**, ppTexture)

#define d3dd9_CreateTexture_(x) \
	d3dd8_CreateTexture_(x), \
	x(HANDLE, *pSharedHandle)

#define d3dd_EndScene_(x) \
	x(IDirect3DDevice*, that)

#define d3dd_SetViewport_(x) \
	x(IDirect3DDevice*, that), \
	x(const D3DVIEWPORT*, pViewport)

#define d3dd_SetRenderState_(x) \
	x(IDirect3DDevice*, that), \
	x(D3DRENDERSTATETYPE, State), \
	x(DWORD, Value)

#define d3dd8_ApplyStateBlock_(x) \
	x(IDirect3DDevice*, that), \
	x(IDirect3DStateBlock, pSB)
	// (Yes, no pointer here, due to Direct3D 8 compatibility)

#define d3dd8_CaptureStateBlock_(x) \
	x(IDirect3DDevice*, that), \
	x(IDirect3DStateBlock, pSB)
	// (Yes, no pointer here, due to Direct3D 8 compatibility)

#define d3dd8_DeleteStateBlock_(x) \
	x(IDirect3DDevice*, that), \
	x(IDirect3DStateBlock, pSB)
	// (Yes, no pointer here, due to Direct3D 8 compatibility)

#define d3dd_CreateStateBlock_(x) \
	x(IDirect3DDevice*, that), \
	x(D3DSTATEBLOCKTYPE, Type), \
	x(IDirect3DStateBlock*, pSB)
	// (Yes, no double pointer here, due to Direct3D 8 compatibility)

#define d3dd_SetTexture_(x) \
	x(IDirect3DDevice*, that), \
	x(DWORD, Stage), \
	x(IDirect3DTexture*, pTexture)

#define d3dd_SetTextureStageState_(x)\
	x(IDirect3DDevice*, that), \
	x(DWORD, Stage), \
	x(D3DTEXTURESTAGESTATETYPE, Type), \
	x(DWORD, Value)

#define d3dd_DrawPrimitiveUP_(x) \
	x(IDirect3DDevice*, that), \
	x(D3DPRIMITIVETYPE, PrimitiveType), \
	x(UINT, PrimitiveCount), \
	x(const void *, pVertexStreamZeroData), \
	x(UINT, VertexStreamZeroStride)

#define d3dd_SetFVF_(x) \
	x(IDirect3DDevice*, that), \
	x(DWORD, FVF)

__inline HRESULT d3dd_GetBackBuffer(d3d_version_t ver, d3dd8_GetBackBuffer_(FULLDEC))
{
	typedef HRESULT __stdcall ft8(d3dd8_GetBackBuffer_(FULLDEC));
	typedef HRESULT __stdcall ft9(d3dd9_GetBackBuffer_(FULLDEC));
	FARPROC *vt = *(FARPROC**)that;
	return
		(ver == D3D8) ? ((ft8*)vt[16])(d3dd8_GetBackBuffer_(VARNAMES)) :
		(ver == D3D9) ? ((ft9*)vt[18])(that, 0, BackBuffer, Type, ppBackBuffer) :
		((ft8*)NULL)(d3dd8_GetBackBuffer_(VARNAMES));
}

__inline HRESULT d3dd_CreateTexture(d3d_version_t ver, d3dd8_CreateTexture_(FULLDEC))
{
	typedef HRESULT __stdcall ft8(d3dd8_CreateTexture_(FULLDEC));
	typedef HRESULT __stdcall ft9(d3dd9_CreateTexture_(FULLDEC));
	FARPROC *vt = *(FARPROC**)that;
	return
		(ver == D3D8) ? ((ft8*)vt[20])(d3dd8_CreateTexture_(VARNAMES)) :
		(ver == D3D9) ? ((ft9*)vt[23])(d3dd8_CreateTexture_(VARNAMES), NULL) :
		((ft8*)NULL)(d3dd8_CreateTexture_(VARNAMES));
}

MINID3D_VTABLE_FUNC_DEF(HRESULT, d3dd_EndScene, 35, 42)
MINID3D_VTABLE_FUNC_DEF(HRESULT, d3dd_SetViewport, 40, 47)

__inline HRESULT d3dd_ApplyStateBlock(d3d_version_t ver, d3dd8_ApplyStateBlock_(FULLDEC))
{
	typedef HRESULT __stdcall ft8(d3dd8_ApplyStateBlock_(FULLDEC));
	typedef HRESULT __stdcall ft9(d3dsb9_Apply_(FULLDEC));
	return
		(ver == D3D8) ? (*(ft8***)that)[54](d3dd8_ApplyStateBlock_(VARNAMES)) :
		(ver == D3D9) ? (*(ft9***) pSB)[ 5](d3dsb9_Apply_(VARNAMES)) :
		((FARPROC)NULL)();
}

__inline HRESULT d3dd_CaptureStateBlock(d3d_version_t ver, d3dd8_CaptureStateBlock_(FULLDEC))
{
	typedef HRESULT __stdcall ft8(d3dd8_CaptureStateBlock_(FULLDEC));
	typedef HRESULT __stdcall ft9(d3dsb9_Capture_(FULLDEC));
	return
		(ver == D3D8) ? (*(ft8***)that)[55](d3dd8_CaptureStateBlock_(VARNAMES)) :
		(ver == D3D9) ? (*(ft9***)pSB)[4](d3dsb9_Capture_(VARNAMES)) :
		((FARPROC)NULL)();
}

__inline HRESULT d3dd_DeleteStateBlock(d3d_version_t ver, d3dd8_DeleteStateBlock_(FULLDEC))
{
	typedef HRESULT __stdcall ft8(d3dd8_DeleteStateBlock_(FULLDEC));
	return
		(ver == D3D8) ? (*(ft8***)that)[56](d3dd8_DeleteStateBlock_(VARNAMES)) :
		(ver == D3D9) ? ((IUnknown*)pSB)->Release() :
		((FARPROC)NULL)();
}

MINID3D_VTABLE_FUNC_DEF(HRESULT, d3dd_SetRenderState, 50, 57)
MINID3D_VTABLE_FUNC_DEF(HRESULT, d3dd_CreateStateBlock, 57, 59)
MINID3D_VTABLE_FUNC_DEF(HRESULT, d3dd_SetTexture, 61, 65)
MINID3D_VTABLE_FUNC_DEF(HRESULT, d3dd_SetTextureStageState, 63, 67)
MINID3D_VTABLE_FUNC_DEF(HRESULT, d3dd_DrawPrimitiveUP, 72, 83)
// Mapped to SetVertexShader() in Direct3D 8
MINID3D_VTABLE_FUNC_DEF(HRESULT, d3dd_SetFVF, 76, 89)
/// ---------------

/// IDirect3DTexture
/// ----------------
#define d3dtex_GetLevelDesc_(x) \
	x(IDirect3DTexture*, that), \
	x(UINT, Level), \
	x(D3DSURFACE_DESC*, pDesc)

#define d3dtex_LockRect_(x) \
	x(IDirect3DTexture*, that), \
	x(UINT, Level), \
	x(D3DLOCKED_RECT*, pLockedRect), \
	x(const RECT*, pRect), \
	x(DWORD, Flags)

#define d3dtex_UnlockRect_(x) \
	x(IDirect3DTexture*, that), \
	x(UINT, Level)

MINID3D_VTABLE_FUNC_DEF(HRESULT, d3dtex_GetLevelDesc, 14, 17);
MINID3D_VTABLE_FUNC_DEF(HRESULT, d3dtex_LockRect, 16, 19);
MINID3D_VTABLE_FUNC_DEF(HRESULT, d3dtex_UnlockRect, 17, 20);
/// ----------------

/// D3DX
/// ----
typedef uint32_t D3DXIMAGE_FILEFORMAT;

typedef struct _D3DXIMAGE_INFO {
	uint32_t Width;
	uint32_t Height;
	uint32_t Depth;
	uint32_t MipLevels;
	D3DFORMAT Format;
	D3DRESOURCETYPE ResourceType;
	D3DXIMAGE_FILEFORMAT ImageFileFormat;
} D3DXIMAGE_INFO;
/// ----

/// Detouring helpers
/// -----------------
// Using the same API as vtable_detour(), these can be used to directly detour
// IDirect3DDevice functions without duplicating the boilerplate of detouring
// Direct3DCreate() and IDirect3D::CreateDevice(). Since the contents of [det]
// need to be cached until the corresponding CreateDevice() function runs, its
// lifetime doesn't matter.
extern "C" THCRAP_API void d3d8_device_detour(vtable_detour_t *det, size_t det_count);
extern "C" THCRAP_API void d3d9_device_detour(vtable_detour_t *det, size_t det_count);
/// -----------------
