/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * Useless encryption functions used throughout the games.
  */

#include <thcrap.h>

void simple_xor(
	uint8_t *data, size_t data_len, const uint8_t *params, size_t params_count
)
{
	size_t i;
	if(!params || params_count < 1) {
		return;
	}
	for(i = 0; i < data_len; i++) {
		data[i] ^= params[0];
	}
}

void util_xor(
	uint8_t *data, size_t data_len, const uint8_t *params, size_t params_count
)
{
	size_t i;
	if(!params || params_count < 3) {
		return;
	}
	for(i = 0; i < data_len; i++) {
		const int ip = i - 1;
		data[i] ^= params[0] + i * params[1] + (ip * ip + ip) / 2 * params[2];
	}
}
