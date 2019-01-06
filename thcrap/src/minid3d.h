/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Mini Direct3D 8/9 combination header, including only what we need so far.
  */

#pragma once

// We currently don't care about any of these, but it's nice to have the
// correct types.
typedef IUnknown IDirect3DDevice;
typedef IUnknown IDirect3DTexture;
typedef uint32_t D3DFORMAT;
typedef uint32_t D3DPOOL;
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

/// IDirect3DTexture
/// ----------------
typedef HRESULT __stdcall d3dtex_GetLevelDesc_type(IDirect3DTexture *,
	UINT Level,
	D3DSURFACE_DESC *pDesc
);
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
