// Aseprite UI Library
// Copyright (C) 2001-2013  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

/* Original code from:
   allegro/tools/datedit.c
   allegro/tools/plugins/datfont.c
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "she/alleg4/font.h"

#include "she/alleg4/surface.h"
#include "she/display.h"
#include "she/system.h"

#include <allegro.h>
#include <allegro/internal/aintern.h>

namespace {

/* state information for the bitmap font importer */
static BITMAP *import_bmp = NULL;

static int import_x = 0;
static int import_y = 0;

/* splits bitmaps into sub-sprites, using regions bounded by col #255 */
static void datedit_find_character(BITMAP *bmp, int *x, int *y, int *w, int *h)
{
  int c1;
  int c2;

  if (bitmap_color_depth(bmp) == 8) {
    c1 = 255;
    c2 = 255;
  }
  else {
    c1 = makecol_depth(bitmap_color_depth(bmp), 255, 255, 0);
    c2 = makecol_depth(bitmap_color_depth(bmp), 0, 255, 255);
  }

  /* look for top left corner of character */
  while ((getpixel(bmp, *x, *y) != c1) ||
         (getpixel(bmp, *x+1, *y) != c2) ||
         (getpixel(bmp, *x, *y+1) != c2) ||
         (getpixel(bmp, *x+1, *y+1) == c1) ||
         (getpixel(bmp, *x+1, *y+1) == c2)) {
    (*x)++;
    if (*x >= bmp->w) {
      *x = 0;
      (*y)++;
      if (*y >= bmp->h) {
        *w = 0;
        *h = 0;
        return;
      }
    }
  }

  /* look for right edge of character */
  *w = 0;
  while ((getpixel(bmp, *x+*w+1, *y) == c2) &&
         (getpixel(bmp, *x+*w+1, *y+1) != c2) &&
         (*x+*w+1 <= bmp->w))
    (*w)++;

  /* look for bottom edge of character */
  *h = 0;
  while ((getpixel(bmp, *x, *y+*h+1) == c2) &&
         (getpixel(bmp, *x+1, *y+*h+1) != c2) &&
         (*y+*h+1 <= bmp->h))
    (*h)++;
}

/* import_bitmap_font_mono:
 *  Helper for import_bitmap_font, below.
 */
static int import_bitmap_font_mono(FONT_GLYPH** gl, int num)
{
  int w = 1, h = 1, i;

  for(i = 0; i < num; i++) {
    if(w > 0 && h > 0) datedit_find_character(import_bmp, &import_x, &import_y, &w, &h);
    if(w <= 0 || h <= 0) {
      int j;

      gl[i] = (FONT_GLYPH*)_al_malloc(sizeof(FONT_GLYPH) + 8);
      gl[i]->w = 8;
      gl[i]->h = 8;

      for(j = 0; j < 8; j++) gl[i]->dat[j] = 0;
    } else {
      int sx = ((w + 7) / 8), j, k;

      gl[i] = (FONT_GLYPH*)_al_malloc(sizeof(FONT_GLYPH) + sx * h);
      gl[i]->w = w;
      gl[i]->h = h;

      for(j = 0; j < sx * h; j++) gl[i]->dat[j] = 0;
      for(j = 0; j < h; j++) {
        for(k = 0; k < w; k++) {
          if(getpixel(import_bmp, import_x + k + 1, import_y + j + 1))
            gl[i]->dat[(j * sx) + (k / 8)] |= 0x80 >> (k & 7);
        }
      }

      import_x += w;
    }
  }

  return 0;
}

/* import_bitmap_font_color:
 *  Helper for import_bitmap_font, below.
 */
