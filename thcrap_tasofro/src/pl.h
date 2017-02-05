/**
 * Touhou Community Reliant Automatic Patcher
 * Tasogare Frontier support plugin
 *
 * ----
 *
 * Patching of pl files.
 */

#pragma once

#include <thcrap.h>
#include "thcrap_tasofro.h"

typedef struct {
	char name[10];

	int has_pause;
	int is_ending;

	int cur_line;
	int nb_lines;

	char owner[20]; // Name of the character owning the balloon.
} balloon_t;

#ifdef __cplusplus
extern "C" {
#endif

int patch_pl(BYTE *file_inout, size_t size_out, size_t size_in, json_t *patch);

#ifdef __cplusplus
}
#endif
