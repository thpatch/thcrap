/**
* Touhou Community Reliant Automatic Patcher
* Team Shanghai Alice support plugin
*
* ----
*
* Png splitter for th06 png files.
*/

#pragma once

void *pngsplit_read(void *buff);
void pngsplit_write(char *buff, void *png);

void *pngsplit_create_png_mask_plt(void *in);
void *pngsplit_create_png_mask(void *in);
void *pngsplit_create_rgb_file_plt(void *in);
void *pngsplit_create_rgb_file(void *in);

void pngsplit_free(void *png);
