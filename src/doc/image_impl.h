// Aseprite Document Library
// Copyright (c) 2001-2014 David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifndef DOC_IMAGE_IMPL_H_INCLUDED
#define DOC_IMAGE_IMPL_H_INCLUDED
#pragma once

#include "doc/blend.h"
#include "doc/image.h"
#include "doc/image_bits.h"
#include "doc/image_iterator.h"
#include "doc/palette.h"

namespace doc {

  template<class Traits>
  class ImageImpl : public Image {
  private:
    typedef typename Traits::address_t address_t;
    typedef typename Traits::const_address_t const_address_t;

    ImageBufferPtr m_buffer;
    address_t m_bits;
    address_t* m_rows;

    inline address_t getBitsAddress() {
      return m_bits;
    }

    inline const_address_t getBitsAddress() const {
      return m_bits;
    }

    inline address_t getLineAddress(int y) {
      ASSERT(y >= 0 && y < height());
      return m_rows[y];
    }

    inline const_address_t getLineAddress(int y) const {
      ASSERT(y >= 0 && y < height());
      return m_rows[y];
    }

  public:
    inline address_t address(int x, int y) const {
      return (address_t)(m_rows[y] + x / (Traits::pixels_per_byte == 0 ? 1 : Traits::pixels_per_byte));
    }

    ImageImpl(int width, int height,
              const ImageBufferPtr& buffer)
      : Image(static_cast<PixelFormat>(Traits::pixel_format), width, height)
      , m_buffer(buffer)
    {
      size_t for_rows = sizeof(address_t) * height;
      size_t rowstride_bytes = Traits::getRowStrideBytes(width);
      size_t required_size = for_rows + rowstride_bytes*height;

      if (!m_buffer)
        m_buffer.reset(new ImageBuffer(required_size));
      else
        m_buffer->resizeIfNecessary(required_size);

      m_rows = (address_t*)m_buffer->buffer();
      m_bits = (address_t)(m_buffer->buffer() + for_rows);

      address_t addr = m_bits;
      for (int y=0; y<height; ++y) {
        m_rows[y] = addr;
        addr = (address_t)(((uint8_t*)addr) + rowstride_bytes);
      }
    }

    uint8_t* getPixelAddress(int x, int y) const override {
      ASSERT(x >= 0 && x < width());
      ASSERT(y >= 0 && y < height());

      return (uint8_t*)address(x, y);
    }

    color_t getPixel(int x, int y) const override {
      ASSERT(x >= 0 && x < width());
      ASSERT(y >= 0 && y < height());

      return *address(x, y);
    }

    void putPixel(int x, int y, color_t color) override {
      ASSERT(x >= 0 && x < width());
      ASSERT(y >= 0 && y < height());

      *address(x, y) = color;
    }

    void clear(color_t color) override {
      LockImageBits<Traits> bits(this);
      typename LockImageBits<Traits>::iterator it(bits.begin());
      typename LockImageBits<Traits>::iterator end(bits.end());

      for (; it != end; ++it)
        *it = color;
    }

    void copy(const Image* _src, int x, int y) override {
      const ImageImpl<Traits>* src = (const ImageImpl<Traits>*)_src;
      ImageImpl<Traits>* dst = this;
      address_t src_address;
      address_t dst_address;
      int xbeg, xend, xsrc;
      int ybeg, yend, ysrc, ydst;
      int bytes;

      // Clipping

      xsrc = 0;
      ysrc = 0;

      xbeg = x;
      ybeg = y;
      xend = x+src->width()-1;
      yend = y+src->height()-1;

      if ((xend < 0) || (xbeg >= dst->width()) ||
          (yend < 0) || (ybeg >= dst->height()))
        return;

      if (xbeg < 0) {
        xsrc -= xbeg;
        xbeg = 0;
      }

      if (ybeg < 0) {
        ysrc -= ybeg;
        ybeg = 0;
      }

      if (xend >= dst->width())
        xend = dst->width()-1;

      if (yend >= dst->height())
        yend = dst->height()-1;

      // Copy process

      bytes = Traits::getRowStrideBytes(xend - xbeg + 1);

      for (ydst=ybeg; ydst<=yend; ++ydst, ++ysrc) {
        src_address = src->address(xsrc, ysrc);
        dst_address = dst->address(xbeg, ydst);

        memcpy(dst_address, src_address, bytes);
      }
    }

