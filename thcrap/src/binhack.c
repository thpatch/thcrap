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
			size += sizeof(void*);
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

int binhack_render(BYTE *binhack_buf, size_t target_addr, const char *binhack_str, json_t *funcs)
{
	const char *c = binhack_str;
	const char *fs = NULL; // function start
	size_t written = 0;
	int func_rel = 0; // Relative function pointer flag
	char conv[3];
	int ret = 0;

	// We don't check [funcs] here, we want to give the precise error later
	if(!binhack_buf || !binhack_str) {
		return -1;
	}

	conv[2] = 0;
	while(*c) {
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
			size_t fp = 0;

			strncpy(function, fs, c - fs);
			function[c - fs] = 0;

			fp = json_object_get_hex(funcs, function);
			if(fp) {
				if(func_rel) {
					fp -= target_addr + written + sizeof(void*);
				}
				memcpy(binhack_buf, &fp, sizeof(void*));
				binhack_buf += sizeof(void*);
				written += sizeof(void*);
			} else {
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

int binhacks_apply(json_t *binhacks, json_t *funcs)
{
	const char *key;
	json_t *hack;
	size_t binhack_count = json_object_size(binhacks);
	size_t c = 0;	// gets incremented at the beginning of the write loop

	if(!binhack_count) {
		log_printf("No binary hacks to apply.\n");
		return 0;
	}

	log_printf("Applying binary hacks...\n");
	log_printf("------------------------\n");

	json_object_foreach(binhacks, key, hack)
	{
		const char *title = json_object_get_string(hack, "title");
		const char *code = json_object_get_string(hack, "code");
		// Addresses can be an array, too
		json_t *json_addr = json_object_get(hack, "addr");
		size_t i;

		// Number of addresses from JSON (at least 1)
		size_t json_addr_count = json_array_size(json_addr);

		// calculated byte size of the hack
		size_t asm_size = binhack_calc_size(code);

		if(!code || !asm_size || !json_addr) {
			binhack_count--;
			continue;
		}

		if(json_addr_count == 0) {
			json_addr_count = 1;
		} else {
			binhack_count += json_addr_count - 1;
		}

		for(i = 0; i < json_addr_count; i++) {
			DWORD addr;
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
