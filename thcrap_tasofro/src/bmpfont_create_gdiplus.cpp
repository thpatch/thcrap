// Copy-pasted from https://github.com/brliron/135tk/tree/master/bmpfont

#include <thcrap.h>
#include <Gdiplus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bmpfont_create.h"

/*
** GDI+ code adapted from https://www.codeproject.com/Articles/42529/Outline-Text
*/

class GdiPlusGraphics
{
private:
  static ULONG_PTR gdiplusToken;

public:
  static void initGdiplus();
  static void freeGdiplus();

  Gdiplus::PrivateFontCollection fontCollection;
  Gdiplus::FontFamily* font;

  HDC     hdc;
  HBITMAP hBmp;
  HGDIOBJ hOrigBmp;
  BYTE*   bmpData;

  int font_size;
  Gdiplus::Color bg;
  Gdiplus::Color outline;
  Gdiplus::Color text;
  INT style;
  int outline_width;
  int margin;

  GdiPlusGraphics();
  ~GdiPlusGraphics();
};
ULONG_PTR GdiPlusGraphics::gdiplusToken;

GdiPlusGraphics::GdiPlusGraphics()
  : font(nullptr), hdc(nullptr), hBmp(nullptr), hOrigBmp(nullptr),
    font_size(20), bg(0, 0, 0), outline(150, 150, 150), text(255, 255, 255), outline_width(4)
{
  HDC hScreen = GetDC(NULL);
  this->hdc  = CreateCompatibleDC(hScreen);
  this->hBmp = CreateCompatibleBitmap(hScreen, 256, 256);
  ReleaseDC(NULL, hScreen);
  this->hOrigBmp = SelectObject(this->hdc, this->hBmp);

  this->bmpData = new BYTE[256 * 256 * 4];
}

GdiPlusGraphics::~GdiPlusGraphics()
{
  delete this->font;

  SelectObject(this->hdc, this->hOrigBmp);
  DeleteObject(this->hBmp);
  DeleteDC(this->hdc);

  delete[] this->bmpData;
}

void GdiPlusGraphics::initGdiplus()
{
  Gdiplus::GdiplusStartupInput gdiplusStartupInput;
  Gdiplus::GdiplusStartup(&GdiPlusGraphics::gdiplusToken, &gdiplusStartupInput, NULL);
}

void GdiPlusGraphics::freeGdiplus()
{
  Gdiplus::GdiplusShutdown(GdiPlusGraphics::gdiplusToken);
}

Gdiplus::Color json_to_color(json_t *obj)
{
	return Gdiplus::Color((BYTE)json_integer_value(json_array_get(obj, 0)),
		(BYTE)json_integer_value(json_array_get(obj, 1)),
		(BYTE)json_integer_value(json_array_get(obj, 2)));
}

void* graphics_init(json_t *config)
{
  GdiPlusGraphics::initGdiplus();
  GdiPlusGraphics* obj = new GdiPlusGraphics;

  // Add the font to the collection
  json_t *chain = resolve_chain(json_string_value(json_object_get(config, "font_file")));
  if (json_array_size(chain)) {
	  size_t font_file_size;
	  log_printf("(Data) Resolving %s... ", json_array_get_string(chain, 0));
	  void *font_file = stack_file_resolve_chain(chain, &font_file_size);
	  Gdiplus::Status status = obj->fontCollection.AddMemoryFont(font_file, font_file_size);
	  if (status != Gdiplus::Ok) {
		  log_printf("AddMemoryFont failed: %d\n", status);
	  }
	  free(font_file);
  }
  json_decref(chain);

  // Load values from config
  obj->font_size     = (int)json_integer_value(json_object_get(config, "font_size"));
  obj->bg            = json_to_color(          json_object_get(config, "bg_color"));
  obj->outline       = json_to_color(          json_object_get(config, "outline_color"));
  obj->outline_width = (int)json_integer_value(json_object_get(config, "outline_width"));
  obj->text          = json_to_color(          json_object_get(config, "fg_color"));
  obj->margin        = (int)json_integer_value(json_object_get(config, "margin"));

  obj->style		 = Gdiplus::FontStyleRegular;
  if (json_is_true(json_object_get(config, "is_bold"))) {
	  obj->style	 |= Gdiplus::FontStyleBold;
  }

  // Create the font
  const char *font_name = json_string_value(json_object_get(config, "font_name"));
  WCHAR_T_DEC(font_name);
  WCHAR_T_CONV(font_name);
  obj->font = new Gdiplus::FontFamily(font_name_w, &obj->fontCollection);
  WCHAR_T_FREE(font_name);
  if (!obj->font->IsAvailable())
    {
      log_printf("Could not open font %s\n", font_name);
      delete obj;
      GdiPlusGraphics::freeGdiplus();
      return NULL;
    }

  return obj;
}

