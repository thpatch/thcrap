/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * On-the-fly th145 TFCS patcher (*.csv)
  * Most CSV files can also be provided as TFCS files (and are provided by the game as TFCS files).
  * These CSV files aren't supported for now because the ULiL TFCS parser
  * is way more reliable than the ULiL CSV parser for these files.
  * If we add CSV support here, I think we'd better do it in the form of CSV=>TFCS conversion,
  * followed by TFCS patching.
  */

#include <thcrap.h>
#include "thcrap_tasofro.h"
#include "tfcs.h"
#include <zlib.h>

static int inflate_bytes(BYTE* file_in, DWORD size_in, BYTE* file_out, DWORD size_out)
{
	int ret;
	z_stream strm;

	strm.zalloc = NULL;
	strm.zfree = NULL;
	strm.opaque = NULL;
	strm.next_in = file_in;
	strm.avail_in = size_in;
	strm.next_out = file_out;
	strm.avail_out = size_out;
	ret = inflateInit(&strm);
	if (ret != Z_OK) {
		return ret;
	}
	do {
		ret = inflate(&strm, Z_NO_FLUSH);
		if (ret != Z_OK) {
			break;
		}
	} while (ret != Z_STREAM_END);
	inflateEnd(&strm);
	return ret == Z_STREAM_END ? Z_OK : ret;
}

static int deflate_bytes(BYTE* file_in, DWORD size_in, BYTE* file_out, DWORD* size_out)
{
	int ret;
	z_stream strm;

	strm.zalloc = NULL;
	strm.zfree = NULL;
	strm.opaque = NULL;
	strm.next_in = file_in;
	strm.avail_in = size_in;
	strm.next_out = file_out;
	strm.avail_out = *size_out;
	ret = deflateInit(&strm, Z_BEST_COMPRESSION);
	if (ret != Z_OK) {
		return ret;
	}
	do {
		ret = deflate(&strm, Z_FINISH);
		if (ret != Z_OK) {
			break;
		}
	} while (ret != Z_STREAM_END);
	deflateEnd(&strm);
	*size_out -= strm.avail_out;
	return ret == Z_STREAM_END ? Z_OK : ret;
}

int patch_tfcs(BYTE *file_inout, size_t size_out, size_t size_in, json_t *patch)
{
	tfcs_header_t *header;

	// Read TFCS header
	header = (tfcs_header_t*)file_inout;
	if (size_in < sizeof(header) || memcmp(header->magic, "TFCS\0", 5) != 0) {
		// Invalid TFCS file (probably a regular CSV file)
		return patch_csv(file_inout, size_out, size_in, patch);
	}

	BYTE *file_in_uncomp = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, header->uncomp_size);
	inflate_bytes(header->data, header->comp_size, file_in_uncomp, header->uncomp_size);

	// We don't have patch_size here, but it is equal to size_out - size_in.
	DWORD file_out_uncomp_size = header->uncomp_size + (size_out - size_in);
	BYTE *file_out_uncomp = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, file_out_uncomp_size);

	BYTE *ptr_in  = file_in_uncomp;
	BYTE *ptr_out = file_out_uncomp;

	DWORD nb_row;
	DWORD nb_col;
	DWORD row;
	DWORD col;

	nb_row = *(DWORD*)ptr_in;  ptr_in  += sizeof(DWORD);
	*(DWORD*)ptr_out = nb_row; ptr_out += sizeof(DWORD);

	for (row = 0; row < nb_row; row++) {
		nb_col = *(DWORD*)ptr_in;  ptr_in  += sizeof(DWORD);
		*(DWORD*)ptr_out = nb_col; ptr_out += sizeof(DWORD);
		json_t *patch_row = json_object_numkey_get(patch, row);

		for (col = 0; col < nb_col; col++) {
			json_t *patch_col = NULL;
			DWORD field_in_size = *(DWORD*)ptr_in;
			ptr_in += sizeof(DWORD);

			const BYTE *field_out;
			DWORD field_out_size;

			if (!json_is_object(patch_row) || !json_is_string(patch_col = json_object_numkey_get(patch_row, col))) {
				field_out = ptr_in;
				field_out_size = field_in_size;
			}
			else {
				field_out = json_string_value(patch_col);
				field_out_size = strlen(field_out);
			}
			ptr_in += field_in_size;

			*(DWORD*)ptr_out = field_out_size; ptr_out += sizeof(DWORD);
			memcpy(ptr_out, field_out, field_out_size);
			ptr_out += field_out_size;

			json_decref_safe(patch_col);
		}
		json_decref_safe(patch_row);
	}

	// Write result
	header->uncomp_size = ptr_out - file_out_uncomp;
	size_out -= sizeof(header);
	int ret = deflate_bytes(file_out_uncomp, file_out_uncomp_size, header->data, &size_out);
	header->comp_size = size_out;

	HeapFree(GetProcessHeap(), 0, file_in_uncomp);
	HeapFree(GetProcessHeap(), 0, file_out_uncomp);

	return 0;
}
