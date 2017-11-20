/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Zipfile handling.
  */

#include "thcrap.h"
#include <zlib.h>

/**
  * Zip is a bad format. It may have had its benefits in the age of MS-DOS and
  * floppy disks, but nowadays, it only causes confusion.
  *
  * First, the "end of central directory" structure, pointing to the *actual*
  * central directory, is stored BEFORE A VARIABLE-SIZED COMMENT AT THE END OF
  * THE FILE. But wait, there's a much saner way to access the files: simply
  * iterating forward over the "local file header" structures! However, this
  * is not guaranteed to work and in no way mentioned in the specification.
  * You *are* supposed to look at the central directory... which mainly
  * consists of COPIES OF THE LOCAL FILE HEADERS that can nevertheless hold
  * different data and have undefined precedence over each other. Add the
  * Zip64 extensions, the overall lack of a proper chunk structure you would
  * expect from an extensible format, and the high probability of rare but
  * definitely exploitable bugs and vulnerabilities in all of this, and you
  * have a format you should better not continue to support.
  *
  * The implementation ("interpretation" is probably a better word) parses
  * both the central directory structure as well as the file header,
  * prioritizing the latter.
  *
  * That said, is there any alternative, widespread container format out there
  * that preferably uses Deflate compression? .tar.gz is solid, so that's out
  * of the question. .7z would come to mind, but it mainly uses LZMA; in the
  * latest 7-Zip version, I couldn't even select Deflate for a 7z archive in
  * the compression dialog, only through the command-line version. Microsoft's
  * own .cab format seems like the only remaining option, and even that is not
  * exactly common either.
  */

#define DEFLATE_CHUNK 16384

/// Zip structures
/// --------------
#define ZIP_MAGIC_FILE 0x04034b50
#define ZIP_MAGIC_DIR 0x02014b50
#define ZIP_MAGIC_DIR_END 0x06054b50

#define ZIP_EXTRA_NTFS 0x000a
#define ZIP_EXTRA_NTFS_TAG 0x0001

#pragma pack(push, 1)
typedef struct {
	uint16_t version;
	uint16_t flag;
	uint16_t compression;
	uint16_t mtime;
	uint16_t mdate;
	uint32_t crc32;
	uint32_t size_compressed;
	uint32_t size_uncompressed;
	uint16_t fn_len;
	uint16_t extra_len;
} zip_file_shared_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
	uint32_t sig; // = ZIP_MAGIC_FILE
	zip_file_shared_t s;
} zip_file_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
	uint16_t id;
	uint16_t len;
} zip_extra_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
	uint32_t reserved;
	uint16_t tag; // == ZIP_EXTRA_NTFS_TAG
	uint16_t tag_size; // = 24
	FILETIME mtime;
	FILETIME atime;
	FILETIME ctime;
} zip_extra_ntfs1_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
	uint32_t sig; // = ZIP_MAGIC_DIR
	uint16_t made_by;
	zip_file_shared_t s;
	uint16_t cmt_len;
	uint16_t disk_start;
	uint16_t attrib_intern;
	uint32_t attrib_extern;
	uint32_t offset_header;
} zip_dir_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
	uint32_t sig; // = ZIP_MAGIC_DIR_END
	uint16_t disk_num;
	uint16_t dir_start_disk;
	uint16_t dir_num_on_disk;
	uint16_t dir_num_total;
	uint32_t dir_len;
	uint32_t dir_start_offset;
	uint16_t cmt_len;
} zip_dir_end_t;
#pragma pack(pop)
/// --------------

// All the file information *we* care about
// ----
typedef struct {
	uint32_t offset;
	uint16_t compression;
	FILETIME ctime;
	FILETIME mtime;
	FILETIME atime;
	uint32_t size_compressed;
	uint32_t size_uncompressed;
} zip_file_info_t;
// ----

// Readability.
DWORD TellFilePointer(HANDLE hFile)
{
	return SetFilePointer(hFile, 0, NULL, FILE_CURRENT);
}

/// Compression methods
/// -------------------
typedef int (*zip_comp_func_t)(void *buf, HANDLE hArc, zip_file_info_t *file);

static int zip_file_copy(void *buf, HANDLE hArc, zip_file_info_t *file)
{
	DWORD byte_ret;
	if(!file || !buf || file->size_compressed != file->size_uncompressed) {
		return -1;
	}
	return ReadFile(hArc, buf, file->size_uncompressed, &byte_ret, NULL) == 0;
}