void graphics_free(void* obj_)
{
  GdiPlusGraphics* obj = (GdiPlusGraphics*)obj_;
  delete obj;
  GdiPlusGraphics::freeGdiplus();
}

void graphics_put_char(void* obj_, WCHAR c, BYTE** dest, int* w, int* h)
{
  GdiPlusGraphics* obj = (GdiPlusGraphics*)obj_;
  Gdiplus::Rect rect;

  {
    Gdiplus::Graphics graphics(obj->hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.Clear(obj->bg);

    Gdiplus::GraphicsPath path;
    Gdiplus::StringFormat strFormat;
    path.AddString(&c, 1, obj->font, obj->style, (Gdiplus::REAL)obj->font_size, Gdiplus::Point(0, 0), &strFormat);

    Gdiplus::Pen pen(obj->outline, (Gdiplus::REAL)obj->outline_width);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    Gdiplus::SolidBrush brush(obj->text);

    graphics.DrawPath(&pen, &path);
    graphics.FillPath(&brush, &path);

    path.GetBounds(&rect, NULL, &pen);
  }

  BITMAPINFO info;
  memset(&info, 0, sizeof(info));
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = 256;
  info.bmiHeader.biHeight = 256;
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  info.bmiHeader.biSizeImage = 0;
  info.bmiHeader.biXPelsPerMeter = 0;
  info.bmiHeader.biYPelsPerMeter = 0;
  info.bmiHeader.biClrUsed = 0;
  info.bmiHeader.biClrImportant = 0;
  GetDIBits(obj->hdc, obj->hBmp, 0, 256, obj->bmpData, &info, DIB_RGB_COLORS);

  // Keep the rect inside the bitmap
  if (rect.X < 0)
    rect.X = 0;
  if (rect.Y < 0)
    rect.Y = 0;
  // Fix left bound
  for (int i = rect.X; i < rect.GetRight(); i++)
    {
      int j;
      for (j = rect.Y; j < rect.GetBottom(); j++)
	if (obj->bmpData[(255 - j) * 256 * 4 + i * 4 + 0] != 0 ||
	    obj->bmpData[(255 - j) * 256 * 4 + i * 4 + 1] != 0 ||
	    obj->bmpData[(255 - j) * 256 * 4 + i * 4 + 2] != 0)
	  break;
      if (j != rect.GetBottom())
	break;
      rect.X++;
      rect.Width--;
    }
  // Fix right bound
  for (int i = rect.X; i < rect.GetRight(); i++)
    {
      int j;
      for (j = rect.Y; j < rect.GetBottom(); j++)
	if (obj->bmpData[(255 - j) * 256 * 4 + i * 4 + 0] != 0 ||
	    obj->bmpData[(255 - j) * 256 * 4 + i * 4 + 1] != 0 ||
	    obj->bmpData[(255 - j) * 256 * 4 + i * 4 + 2] != 0)
	  break;
      if (j == rect.GetBottom())
	{
	  rect.Width = i - rect.X;
	  break;
	}
    }

  for (int y = 0; y < rect.GetBottom(); y++)
    memcpy(dest[y], &obj->bmpData[(255 - y) * 256 * 4 + rect.X * 4], rect.Width * 4);

  *w = rect.Width + obj->margin;
  *h = rect.GetBottom();
}
