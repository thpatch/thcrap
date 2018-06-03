// Copy-pasted from https://github.com/brliron/135tk/tree/master/bmpfont

#include <Windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include "bmpfont_create.h"

#pragma pack(push, 1)
typedef struct
{
  int16_t x;
  int16_t y;
  uint8_t width;
  uint8_t height;
  uint8_t y_offset;
  uint8_t channel;
} CharDetail;
#pragma pack(pop)

typedef struct
{
  unsigned int x;
  unsigned int y;
  int channel;
  unsigned int w;
  unsigned int h;
  unsigned int char_w;
  unsigned int line_h;
} State;

typedef enum
  {
    // Plain RGBA output
    UNPACKED_RGBA,
    // The mode used by the game. Each color channel is used as a grayscale image.
    PACKED_RGBA,
    // Grayscale output
    UNPACKED_GRAYSCALE,
    // Grayscale output, with:
    // (0;0) containing the 1st pixel of the 1st character,
    // (1;0) containing the 1st pixel of the 2st character,
    // (2;0) containing the 1st pixel of the 3st character,
    // (3;0) containing the 1st pixel of the 4st character,
    // (4;0) containing the 2st pixel of the 1st character,
    // etc. It is the format used by thcrap (to make conversion to packed RGBA easier).
    PACKED_GRAYSCALE,

    INVALID_FORMAT
  } OutputType;
static OutputType output_type = INVALID_FORMAT;

// Biggest character used. Use 65535 for all supported characters, 127 for ASCII only.
//#define BIGGEST_CHAR 127
//#define BIGGEST_CHAR 65535

int put_char(void* obj, WCHAR c, BYTE** dest, State* state, CharDetail* charDetail)
{
  int w;
  int h;
  BYTE*  buffer =      (BYTE*) malloc(256 * 256 * 4);
  BYTE** buffer_rows = (BYTE**)malloc(256 * sizeof(BYTE*));
  memset(buffer, 0, 256 * 256 * 4);
  int i;
  for (i = 0; i < 256; i++)
    buffer_rows[i] = &buffer[256 * 4 * i];

  graphics_put_char(obj, c, buffer_rows, &w, &h);
  if (c == L' ')
    w += 8;

  if (state->x + w >= state->w)
    {
      free(buffer);
      free(buffer_rows);
      return 0;
    }
  if (w > (int)state->char_w)
    state->char_w = w;
  if (h > (int)state->line_h)
    state->line_h = h;

  int x;
  int y;
  for (x = 0; x < w; x++)
    for (y = 0; y < h; y++) {
      int x2 = (state->x + x) * 4 + state->channel;
      int y2 = state->y + y;
      switch (output_type)
	{
	case UNPACKED_RGBA:
	  dest[y2][x2 + 0] = buffer_rows[y][x * 4 + 0];
	  dest[y2][x2 + 1] = buffer_rows[y][x * 4 + 1];
	  dest[y2][x2 + 2] = buffer_rows[y][x * 4 + 2];
	  dest[y2][x2 + 3] = buffer_rows[y][x * 4 + 3];
	  break;
	case UNPACKED_GRAYSCALE:
	  dest[y2][state->x + x] = buffer_rows[y][x * 4];
	  break;
	case PACKED_RGBA:
	case PACKED_GRAYSCALE:
	  dest[y2][x2] = buffer_rows[y][x * 4];
	  break;
	case INVALID_FORMAT:
	  break;
	}
    }

  charDetail->x        = state->x;
  charDetail->y        = state->y;
  charDetail->width    = w;
  charDetail->height   = h;
  charDetail->y_offset = 0;
  charDetail->channel  = state->channel;

  free(buffer_rows);
  free(buffer);
  return 1;
}

