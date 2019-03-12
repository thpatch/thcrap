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
typedef IUnknown IDirect3DTexture;

// We currently don't care about any of these, but it's nice to have the
// correct types.
typedef uint32_t D3DDEVTYPE;
typedef uint32_t D3DFORMAT;
typedef uint32_t D3DPOOL;
typedef struct {} D3DPRESENT_PARAMETERS;
typedef uint32_t D3DRESOURCETYPE;

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

/// IDirect3DDevice
/// ---------------
#define d3dd_EndScene_(x) \
	x(IDirect3DDevice*, that)

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

MINID3D_VTABLE_FUNC_DEF(HRESULT, d3dd_EndScene, 35, 42)

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

MINID3D_VTABLE_FUNC_DEF(HRESULT, d3dd_CreateStateBlock, 57, 59)
/// ---------------

/// IDirect3DTexture
/// ----------------
#define d3dtex_GetLevelDesc_(x) \
	x(IDirect3DTexture*, that), \
	x(UINT, Level), \
	x(D3DSURFACE_DESC*, pDesc)

MINID3D_VTABLE_FUNC_DEF(HRESULT, d3dtex_GetLevelDesc, 14, 17);
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
