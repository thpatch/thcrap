/**
  * Touhou Community Reliant Automatic Patcher
  * Main DLL
  *
  * ----
  *
  * Translation of hardcoded strings.
  */

#include <thcrap.h>

static json_t *stringdefs = NULL;
static json_t *stringlocs = NULL;

const char *strings_lookup(const char *in)
{
#define addr_key_len 2 + 8 + 1

	char addr_key[addr_key_len];
	const char *id_key = NULL;
	const char *ret = in;

	if(!in) {
		return in;
	}

	_snprintf(addr_key, addr_key_len, "0x%x", in);
	id_key = json_object_get_string(stringlocs, addr_key);
	if(id_key) {
		const char *new_str = json_object_get_string(stringdefs, id_key);
		if(new_str && new_str[0]) {
			ret = new_str;
		}
	}
	return ret;
}

void strings_init()
{
	stringdefs = stack_json_resolve("stringdefs.js", NULL);
	stringlocs = stack_game_json_resolve("stringlocs.js", NULL);
}

void strings_exit()
{
	json_decref(stringdefs);
	json_decref(stringlocs);
}
