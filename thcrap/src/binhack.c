/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Binary hack handling.
  */

#include "thcrap.h"

int is_valid_hex(char c)
{
	return
		('0' <= c && c <= '9') ||
		('A' <= c && c <= 'F') ||
		('a' <= c && c <= 'f');
}

size_t binhack_calc_size(const char *binhack_str)
{
	size_t size = 0;
	const char *c = binhack_str;
	const char *fs = NULL; // function start
	if(!binhack_str) {
		return 0;
	}
	while(*c) {
		if(!fs && is_valid_hex(*c) && is_valid_hex(*(c+1)) ) {
			size++;
			c++;
		}
		else if(*c == '[' || *c == '<') {
			if(fs) {
				log_printf("ERROR: Nested function pointers near %s!\n", c);
				return 0;
			}
			fs = c + 1;
		}
		else if(fs && (*c == ']' || *c == '>')) {
			size += 4;
			fs = NULL;
		}
		c++;
	}
	if(fs) {
		log_printf("ERROR: Function name '%s' not terminated...\n", fs);
		size = 0;
	}
	return size;
}

int binhack_render(BYTE *binhack_buf, size_t target_addr, const char *binhack_str, json_t *inj_funcs)
{
	const char *c = binhack_str;
	const char *fs = NULL; // function start
	size_t written = 0;
	int func_rel; // Relative function pointer flag
	char conv[3];
	int ret = 0;

	// We don't check [inj_funcs] here, we want to give the precise error later
	if(!binhack_buf || !binhack_str) {
		return 1;
	}

	conv[2] = 0;
	while(*c)
	{
		if(!fs && is_valid_hex(*c) && is_valid_hex(*(c+1)) ) {
			memcpy(conv, c, 2);
			*binhack_buf = (char)strtol(conv, NULL, 16);

			binhack_buf++;
			c++;
			written++;
		}
		else if(*c == '[' || *c == '<') {
			if(fs) {
				log_printf("ERROR: Nested function pointers near %s!\n", c);
				return 0;
			}
			func_rel = (*c == '[');
			fs = c + 1;
		}
		else if(fs && (*c == ']' || *c == '>')) {
			VLA(char, function, (c - fs) + 1);
			json_t *json_fp;
			unsigned long fp = 0;

			strncpy(function, fs, c - fs);
			function[c - fs] = 0;

			json_fp = json_object_get(inj_funcs, function);
			if(json_fp) {
				fp = (unsigned long)json_integer_value(json_fp);
				if(func_rel) {
					fp -= target_addr + written + 4;
				}
			}
			if(fp) {
				memcpy(binhack_buf, &fp, 4);
				binhack_buf += 4;
				written += 4;
			}
			else {
				log_printf("ERROR: No pointer for function '%s'...\n", function);
				ret = 2;
			}
			fs = NULL;
			VLA_FREE(function);
			if(ret) {
				break;
			}
		}
		c++;
	}
	if(fs) {
		log_printf("ERROR: Function name '%s' not terminated...\n", fs);
		ret = 1;
	}
	return ret;
}

int GetExportedFunctions(json_t *funcs, HMODULE hDll)
{
	IMAGE_EXPORT_DIRECTORY *ExportDesc;
	DWORD *func_ptrs = NULL;
	DWORD *name_ptrs = NULL;
	WORD *name_indices = NULL;
	DWORD dll_base = (DWORD)hDll; // All this type-casting is annoying
	WORD i, j; // can only ever be 16-bit values

	if(!funcs) {
		return -1;
	}

	ExportDesc = GetDllExportDesc(hDll);

	if(!ExportDesc) {
		return -2;
	}

	func_ptrs = (DWORD*)(ExportDesc->AddressOfFunctions + dll_base);
	name_ptrs = (DWORD*)(ExportDesc->AddressOfNames + dll_base);
	name_indices = (WORD*)(ExportDesc->AddressOfNameOrdinals + dll_base);
	
	for(i = 0; i < ExportDesc->NumberOfFunctions; i++) {
		DWORD name_ptr = 0;
		const char *name;
		char auto_name[16];

		// Look up name
		for(j = 0; (j < ExportDesc->NumberOfNames && !name_ptr); j++) {
			if(name_indices[j] == i) {
				name_ptr = name_ptrs[j];
			}
		}

		if(name_ptr) {
			name = (const char*)(dll_base + name_ptr);
		} else {
			itoa(i + ExportDesc->Base, auto_name, 10);
			name = auto_name;
		}

		log_printf("0x%08x %s\n", dll_base + func_ptrs[i], name);

		json_object_set_new(funcs, name, json_integer(dll_base + func_ptrs[i]));
	}
	return 0;
}

int binhacks_apply(json_t *binhacks, json_t *funcs)
{
	const char *key;
	json_t *hack;
	size_t binhack_count;
	size_t c = 0;	// gets incremented at the beginning of the write loop

	if(!binhacks || !funcs) {
		return -1;
	}

	binhack_count = json_object_size(binhacks);

	if(!binhack_count) {
		log_printf("No binary hacks to apply.\n");
		return 0;
	}

	log_printf("Applying binary hacks...\n");
	log_printf("------------------------\n");

	json_object_foreach(binhacks, key, hack)
	{
		const char *code; // assembly code from JSON
		size_t i;
		json_t *json_addr; // Addresses can be an array, too
		size_t json_addr_count; // Number of addresses from JSON (at least 1)
		DWORD addr;
		size_t asm_size; // calculated byte size of the hack

		code = json_object_get_string(hack, "code");
		json_addr = json_object_get(hack, "addr");
		asm_size = binhack_calc_size(code);

		if(!code || !asm_size || !json_addr) {
			binhack_count--;
			continue;
		}

		// Determine addresses
		json_addr_count = json_array_size(json_addr);

		if(json_addr_count == 0) {
			json_addr_count = 1;
		} else {
			binhack_count += json_addr_count - 1;
		}
		
		for(i = 0; i < json_addr_count; i++) {
			const char *title = json_object_get_string(hack, "title");
			// buffer for the rendered assembly code
			VLA(BYTE, asm_buf, asm_size);

			c++;

			if(json_is_array(json_addr)) {
				addr = json_array_get_hex(json_addr, i);
			} else {
				addr = json_hex_value(json_addr);
			}
			if(!addr) {
				continue;
			}

			log_printf("(%2d/%2d) 0x%08x ", c, binhack_count, addr);
			if(title) {
				log_printf("%s (%s)... ", title, key);
			} else {
				log_printf("%s...", key);
			}

			if(binhack_render(asm_buf, addr, code, funcs)) {
				continue;
			}
			PatchRegionNoCheck((void*)addr, asm_buf, asm_size);
			VLA_FREE(asm_buf);
			log_printf("OK\n");
		}
	}
	log_printf("------------------------\n");
	return 0;
}