static int zip_file_inflate(void *buf, HANDLE hArc, zip_file_info_t *file)
{
	int ret;
	z_stream strm;
	BYTE in[DEFLATE_CHUNK];
	if(!file || !buf) {
		return -1;
	}

	strm.zalloc = NULL;
	strm.zfree = NULL;
	strm.opaque = NULL;
	strm.avail_in = 0;
	strm.next_out = (BYTE*)buf;
	strm.avail_out = file->size_uncompressed;
	// Use inflateInit2 with negative window bits to indicate raw data
	// (http://codeandlife.com/2014/01/01/unzip-library-for-c/)
	ret = inflateInit2(&strm, -MAX_WBITS);
	if(ret != Z_OK) {
		return ret;
	}
	do {
		DWORD byte_ret;
		ReadFile(hArc, in, DEFLATE_CHUNK, &byte_ret, NULL);
		if(!byte_ret) {
			ret = Z_ERRNO;
			break;
		}
		strm.avail_in = byte_ret;
		strm.next_in = in;
		ret = inflate(&strm, Z_NO_FLUSH);
		if(ret != Z_OK) {
			break;
		}
	} while(ret != Z_STREAM_END);
	inflateEnd(&strm);
	return ret == Z_STREAM_END ? Z_OK : ret;
}
/// -------------------

/// File data
/// ---------
// Updates [file] with the data from [s].
static int zip_file_info_set(zip_file_info_t *file, zip_file_shared_t *s)
{
	if(!file || !s) {
		return -1;
	}
	file->compression = s->compression;
	file->size_compressed = s->size_compressed;
	file->size_uncompressed = s->size_uncompressed;
	if(file->mtime.dwLowDateTime == 0) {
		DosDateTimeToFileTime(s->mdate, s->mtime, &file->mtime);
		file->ctime = file->mtime;
		file->atime = file->mtime;
	}
	return 0;
}

static void zip_file_extra_parse(zip_file_info_t *file, const char *buf, size_t len)
{
	assert(file);
	assert(buf);

	auto p = buf;
	while(p < buf + len) {
		const auto extra = (zip_extra_t *)p;
		p += sizeof(zip_extra_t);

		if(extra->id == ZIP_EXTRA_NTFS && extra->len >= sizeof(zip_extra_ntfs1_t)) {
			const auto ntfs = (zip_extra_ntfs1_t *)p;
			if(ntfs->tag == ZIP_EXTRA_NTFS_TAG && ntfs->tag_size == 24) {
				file->ctime = ntfs->ctime;
				file->mtime = ntfs->mtime;
				file->atime = ntfs->atime;
			}
		}
		p += extra->len;
	}
}

static int zip_file_extra_read(zip_file_info_t *file, zip_t *zip, int extra_len)
{
	assert(file);
	assert(zip);

	VLA(char, extra_buf, extra_len);
	DWORD byte_ret;
	int ret = W32_ERR_WRAP(ReadFile(
		zip->hArc, extra_buf, extra_len, &byte_ret, NULL
	));
	if(!ret) {
		zip_file_extra_parse(file, extra_buf, byte_ret);
	}
	VLA_FREE(extra_buf);
	return 0;
}

// Seeks [zip->hArc] to the central directory record of [fn] in [zip].
static int zip_file_seek(zip_t *zip, const char *fn)
{
	int ret = -1;
	if(zip && fn) {
		json_t *offset_json = json_object_get(zip->files, fn);
		ret = 1;
		if(json_is_integer(offset_json)) {
			json_int_t pos = json_integer_value(offset_json);
			ret = SetFilePointer(
				zip->hArc, (LONG)pos, NULL, FILE_BEGIN
			) == INVALID_SET_FILE_POINTER ? GetLastError() : 0;
		}
	}
	return ret;
}

// Fills [file] with all the necessary information to unzip [fn] from [zip].
// Seeks the file to the beginning of the zipped data.
static int zip_file_prepare(zip_file_info_t *file, zip_t *zip, const char *fn)
{
	int ret = -1;
	if(file && !zip_file_seek(zip, fn)) {
		DWORD byte_ret;
		zip_dir_t zd;
		zip_file_t zf;
		ZeroMemory(file, sizeof(zip_file_info_t));
		if(
			ReadFile(zip->hArc, &zd, sizeof(zd), &byte_ret, NULL)
			&& zd.sig == ZIP_MAGIC_DIR
			&& zd.offset_header != 0xffffffff
		) {
			zip_file_info_set(file, &zd.s);
			SetFilePointer(zip->hArc, zd.s.fn_len, NULL, FILE_CURRENT);
			zip_file_extra_read(file, zip, zd.s.extra_len);
			SetFilePointer(zip->hArc, zd.offset_header, NULL, FILE_BEGIN);
		} else {
			return 1;
		}
		ret = !(
			ReadFile(zip->hArc, &zf, sizeof(zf), &byte_ret, NULL)
			&& zf.sig == ZIP_MAGIC_FILE
		);
		if(!ret) {
			zip_file_info_set(file, &zf.s);
			SetFilePointer(zip->hArc, zf.s.fn_len, NULL, FILE_CURRENT);
			zip_file_extra_read(file, zip, zf.s.extra_len);
		}
	}
	return ret;
}
/// ---------

