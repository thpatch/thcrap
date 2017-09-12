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
#include "pl.h"
#include <zlib.h>
#include <string>
#include <vector>
#include <list>

static int inflate_bytes(BYTE* file_in, size_t size_in, BYTE* file_out, size_t size_out)
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

static int deflate_bytes(BYTE* file_in, size_t size_in, BYTE* file_out, size_t* size_out)
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

void patch_line(BYTE *&in, BYTE *&out, DWORD nb_col, json_t *patch_row)
{
	// Read input
	std::vector<std::string> line;
	for (DWORD col = 0; col < nb_col; col++) {
		DWORD field_size = *(DWORD*)in;
		in += sizeof(DWORD);
		line.push_back(std::string((const char*)in, field_size));
		in += field_size;
	}

	// Patch as data/win/message/*.csv
	json_t *patch_lines = json_object_get(patch_row, "lines");
	if (patch_lines && line.size() <= 13 && line[4].empty() == false) {
		std::list<TasofroPl::ALine*> texts;
		// We want to overwrite all the balloons with the user-provided ones,
		// so we only need to put the 1st one, we can ignore the others.
		TasofroPl::Text *text = new TasofroPl::Text(std::vector<std::string>({
			line[4],
			line[3]
		}), "", TasofroPl::Text::WIN);
		texts.push_back(text);
		text->patch(texts, texts.begin(), "", patch_lines);

		size_t i = 0;
		for (TasofroPl::ALine* it : texts) {
			if (i == 3) {
				log_print("TFCS: warning: trying to put more than 3 balloons in a win line.\n");
				break;
			}
			line[1 + 4 * i + 3] = it->get(0);
			line[1 + 4 * i + 2] = it->get(1);
			i++;
		}
	}

	// Patch each column independently
	json_t *patch_col;
	for (DWORD col = 0; col < nb_col; col++) {
		patch_col = json_object_numkey_get(patch_row, col);
		if (patch_col && json_is_string(patch_col)) {
			line[col] = json_string_value(patch_col);
		}
	}

	// Write output
	for (const std::string& col : line) {
		DWORD field_size = col.length();
		*(DWORD*)out = field_size;
		out += sizeof(DWORD);
		memcpy(out, col.c_str(), field_size);
		out += field_size;
	}
}

void skip_line(BYTE *&in, BYTE *&out, DWORD nb_col)
{
	for (DWORD col = 0; col < nb_col; col++) {
		DWORD field_size = *(DWORD*)in; in += sizeof(DWORD);
		*(DWORD*)out = field_size;      out += sizeof(DWORD);

		memcpy(out, in, field_size);
		in  += field_size;
		out += field_size;
	}
}

int patch_tfcs(void *file_inout, size_t size_out, size_t size_in, json_t *patch)
{
	tfcs_header_t *header;

	// Read TFCS header
	header = (tfcs_header_t*)file_inout;
	if (size_in < sizeof(header) || memcmp(header->magic, "TFCS\0", 5) != 0) {
		// Invalid TFCS file (probably a regular CSV file)
		return patch_csv((char*)file_inout, size_out, size_in, patch);
	}

	BYTE *file_in_uncomp = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, header->uncomp_size);
	inflate_bytes(header->data, header->comp_size, file_in_uncomp, header->uncomp_size);

	// We don't have patch_size here, but it is equal to size_out - size_in.
	DWORD file_out_uncomp_size = header->uncomp_size + (size_out - size_in);
	BYTE *file_out_uncomp = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, file_out_uncomp_size);

	BYTE *ptr_in  = file_in_uncomp;
	BYTE *ptr_out = file_out_uncomp;

	DWORD nb_row;
	DWORD nb_col;
	DWORD row;

	nb_row = *(DWORD*)ptr_in;  ptr_in  += sizeof(DWORD);
	*(DWORD*)ptr_out = nb_row; ptr_out += sizeof(DWORD);

	for (row = 0; row < nb_row; row++) {
		nb_col = *(DWORD*)ptr_in;  ptr_in  += sizeof(DWORD);
		*(DWORD*)ptr_out = nb_col; ptr_out += sizeof(DWORD);
		json_t *patch_row = json_object_numkey_get(patch, row);

		if (patch_row) {
			patch_line(ptr_in, ptr_out, nb_col, patch_row);
		}
		else {
			skip_line(ptr_in, ptr_out, nb_col);
		}
	}

	// Write result
	file_out_uncomp_size = ptr_out - file_out_uncomp;
	header->uncomp_size = file_out_uncomp_size;
	size_out -= sizeof(header) - 1;
	int ret = deflate_bytes(file_out_uncomp, file_out_uncomp_size, header->data, &size_out);
	header->comp_size = size_out;
	if (ret != Z_OK) {
		log_printf("WARNING: tasofro CSV patching: compression failed with zlib error %d\n", ret);
	}

	HeapFree(GetProcessHeap(), 0, file_in_uncomp);
	HeapFree(GetProcessHeap(), 0, file_out_uncomp);

	return 0;
}