static int import_bitmap_font_color(BITMAP** bits, int num)
{
  int w = 1, h = 1, i;

  for(i = 0; i < num; i++) {
    if(w > 0 && h > 0) datedit_find_character(import_bmp, &import_x, &import_y, &w, &h);
    if(w <= 0 || h <= 0) {
      bits[i] = create_bitmap_ex(8, 8, 8);
      if(!bits[i]) return -1;
      clear_to_color(bits[i], 255);
    } else {
      bits[i] = create_bitmap_ex(8, w, h);
      if(!bits[i]) return -1;
      blit(import_bmp, bits[i], import_x + 1, import_y + 1, 0, 0, w, h);
      import_x += w;
    }
  }

  return 0;
}

/* bitmap_font_ismono:
 *  Helper for import_bitmap_font, below.
 */
static int bitmap_font_ismono(BITMAP *bmp)
{
  int x, y, col = -1, pixel;

  for(y = 0; y < bmp->h; y++) {
    for(x = 0; x < bmp->w; x++) {
      pixel = getpixel(bmp, x, y);
      if(pixel == 0 || pixel == 255) continue;
      if(col > 0 && pixel != col) return 0;
      col = pixel;
    }
  }

  return 1;
}

/* bitmap_font_count:
 *  Helper for `import_bitmap_font', below.
 */
static int bitmap_font_count(BITMAP* bmp)
{
  int x = 0, y = 0, w = 0, h = 0;
  int num = 0;

  while(1) {
    datedit_find_character(bmp, &x, &y, &w, &h);
    if (w <= 0 || h <= 0) break;
    num++;
    x += w;
  }

  return num;
}

FONT* bitmap_to_font(BITMAP* bmp)
{
  FONT *f;
  int begin = ' ';
  int end = -1;

  import_bmp = bmp;
  import_x = 0;
  import_y = 0;

  if(bitmap_color_depth(import_bmp) != 8) {
    import_bmp = NULL;
    return 0;
  }

  f = (FONT*)_al_malloc(sizeof(FONT));
  if(end == -1) end = bitmap_font_count(import_bmp) + begin;

  if (bitmap_font_ismono(import_bmp)) {

    FONT_MONO_DATA* mf = (FONT_MONO_DATA*)_al_malloc(sizeof(FONT_MONO_DATA));

    mf->glyphs = (FONT_GLYPH**)_al_malloc(sizeof(FONT_GLYPH*) * (end - begin));

    if( import_bitmap_font_mono(mf->glyphs, end - begin) ) {

      free(mf->glyphs);
      free(mf);
      free(f);
      f = 0;

    } else {

      f->data = mf;
      f->vtable = font_vtable_mono;
      f->height = mf->glyphs[0]->h;

      mf->begin = begin;
      mf->end = end;
      mf->next = 0;
    }

  } else {

    FONT_COLOR_DATA* cf = (FONT_COLOR_DATA*)_al_malloc(sizeof(FONT_COLOR_DATA));
    cf->bitmaps = (BITMAP**)_al_malloc(sizeof(BITMAP*) * (end - begin));

    if( import_bitmap_font_color(cf->bitmaps, end - begin) ) {

      free(cf->bitmaps);
      free(cf);
      free(f);
      f = 0;

    } else {

      f->data = cf;
      f->vtable = font_vtable_color;
      f->height = cf->bitmaps[0]->h;

      cf->begin = begin;
      cf->end = end;
      cf->next = 0;

    }

  }

  import_bmp = NULL;

  return f;
}

}

namespace she {

Font* load_bitmap_font(const char* filename, int scale)
{
  FONT* allegFont = NULL;
  int old_color_conv = _color_conv;

  set_color_conversion(COLORCONV_NONE);
  PALETTE junk;
  BITMAP* bmp = load_bitmap(filename, junk);
  set_color_conversion(old_color_conv);

  if (bmp) {
    Alleg4Surface sur(bmp, Alleg4Surface::DestroyHandle);
    if (scale > 1)
      sur.applyScale(scale);
    
    allegFont = bitmap_to_font(reinterpret_cast<BITMAP*>(sur.nativeHandle()));
  }

  if (allegFont)
    return new Alleg4Font(allegFont, Alleg4Font::DeleteAndDestroy);
  else
    return NULL;
}

} // namespace she