/// Decompression helpers
/// ---------------------
static void* zip_file_decompress(zip_file_info_t *file, zip_t *zip, const char *fn)
{
	void *file_buffer = NULL;
	if(!zip_file_prepare(file, zip, fn) && file->size_uncompressed) {
		int ret = 0;
		zip_comp_func_t comp_func = NULL;
		file_buffer = malloc(file->size_uncompressed);
		switch(file->compression) {
			case 0:
				comp_func = zip_file_copy;
				break;
			case Z_DEFLATED:
				comp_func = zip_file_inflate;
				break;
		}
		if(comp_func) {
			ret = comp_func(file_buffer, zip->hArc, file);
		} else {
			log_func_printf("Unsupported compression method (%d)\n", file->compression);
			ret = 1;
		}
		if(ret) {
			SAFE_FREE(file_buffer);
		}
	}
	return file_buffer;
}
/// ---------------------

/// Indexing helpers
/// ----------------
// Adds an individual zipped file to the index.
// Expects the file name at the current file pointer of [zip->hArc].
static int zip_file_add(zip_t *zip, zip_file_shared_t *zf, json_int_t offset)
{
	int ret = -1;
	if(zip && zf) {
		DWORD byte_ret;
		VLA(char, fn, zf->fn_len + 1);
		ReadFile(zip->hArc, fn, zf->fn_len, &byte_ret, NULL);
		fn[zf->fn_len] = 0;
		SetFilePointer(zip->hArc, zf->extra_len, NULL, FILE_CURRENT);
		if(zf->fn_len) {
			if(zf->size_compressed != 0) {
				json_object_set_new(zip->files, fn, json_integer(offset));
			} else {
				char last = fn[zf->fn_len - 1];
				if(last != '/' && last != '\\') {
					json_array_append_new(zip->files_empty, json_string(fn));
				}
			}
		}
		VLA_FREE(fn);
		ret = 0;
	}
	return ret;
}

// Adds an individual zipped file from the central directory at [zip->hArc]'s
// file pointer to the index.
int zip_file_add_from_dir(zip_t *zip)
{
	int ret = -1;
	if(zip) {
		zip_dir_t dir = {0};
		DWORD byte_ret = 0;
		json_int_t offset = TellFilePointer(zip->hArc);
		ret = 1;
		if(
			ReadFile(zip->hArc, &dir, sizeof(dir), &byte_ret, NULL)
			&& dir.sig == ZIP_MAGIC_DIR
		) {
			ret = zip_file_add(zip, &dir.s, offset);
			SetFilePointer(zip->hArc, dir.cmt_len, NULL, FILE_CURRENT);
		}
	}
	return ret;
}

// Locates the end of central directory record in the ZIP file and optionally
// copies it to [end]. Also stores the archive comment in [zip].
static size_t zip_dir_end_prepare(zip_dir_end_t *dir_end, zip_t *zip)
{
	size_t read_back = 0;
	const int ZDE_SIZE = sizeof(zip_dir_end_t);
	DWORD zip_size;
	size_t CMT_MAX;
	if(!zip) {
		return -1;
	}
	zip_size = SetFilePointer(zip->hArc, 0, NULL, FILE_END);
	// ZIP comments have a maximum size of 64K
	CMT_MAX = min(zip_size, 0xffff);
	do {
		DWORD byte_ret;
		BYTE test[sizeof(zip_dir_end_t) * 2];
		int step = sizeof(test);
		int i = 0;

		SetFilePointer(zip->hArc, -step, NULL, FILE_CURRENT);
		ReadFile(zip->hArc, test, step, &byte_ret, NULL);
		read_back += byte_ret;
		for(i = 0; i <= step - ZDE_SIZE; i++) {
			// Verification.
			// Note that we check the offset and its length separately,
			// because the addition might overflow.
			zip_dir_end_t *end = (zip_dir_end_t*)(test + i);
			if(
				(end->sig == ZIP_MAGIC_DIR_END)
				&& (end->cmt_len <= read_back - i - ZDE_SIZE)
				&& (end->dir_start_offset < zip_size)
				&& (end->dir_start_offset + end->dir_len < zip_size)
			) {
				size_t end_pos = zip_size - read_back + i;
				if(end->disk_num != 0 || end->dir_start_disk != 0) {
					log_func_printf("Multi-part archives are unsupported\n");
					return -1;
				}
				if(dir_end) {
					memcpy(dir_end, end, ZDE_SIZE);
				}
				// Load the archive comment.
				// Yeah, it's stylistically questionable to shove in this functionality
				// here, but I'm *really* not in the mood for another round of checks...
				SetFilePointer(zip->hArc, end_pos + ZDE_SIZE, NULL, FILE_BEGIN);
				zip->cmt_len = end->cmt_len;
				if(end->cmt_len) {
					zip->cmt = (BYTE *)malloc(end->cmt_len);
					ReadFile(zip->hArc, zip->cmt, end->cmt_len, &byte_ret, NULL);
				}
				return end_pos;
			}
		}
		SetFilePointer(zip->hArc, -step, NULL, FILE_CURRENT);
	} while(read_back < CMT_MAX);
	return -1;
}

