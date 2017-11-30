/**
  * Touhou Community Reliant Automatic Patcher
  * Tasogare Frontier support plugin
  *
  * ----
  *
  * On-the-fly th145 CSV patcher (*.csv)
  * Some CSV files don't support TFCS and use a completely different parser,
  * so we need to write a CSV patcher for them.
  */

#include <thcrap.h>
#include "thcrap_tasofro.h"
#include "tfcs.h"

static int next_line(char *in, size_t size_in, char *out, int copy)
{
	size_t i = 0;

	while (i < size_in && in[i] && in[i] != ',' && in[i] != '\r' && in[i] != '\n') {
		if (in[i] == '"') {
			if (copy) { out[i] = in[i]; }
			i++;
			while (i < size_in && in[i] && (in[i] != '"' || in[i + 1] == '"')) {
				if (copy) { out[i] = in[i]; }
				i++;
			}
			if (copy) { out[i] = in[i]; }
			i++;
		}
		else {
			if (copy) { out[i] = in[i]; }
			i++;
		}
	}
	return i;
}

int patch_csv(char *file_inout, size_t size_out, size_t size_in, const char*, json_t *patch)
{
	if (!patch) {
		return 0;
	}

	char* file_in = (char*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size_in);
	memcpy(file_in, file_inout, size_in);

	json_t *patch_row = json_object_numkey_get(patch, 0);
	json_t *patch_col = NULL;
	int row = 0;
	int col = 0;
	size_t i = 0;
	size_t j = 0;
	while (i < size_in && file_in[i]) {
		if (!json_is_object(patch_row) || !json_is_string(patch_col = json_object_numkey_get(patch_row, col))) {
			int n = next_line(file_in + i, size_in, file_inout + j, 1);
			i += n;
			j += n;
		}
		else {
			const char* field = json_string_value(patch_col);
			int k;
			file_inout[j] = '"';
			j++;
			for (k = 0; field[k]; j++, k++) {
				if (field[k] == '"') {
					file_inout[j] = '"';
					j++;
				}
				file_inout[j] = field[k];
			}
			file_inout[j] = '"';
			j++;
			i += next_line(file_in + i, size_in, NULL, 0);
		}
		if (file_in[i] == ',') {
			file_inout[j] = file_in[i];
			i++;
			j++;
			col++;
		}
		else if (file_in[i] == '\r' || file_in[i] == '\n') {
			file_inout[j] = file_in[i];
			i++;
			j++;
			if (file_in[i - 1] == '\r' && file_in[i] == '\n') {
				file_inout[j] = file_in[i];
				i++;
				j++;
			}
			row++;
			col = 0;
		}
	}
	if (j > size_out) {
		log_print("WARNING: buffer overflow in tasofro CSV patching (output buffer too small)!\n");
	}

#ifdef _DEBUG
	FILE* fd = fopen("out.csv", "wb");
	if (fd) {
		fwrite(file_inout, j, 1, fd);
		fclose(fd);
	}
#endif

	HeapFree(GetProcessHeap(), 0, file_in);
	return 1;
}

/**
  * Tweaks the th105 csv parser, adding a way to escape quotes.
  *
  * Own JSON parameters
  * -------------------
  * Mandatory:
  *
  *	[character]
  *		Character currently being parsed.
  *		Type: pointer
  *
  *	[special_character]
  *		Copy of character, used to test if it is a special character.
  *		Type: pointer
  *
  *	[string]
  *		Current position of the parser in the CSV file.
  *		Type: pointer
  *
  *	[is_in_quote]
  *		Boolean: true if the parser is in a quote, false otherwise.
  *		Type: immediate
  *
  * Other breakpoints called
  * ------------------------
  *	None
  */
int BP_th105_fix_csv_parser(x86_reg_t *regs, json_t *bp_info)
{
	// Parameters
	// ----------
	size_t *character = json_object_get_pointer(bp_info, regs, "character");
	size_t *special_character = json_object_get_pointer(bp_info, regs, "special_character");
	const char **string = (const char**)json_object_get_pointer(bp_info, regs, "string");
	BYTE is_in_quote = json_object_get_immediate(bp_info, regs, "is_in_quote");
	// ----------

	// We'll keep the th135 parser behavior of escaping quotes only inside quotes, to avoid adding more differences to patch_csv.
	if (is_in_quote == 1 && *character == '"' && (*string)[1] == '"') {
		*special_character = 'a'; // Just a plain, non-special character.
		(*string)++; // Skip one of the 2 quotes
	}
	return 1;
}