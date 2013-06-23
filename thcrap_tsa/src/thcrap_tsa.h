/**
  * Touhou Community Reliant Automatic Patcher
  * Team Shanghai Alice support plugin
  *
  * ----
  *
  * Format patcher declarations.
  */

#pragma once

/**
  * Encryption function type.
  *
  * Parameters
  * ----------
  *	BYTE* data
  *		Buffer to encrypt
  *
  *	size_t data_len
  *		Length of [data]
  *
  *	const BYTE* params
  *		Optional array of parameters required
  *
  *	const BYTE* params_count
  *		Number of parameters in [params].
  *
  * Returns nothing.
  */

typedef void (*EncryptionFunc_t)(
	BYTE *data, size_t data_len, const BYTE *params, size_t params_count
);

int patch_msg(BYTE *msg_out, size_t size_out, size_t size_in, json_t *patch, json_t *format);