    void merge(const Image* _src, int x, int y, int opacity, int blend_mode) override {
      BLEND_COLOR blender = Traits::get_blender(blend_mode);
      const ImageImpl<Traits>* src = (const ImageImpl<Traits>*)_src;
      ImageImpl<Traits>* dst = this;
      address_t src_address;
      address_t dst_address;
      int xbeg, xend, xsrc, xdst;
      int ybeg, yend, ysrc, ydst;
      uint32_t mask_color = src->maskColor();

      // nothing to do
      if (!opacity)
        return;

      // clipping

      xsrc = 0;
      ysrc = 0;

      xbeg = x;
      ybeg = y;
      xend = x+src->width()-1;
      yend = y+src->height()-1;

      if ((xend < 0) || (xbeg >= dst->width()) ||
          (yend < 0) || (ybeg >= dst->height()))
        return;

      if (xbeg < 0) {
        xsrc -= xbeg;
        xbeg = 0;
      }

      if (ybeg < 0) {
        ysrc -= ybeg;
        ybeg = 0;
      }

      if (xend >= dst->width())
        xend = dst->width()-1;

      if (yend >= dst->height())
        yend = dst->height()-1;

      // Merge process

      for (ydst=ybeg; ydst<=yend; ++ydst, ++ysrc) {
        src_address = (address_t)src->address(xsrc, ysrc);
        dst_address = (address_t)dst->address(xbeg, ydst);

        for (xdst=xbeg; xdst<=xend; ++xdst) {
          if (*src_address != mask_color)
            *dst_address = (*blender)(*dst_address, *src_address, opacity);

          ++dst_address;
          ++src_address;
        }
      }
    }

    void drawHLine(int x1, int y, int x2, color_t color) override {
      LockImageBits<Traits> bits(this, gfx::Rect(x1, y, x2 - x1 + 1, 1));
      typename LockImageBits<Traits>::iterator it(bits.begin());
      typename LockImageBits<Traits>::iterator end(bits.end());

      for (; it != end; ++it)
        *it = color;
    }

    void fillRect(int x1, int y1, int x2, int y2, color_t color) override {
      for (int y=y1; y<=y2; ++y)
        ImageImpl<Traits>::drawHLine(x1, y, x2, color);
    }

    void blendRect(int x1, int y1, int x2, int y2, color_t color, int opacity) override {
      fillRect(x1, y1, x2, y2, color);
    }
  };

  //////////////////////////////////////////////////////////////////////
  // Specializations

  template<>
  inline void ImageImpl<IndexedTraits>::clear(color_t color) {
    memset(m_bits, color, width()*height());
  }

  template<>
  inline void ImageImpl<BitmapTraits>::clear(color_t color) {
    memset(m_bits, (color ? 0xff: 0x00),
           BitmapTraits::getRowStrideBytes(width()) * height());
  }

  template<>
  inline color_t ImageImpl<BitmapTraits>::getPixel(int x, int y) const {
    ASSERT(x >= 0 && x < width());
    ASSERT(y >= 0 && y < height());

    div_t d = div(x, 8);
    return ((*(m_rows[y] + d.quot)) & (1<<d.rem)) ? 1: 0;
  }

  template<>
  inline void ImageImpl<BitmapTraits>::putPixel(int x, int y, color_t color) {
    ASSERT(x >= 0 && x < width());
    ASSERT(y >= 0 && y < height());

    div_t d = div(x, 8);
    if (color)
      (*(m_rows[y] + d.quot)) |= (1 << d.rem);
    else
      (*(m_rows[y] + d.quot)) &= ~(1 << d.rem);
  }

  template<>
  inline void ImageImpl<RgbTraits>::blendRect(int x1, int y1, int x2, int y2, color_t color, int opacity) {
    address_t addr;
    int x, y;

    for (y=y1; y<=y2; ++y) {
      addr = (address_t)getPixelAddress(x1, y);
      for (x=x1; x<=x2; ++x) {
        *addr = rgba_blend_normal(*addr, color, opacity);
        ++addr;
      }
    }
  }

