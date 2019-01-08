/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Mini Direct3D 8/9 combination header, including only what we need so far.
  */

#pragma once

/// Calling undetoured, identical functions at different vtable offsets
/// -------------------------------------------------------------------
typedef enum {
	D3D8, D3D9
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
typedef IUnknown IDirect3DTexture;

// We currently don't care about any of these, but it's nice to have the
// correct types.
typedef uint32_t D3DDEVTYPE;
typedef uint32_t D3DFORMAT;
typedef uint32_t D3DPOOL;
typedef struct {} D3DPRESENT_PARAMETERS;
typedef uint32_t D3DRESOURCETYPE;
typedef uint32_t D3DMULTISAMPLE_TYPE;

typedef struct _D3DSURFACE_DESC {
	D3DFORMAT Format;
	D3DRESOURCETYPE Type;
	DWORD Usage;
	D3DPOOL Pool;
	UINT Size;
	D3DMULTISAMPLE_TYPE MultiSampleType;
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