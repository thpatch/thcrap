/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * PNG screenshots.
  */

#include <thcrap.h>
#include <png.h>

static auto chain_CreateFileA = CreateFileU;
static auto chain_WriteFile = WriteFile;
static auto chain_CloseHandle = CloseHandle;

static HANDLE hScreenshot = NULL;
static std::vector<uint8_t> bmp;

HANDLE WINAPI screenshot_CreateFileA(
	LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
	DWORD dwFlagsAndAttributes, HANDLE hTemplateFile
)
{
	if (PathMatchSpecU(lpFileName, "*.bmp")) {
		size_t fn_len = strlen(lpFileName);
		char* fn_real = strdup_size(lpFileName, fn_len);
		defer(free(fn_real));
		fn_real[fn_len - 3] = 'p';
		fn_real[fn_len - 2] = 'n';
		fn_real[fn_len - 1] = 'g';
		bmp.clear();
		return hScreenshot = chain_CreateFileA(
			fn_real, dwDesiredAccess, dwShareMode,
			lpSecurityAttributes, dwCreationDisposition,
			dwFlagsAndAttributes, hTemplateFile
		);
	} else {
		return chain_CreateFileA(
			lpFileName, dwDesiredAccess, dwShareMode,
			lpSecurityAttributes, dwCreationDisposition,
			dwFlagsAndAttributes, hTemplateFile
		);
	}
}

BOOL WINAPI screenshot_WriteFile(
	HANDLE hFile, LPCVOID lpBuffer,
	DWORD nNumberOfBytesToWrite,
	LPDWORD lpNumberOfBytesWritten,
	LPOVERLAPPED lpOverlapped
)
{
	if (hFile == hScreenshot) {
		auto buf = (uint8_t*)lpBuffer;
		bmp.insert(bmp.end(), buf, buf + nNumberOfBytesToWrite);
		if (lpNumberOfBytesWritten)
			*lpNumberOfBytesWritten = nNumberOfBytesToWrite;
		return TRUE;
	}
	return chain_WriteFile(hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped);
}

BOOL WINAPI screenshot_CloseHandle(HANDLE handle) {
	if (handle == hScreenshot) {
		hScreenshot = NULL;

		if(!bmp.size())
			return chain_CloseHandle(handle);

		auto bmp_header = (BITMAPFILEHEADER*)bmp.data();
		uint8_t* data = bmp.data() + bmp_header->bfOffBits;

		size_t width = 0;
		size_t height = 0;

		auto bmp_info = (BITMAPINFOHEADER*)(bmp.data() + sizeof(BITMAPFILEHEADER));
		if (bmp_info->biSize == sizeof(BITMAPCOREHEADER)) {
			auto bi_real = (BITMAPCOREHEADER*)&bmp_info;
			width = bi_real->bcWidth;
			height = bi_real->bcHeight;
			//bitdepth = bi_real->bcBitCount;
		} else {
			width = bmp_info->biWidth;
			height = bmp_info->biHeight;
			//bitdepth = bmp_info->biBitCount;
		}
		size_t pitch = 3 * width;

		png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		png_infop info_ptr = png_create_info_struct(png_ptr);
		png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
		png_bytep* row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * height);
		for (size_t i = 0; i < height; i++) {
			uint8_t* row = data + i * pitch;
			for (size_t j = 0; j < width * 3; j += 3) {
				uint8_t px[3];
				px[0] = row[j];
				px[1] = row[j + 1];
				px[2] = row[j + 2];

				row[j]     = px[2];
				row[j + 1] = px[1];
				row[j + 2] = px[0];
			}
			row_pointers[height - i - 1] = data + i * pitch;
		}
		png_set_write_fn(png_ptr, handle,
		[](png_structp png_ptr, png_bytep data, png_size_t size) {
			HANDLE hFile = png_get_io_ptr(png_ptr);
			DWORD byteRet;
			WriteFile(hFile, data, size, &byteRet, NULL);
		},
		[](png_structp png_ptr) {
			HANDLE hFile = png_get_io_ptr(png_ptr);
			FlushFileBuffers(hFile);
		});
		png_set_rows(png_ptr, info_ptr, row_pointers);
		png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
		png_destroy_write_struct(&png_ptr, &info_ptr);
	}
	return chain_CloseHandle(handle);
}

extern "C" TH_EXPORT void screenshot_mod_detour(void) {
	detour_chain("kernel32.dll", 1,
		"CreateFileA", screenshot_CreateFileA, &chain_CreateFileA,
		"WriteFile", screenshot_WriteFile, &chain_WriteFile,
		"CloseHandle", screenshot_CloseHandle, &chain_CloseHandle,
		nullptr
	);
}

#include <minid3d.h>
extern "C" TH_EXPORT int BP_th06_screenshot(x86_reg_t* regs, json_t* bp_info) {
	auto snapshot_open = []() -> HANDLE {
		wchar_t dir[] = L"snapshot/th000.png";
		HANDLE hFile = INVALID_HANDLE_VALUE;
		CreateDirectoryW(L"snapshot", NULL);
		for (int i = 0; i < 1000; i++) {
			dir[13] = i % 10 + 0x30;
			dir[12] = ((i % 100 - i % 10) / 10) + 0x30;
			dir[11] = ((i - i % 100) / 100) + 0x30;
			hFile = CreateFileW(dir, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
			if (hFile != INVALID_HANDLE_VALUE)
				break;
		}
		return hFile;
	};
	if ((GetKeyState('P') & 0x8000)) {
		HANDLE hFile = snapshot_open();
		if (hFile != INVALID_HANDLE_VALUE) {
			auto device = (IDirect3DDevice*)json_object_get_immediate(bp_info, regs, "pD3DDevice");
			IDirect3DSurface* backbuf;
			if (SUCCEEDED(d3dd_GetBackBuffer(D3D8, device, 0, D3DBACKBUFFER_TYPE_MONO, &backbuf))) {
				D3DLOCKED_RECT data;
				if (SUCCEEDED(d3ds_LockRect(D3D8, backbuf, &data, NULL, 0))) {
					constexpr size_t width = 640;
					constexpr size_t height = 480;
					png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
					png_infop info_ptr = png_create_info_struct(png_ptr);
					png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
					png_bytep row_pointers[height];

					for (size_t i = 0; i < height; i++) {
						png_bytep row_orig = (png_bytep)data.pBits + i * data.Pitch;
						png_bytep row = (png_bytep)malloc(width * 3);
						row_pointers[i] = row;
						for (size_t j = 0; j < width; j++) {
							png_bytep px_orig = row_orig + j * 4;
							png_bytep px = row + j * 3;
							px[0] = px_orig[2];
							px[1] = px_orig[1];
							px[2] = px_orig[0];
						}
					}
					png_set_write_fn(png_ptr, hFile,
						[](png_structp png_ptr, png_bytep data, png_size_t size) {
							HANDLE hFile = png_get_io_ptr(png_ptr);
							DWORD byteRet;
							WriteFile(hFile, data, size, &byteRet, NULL);
						},
						[](png_structp png_ptr) {
							HANDLE hFile = png_get_io_ptr(png_ptr);
							FlushFileBuffers(hFile);
						});
					png_set_rows(png_ptr, info_ptr, row_pointers);
					png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
					png_destroy_write_struct(&png_ptr, &info_ptr);
					for (size_t i = 0; i < height; i++) {
						free(row_pointers[i]);
					}
					d3ds_UnlockRect(D3D8, backbuf);
				}
				backbuf->Release();
			}
			CloseHandle(hFile);
		}
	}
	return 1;
}