// Fills [zip->files] with pointers to all the files in the archive.
static int zip_prepare(zip_t *zip)
{
	zip_dir_end_t dir_end;
	size_t end_pos;
	uint16_t i = 0;
	if(!zip) {
		return -1;
	}
	json_object_clear(zip->files);
	end_pos = zip_dir_end_prepare(&dir_end, zip);
	if(end_pos == -1) {
		log_func_printf(
			"End of central directory record could not be located\n"
		);
		return 1;
	}
	SetFilePointer(zip->hArc, dir_end.dir_start_offset, NULL, FILE_BEGIN);
	for(i = 0; i < dir_end.dir_num_total; i++) {
		if(zip_file_add_from_dir(zip) != 0) {
			log_func_printf(
				"Invalid ZIP directory entry at offset 0x%08x\n",
				TellFilePointer(zip->hArc)
			);
			return 2;
		}
	}
	return 0;
}
/// ----------------

/// Public API
/// ----------
json_t* zip_list(zip_t *zip)
{
	return zip ? zip->files : NULL;
}

json_t* zip_list_empty(zip_t *zip)
{
	return zip ? zip->files_empty : NULL;
}

const BYTE* zip_comment(zip_t *zip, size_t *cmt_len)
{
	BYTE *ret = NULL;
	if(zip && cmt_len) {
		ret = zip->cmt;
		*cmt_len = zip->cmt_len;
	}
	return ret;
}

void* zip_file_load(zip_t *zip, const char *fn, size_t *file_size)
{
	zip_file_info_t file = {0};
	void *ret = NULL;
	if(file_size) {
		ret = zip_file_decompress(&file, zip, fn);
		*file_size = file.size_uncompressed;
	}
	return ret;
}

int zip_file_unzip(zip_t *zip, const char *fn)
{
	int ret = -1;
	zip_file_info_t file = {0};
	void* file_buffer = zip_file_decompress(&file, zip, fn);
	if(file_buffer && !dir_create_for_fn(fn)) {
		DWORD byte_ret;
		HANDLE handle = CreateFile(
			fn, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL
		);
		ret = (handle == INVALID_HANDLE_VALUE);
		if(!ret) {
			SetFileTime(handle, &file.ctime, &file.mtime, &file.atime);
			ret = W32_ERR_WRAP(WriteFile(
				handle, file_buffer, file.size_uncompressed, &byte_ret, NULL
			));
			CloseHandle(handle);
		}
	}
	SAFE_FREE(file_buffer);
	return ret;
}

zip_t* zip_open(const char *fn)
{
	zip_t *ret = NULL;
	HANDLE hArc;
	if(!fn) {
		return NULL;
	}
	// Disabling FILE_SHARE_WRITE until we've figured out ZIP repatching...
	hArc = CreateFile(
		fn, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL
	);
	if(hArc != INVALID_HANDLE_VALUE) {
		ret = (zip_t *)malloc(sizeof(zip_t));
		if(!ret) {
			return NULL;
		}
		ret->hArc = hArc;
		ret->files = json_object();
		ret->files_empty = json_array();
		ret->cmt_len = 0;
		ret->cmt = NULL;
		log_printf("(Zip) Preparing %s...\n", fn);
		if(zip_prepare(ret)) {
			ret = zip_close(ret);
		}
	}
	return ret;
}

zip_t* zip_close(zip_t *zip)
{
	if(zip) {
		SAFE_FREE(zip->cmt);
		json_decref(zip->files_empty);
		json_decref(zip->files);
		CloseHandle(zip->hArc);
		SAFE_FREE(zip);
	}
	return zip;
}
/// ----------
