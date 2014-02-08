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

int binhack_render(BYTE *binhack_buf, size_t target_addr, const char *binhack_str)
{
	const char *c = binhack_str;
	const char *fs = NULL; // function start
	size_t written = 0;
	int func_rel = 0; // Relative function pointer flag
	char conv[3];
	int ret = 0;

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

			fp = (size_t)runconfig_func_get(function);
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

size_t hackpoints_count(json_t *hackpoints)
{
	int ret = 0;
	const char *key;
	json_t *obj;
	json_object_foreach(hackpoints, key, obj) {
		json_t *addr = json_object_get(obj, "addr");
		ret += json_flex_array_size(addr);
	}
	return ret;
}

int binhacks_apply(json_t *binhacks)
{
	const char *key;
	json_t *hack;
	size_t binhack_count = hackpoints_count(binhacks);
	size_t c = 0;	// gets incremented at the beginning of the write loop

	if(!binhack_count) {
		log_printf("No binary hacks to apply.\n");
		return 0;
	}

	log_printf("Applying binary hacks...\n");
	log_printf("------------------------\n");

	json_object_foreach(binhacks, key, hack) {
		const char *title = json_object_get_string(hack, "title");
		const char *code = json_object_get_string(hack, "code");
		// Addresses can be an array, too
		json_t *json_addr = json_object_get(hack, "addr");
		size_t i;
		json_t *addr_val;

		// calculated byte size of the hack
		size_t asm_size = binhack_calc_size(code);

		if(!code || !asm_size || !json_addr) {
			continue;
		}
		json_flex_array_foreach(json_addr, i, addr_val) {
			DWORD addr = json_hex_value(addr_val);
			// buffer for the rendered assembly code
			VLA(BYTE, asm_buf, asm_size);
			if(!addr) {
				continue;
			}

			log_printf("(%2d/%2d) 0x%08x ", ++c, binhack_count, addr);
			if(title) {
				log_printf("%s (%s)... ", title, key);
			} else {
				log_printf("%s...", key);
			}

			if(binhack_render(asm_buf, addr, code)) {
				continue;
			}
			PatchRegion((void*)addr, NULL, asm_buf, asm_size);
			VLA_FREE(asm_buf);
			log_printf("OK\n");
		}
	}
	log_printf("------------------------\n");
	return 0;
}