  template<>
  inline void ImageImpl<IndexedTraits>::merge(const Image* src, int x, int y, int opacity, int blend_mode) {
    Image* dst = this;
    address_t src_address;
    address_t dst_address;
    int xbeg, xend, xsrc, xdst;
    int ybeg, yend, ysrc, ydst;

    // clipping

    xsrc = 0;
    ysrc = 0;

    xbeg = x;
    ybeg = y;
    xend = x+src->width()-1;
    yend = y+src->height()-1;

    if ((xend < 0) || (xbeg >= dst->width()) ||
        (yend < 0) || (ybeg >= dst->height()))
      return;

    if (xbeg < 0) {
      xsrc -= xbeg;
      xbeg = 0;
    }

    if (ybeg < 0) {
      ysrc -= ybeg;
      ybeg = 0;
    }

    if (xend >= dst->width())
      xend = dst->width()-1;

    if (yend >= dst->height())
      yend = dst->height()-1;

    // merge process

    // direct copy
    if (blend_mode == BLEND_MODE_COPY) {
      for (ydst=ybeg; ydst<=yend; ++ydst, ++ysrc) {
        src_address = src->getPixelAddress(xsrc, ysrc);
        dst_address = dst->getPixelAddress(xbeg, ydst);

        for (xdst=xbeg; xdst<=xend; xdst++) {
          *dst_address = (*src_address);

          ++dst_address;
          ++src_address;
        }
      }
    }
    // with mask
    else {
      int mask_color = src->maskColor();

      for (ydst=ybeg; ydst<=yend; ++ydst, ++ysrc) {
        src_address = src->getPixelAddress(xsrc, ysrc);
        dst_address = dst->getPixelAddress(xbeg, ydst);

        for (xdst=xbeg; xdst<=xend; ++xdst) {
          if (*src_address != mask_color)
            *dst_address = (*src_address);

          ++dst_address;
          ++src_address;
        }
      }
    }
  }

  template<>
  inline void ImageImpl<BitmapTraits>::copy(const Image* src, int x, int y) {
    Image* dst = this;
    int xbeg, xend, xsrc, xdst;
    int ybeg, yend, ysrc, ydst;

    // clipping

    xsrc = 0;
    ysrc = 0;

    xbeg = x;
    ybeg = y;
    xend = x+src->width()-1;
    yend = y+src->height()-1;

    if ((xend < 0) || (xbeg >= dst->width()) ||
        (yend < 0) || (ybeg >= dst->height()))
      return;

    if (xbeg < 0) {
      xsrc -= xbeg;
      xbeg = 0;
    }

    if (ybeg < 0) {
      ysrc -= ybeg;
      ybeg = 0;
    }

    if (xend >= dst->width())
      xend = dst->width()-1;

    if (yend >= dst->height())
      yend = dst->height()-1;

    // copy process

    int w = xend - xbeg + 1;
    int h = yend - ybeg + 1;
    ImageConstIterator<BitmapTraits> src_it(src, gfx::Rect(xsrc, ysrc, w, h), xsrc, ysrc);
    ImageIterator<BitmapTraits> dst_it(dst, gfx::Rect(xbeg, ybeg, w, h), xbeg, ybeg);

    for (ydst=ybeg; ydst<=yend; ++ydst, ++ysrc) {
      for (xdst=xbeg; xdst<=xend; ++xdst) {
        *dst_it = *src_it;
        ++src_it;
        ++dst_it;
      }
    }
  }

  template<>
  inline void ImageImpl<BitmapTraits>::merge(const Image* src, int x, int y, int opacity, int blend_mode) {
    Image* dst = this;
    int xbeg, xend, xsrc, xdst;
    int ybeg, yend, ysrc, ydst;

    // clipping

    xsrc = 0;
    ysrc = 0;

    xbeg = x;
    ybeg = y;
    xend = x+src->width()-1;
    yend = y+src->height()-1;

    if ((xend < 0) || (xbeg >= dst->width()) ||
        (yend < 0) || (ybeg >= dst->height()))
      return;

    if (xbeg < 0) {
      xsrc -= xbeg;
      xbeg = 0;
    }

    if (ybeg < 0) {
      ysrc -= ybeg;
      ybeg = 0;
    }

    if (xend >= dst->width())
      xend = dst->width()-1;

    if (yend >= dst->height())
      yend = dst->height()-1;

    // merge process

    int w = xend - xbeg + 1;
    int h = yend - ybeg + 1;
    ImageConstIterator<BitmapTraits> src_it(src, gfx::Rect(xsrc, ysrc, w, h), xsrc, ysrc);
    ImageIterator<BitmapTraits> dst_it(dst, gfx::Rect(xbeg, ybeg, w, h), xbeg, ybeg);

    for (ydst=ybeg; ydst<=yend; ++ydst, ++ysrc) {
      for (xdst=xbeg; xdst<=xend; ++xdst) {
        if (*dst_it != 0)
          *dst_it = *src_it;
        ++src_it;
        ++dst_it;
      }
    }
  }

} // namespace doc

#endif
