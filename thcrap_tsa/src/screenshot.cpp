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
		png_set_write_fn(png_ptr, handle, [](png_structp png_ptr, png_bytep data, png_size_t size) {
			HANDLE hFile = png_get_io_ptr(png_ptr);
			DWORD byteRet;
			WriteFile(hFile, data, size, &byteRet, NULL);
			}, [](png_structp) {});
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