BYTE *generate_bitmap_font(bool *chars_list, int chars_count, json_t *config, size_t *output_size)
{
	// OutputType outputType; // global

	output_type = PACKED_RGBA;
	void *obj = graphics_init(config);
	if (!obj)
		return NULL;

	State state;
	state.x = 0;
	state.y = 0;
	state.channel = 0;
	state.w = 2048;
	state.h = 1024;
	state.char_w = 0;
	state.line_h = 0;
	BYTE*  data = (BYTE*)malloc(state.w * state.h * 4);
	BYTE** rows = (BYTE**)malloc(state.h * sizeof(BYTE*));
	memset(data, 0, state.w * state.h * 4);
	unsigned int i;
	for (i = 0; i < state.h; i++)
		rows[i] = data + i * state.w * 4;
	CharDetail* charDetails = (CharDetail*)malloc(65536 * sizeof(CharDetail));

	uint16_t c;
	for (c = 0; c < 65535; c++) {
		if (chars_list[c]) {
			if (put_char(obj, c, rows, &state, &charDetails[c]) == 0)
			{
				// TODO: clear (x;y) -> (x+char_w;y+line_h)
				if (state.y + state.line_h + 256 > state.h)
				{
					state.h += 1024;
					data = (BYTE*)realloc(data, state.w * state.h * 4);
					rows = (BYTE**)realloc(rows, state.h * sizeof(BYTE*));
					for (i = 0; i < state.h; i++)
						rows[i] = data + i * state.w * 4;
				}
				state.y += state.line_h;
				state.x = 0;
				state.line_h = 0;

				// Redraw all the characters in the current cell
				state.char_w = 0;
				while (state.channel > 0)
				{
					c--;
					state.channel--;
				}
				c--;
				continue;
			}

			if (output_type != UNPACKED_RGBA && output_type != UNPACKED_GRAYSCALE)
				state.channel++;
			else
				state.channel += 4;
			if (state.channel == 4) {
				state.x += state.char_w;
				state.char_w = 0;
				state.channel = 0;
			}
		}
	}

  state.h = state.y + state.line_h;
  graphics_free(obj);

  if (output_type == PACKED_GRAYSCALE)
    state.w *= 4;
  BITMAPFILEHEADER header;
  BITMAPINFOHEADER info;
  header.bfType = 'B' | ('M' << 8);
  if (output_type != UNPACKED_GRAYSCALE && output_type != PACKED_GRAYSCALE)
    header.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + state.w * state.h * 4;
  else
    header.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + 256 * 4 + state.w * state.h;
  header.bfReserved1 = 0;
  header.bfReserved2 = 0;
  if (output_type != UNPACKED_GRAYSCALE && output_type != PACKED_GRAYSCALE)
    header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  else
    header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + 256 * 4;

  info.biSize = sizeof(BITMAPINFOHEADER);
  info.biWidth = state.w;
  info.biHeight = state.h;
  info.biPlanes = 1;
  if (output_type != UNPACKED_GRAYSCALE && output_type != PACKED_GRAYSCALE)
    info.biBitCount = 32;
  else
    info.biBitCount = 8;
  info.biCompression = BI_RGB;
  info.biSizeImage = 0;
  info.biXPelsPerMeter = 0;
  info.biYPelsPerMeter = 0;
  info.biClrUsed = 0;
  info.biClrImportant = 0;

  *output_size = sizeof(header) + sizeof(info) + (state.w * (info.biBitCount / 8) * state.h) + 4 + chars_count * (2 + sizeof(CharDetail));
  BYTE *output = new BYTE[*output_size];
  BYTE *output_ptr = output;
  if (!output)
    {
      perror("malloc");
      return NULL;
    }

  // Header
  memcpy(output_ptr, &header, sizeof(header));
  output_ptr += sizeof(header);
  memcpy(output_ptr, &info, sizeof(info));
  output_ptr += sizeof(info);

  // Bitmap
  int line;
  for (line = state.h - 1; line >= 0; line--) {
	  memcpy(output_ptr, rows[line], state.w * info.biBitCount / 8);
	  output_ptr += state.w * info.biBitCount / 8;
  }

  // Metadatas
  if (output_type == PACKED_RGBA || output_type == PACKED_GRAYSCALE)
    {
      uint16_t unk = 0x0215; // I don't know what is that, so I take the bytes in spell_font.bmp for now.
      uint16_t nb_chars = chars_count;
	  memcpy(output_ptr,     &unk, 2);
	  memcpy(output_ptr + 2, &nb_chars, 2);
	  output_ptr += 4;
	  for (c = 0; c < 65535; c++) {
		  if (chars_list[c]) {
			  memcpy(output_ptr, &c, 2);
			  output_ptr += 2;
		  }
	  }
	  for (c = 0; c < 65535; c++) {
		  if (chars_list[c]) {
			  memcpy(output_ptr, &charDetails[c], sizeof(CharDetail));
			  output_ptr += sizeof(CharDetail);
		  }
	  }
    }

  free(charDetails);
  free(rows);
  free(data);

  return output;
}
