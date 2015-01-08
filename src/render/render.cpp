// Aseprite Render Library
// Copyright (c) 2001-2014 David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "render/render.h"

#include "doc/doc.h"

namespace render {

//////////////////////////////////////////////////////////////////////
// Scaled composite

template<class DstTraits, class SrcTraits>
class BlenderHelper
{
  BLEND_COLOR m_blend_color;
  color_t m_mask_color;
public:
  BlenderHelper(const Image* src, const Palette* pal, int blend_mode)
  {
    m_blend_color = SrcTraits::get_blender(blend_mode);
    m_mask_color = src->maskColor();
  }
  inline void operator()(typename DstTraits::pixel_t& scanline,
                         const typename DstTraits::pixel_t& dst,
                         const typename SrcTraits::pixel_t& src,
                         int opacity)
  {
    if (src != m_mask_color)
      scanline = (*m_blend_color)(dst, src, opacity);
    else
      scanline = dst;
  }
};

template<>
class BlenderHelper<RgbTraits, GrayscaleTraits>
{
  BLEND_COLOR m_blend_color;
  color_t m_mask_color;
public:
  BlenderHelper(const Image* src, const Palette* pal, int blend_mode)
  {
    m_blend_color = RgbTraits::get_blender(blend_mode);
    m_mask_color = src->maskColor();
  }
  inline void operator()(RgbTraits::pixel_t& scanline,
                         const RgbTraits::pixel_t& dst,
                         const GrayscaleTraits::pixel_t& src,
                         int opacity)
  {
    if (src != m_mask_color) {
      int v = graya_getv(src);
      scanline = (*m_blend_color)(dst, rgba(v, v, v, graya_geta(src)), opacity);
    }
    else
      scanline = dst;
  }
};

template<>
class BlenderHelper<RgbTraits, IndexedTraits>
{
  const Palette* m_pal;
  int m_blend_mode;
  color_t m_mask_color;
public:
  BlenderHelper(const Image* src, const Palette* pal, int blend_mode)
  {
    m_blend_mode = blend_mode;
    m_mask_color = src->maskColor();
    m_pal = pal;
  }
  inline void operator()(RgbTraits::pixel_t& scanline,
                         const RgbTraits::pixel_t& dst,
                         const IndexedTraits::pixel_t& src,
                         int opacity)
  {
    if (m_blend_mode == BLEND_MODE_COPY) {
      scanline = m_pal->getEntry(src);
    }
    else {
      if (src != m_mask_color) {
        scanline = rgba_blend_normal(dst, m_pal->getEntry(src), opacity);
      }
      else
        scanline = dst;
    }
  }
};

template<class DstTraits, class SrcTraits>
static void compose_scaled_image_scale_up(
  Image* dst, const Image* src, const Palette* pal,
  gfx::Clip area,
  int opacity, int blend_mode, Zoom zoom)
{
  BlenderHelper<DstTraits, SrcTraits> blender(src, pal, blend_mode);
  int px_x, px_y;

  if (!area.clip(dst->width(), dst->height(),
      zoom.apply(src->width()),
      zoom.apply(src->height())))
    return;

  int px_w = zoom.apply(1);
  int px_h = zoom.apply(1);
  int first_px_w = px_w - (area.src.x % px_w);
  int first_px_h = px_h - (area.src.y % px_h);
  gfx::Rect srcBounds = zoom.remove(area.srcBounds());
  gfx::Rect dstBounds = area.dstBounds();
  int bottom = area.dst.y+area.size.h-1;
  int line_h;

  if ((area.src.x+area.size.w) % px_w > 0) ++srcBounds.w;
  if ((area.src.y+area.size.h) % px_h > 0) ++srcBounds.h;

  if (srcBounds.isEmpty())
    return;

  // the scanline variable is used to blend src/dst pixels one time for each pixel
  typedef std::vector<typename DstTraits::pixel_t> Scanline;
  Scanline scanline(srcBounds.w);
  typename Scanline::iterator scanline_it;
#ifdef _DEBUG
  typename Scanline::iterator scanline_end = scanline.end();
#endif

  // Lock all necessary bits
  const LockImageBits<SrcTraits> srcBits(src, srcBounds);
  LockImageBits<DstTraits> dstBits(dst, dstBounds);
  typename LockImageBits<SrcTraits>::const_iterator src_it = srcBits.begin();
#ifdef _DEBUG
  typename LockImageBits<SrcTraits>::const_iterator src_end = srcBits.end();
#endif
  typename LockImageBits<DstTraits>::iterator dst_it, dst_end;

  // For each line to draw of the source image...
  dstBounds.h = 1;
  for (int y=0; y<srcBounds.h; ++y) {
    dst_it = dstBits.begin_area(dstBounds);
    dst_end = dstBits.end_area(dstBounds);

    // Read 'src' and 'dst' and blend them, put the result in `scanline'
    scanline_it = scanline.begin();
    for (int x=0; x<srcBounds.w; ++x) {
      ASSERT(src_it >= srcBits.begin() && src_it < src_end);
      ASSERT(dst_it >= dstBits.begin() && dst_it < dst_end);
      ASSERT(scanline_it >= scanline.begin() && scanline_it < scanline_end);

      blender(*scanline_it, *dst_it, *src_it, opacity);
      ++src_it;

      int delta;
      if (x == 0)
        delta = first_px_w;
      else
        delta = px_w;

      while (dst_it != dst_end && delta-- > 0)
        ++dst_it;

      ++scanline_it;
    }

    // Get the 'height' of the line to be painted in 'dst'
    if ((y == 0) && (first_px_h > 0))
      line_h = first_px_h;
    else
      line_h = px_h;

    // Draw the line in 'dst'
    for (px_y=0; px_y<line_h; ++px_y) {
      dst_it = dstBits.begin_area(dstBounds);
      dst_end = dstBits.end_area(dstBounds);
      scanline_it = scanline.begin();

      int x = 0;

      // first pixel
      for (px_x=0; px_x<first_px_w; ++px_x) {
        ASSERT(scanline_it != scanline_end);
        ASSERT(dst_it != dst_end);

        *dst_it = *scanline_it;

        ++dst_it;
        if (dst_it == dst_end)
          goto done_with_line;
      }

      ++scanline_it;
      ++x;

      // the rest of the line
      for (; x<srcBounds.w; ++x) {
        for (px_x=0; px_x<px_w; ++px_x) {
          ASSERT(dst_it !=  dst_end);

          *dst_it = *scanline_it;

          ++dst_it;
          if (dst_it == dst_end)
            goto done_with_line;
        }

        ++scanline_it;
      }

done_with_line:;
      if (++dstBounds.y > bottom)
        goto done_with_blit;
    }
  }

done_with_blit:;
}

template<class DstTraits, class SrcTraits>
static void compose_scaled_image_scale_down(
  Image* dst, const Image* src, const Palette* pal,
  gfx::Clip area,
  int opacity, int blend_mode, Zoom zoom)
{
  BlenderHelper<DstTraits, SrcTraits> blender(src, pal, blend_mode);
  int unbox_w = zoom.remove(1);
  int unbox_h = zoom.remove(1);

  if (!area.clip(dst->width(), dst->height(),
      zoom.apply(src->width()),
      zoom.apply(src->height())))
    return;

  gfx::Rect srcBounds = zoom.remove(area.srcBounds());
  gfx::Rect dstBounds = area.dstBounds();
  int bottom = area.dst.y+area.size.h-1;

  if (srcBounds.isEmpty())
    return;

  // Lock all necessary bits
  const LockImageBits<SrcTraits> srcBits(src, srcBounds);
  LockImageBits<DstTraits> dstBits(dst, dstBounds);
  typename LockImageBits<SrcTraits>::const_iterator src_it = srcBits.begin();
  typename LockImageBits<SrcTraits>::const_iterator src_end = srcBits.end();
  typename LockImageBits<DstTraits>::iterator dst_it, dst_end;

  // For each line to draw of the source image...
  dstBounds.h = 1;
  for (int y=0; y<srcBounds.h; y+=unbox_h) {
    dst_it = dstBits.begin_area(dstBounds);
    dst_end = dstBits.end_area(dstBounds);

    for (int x=0; x<srcBounds.w; x+=unbox_w) {
      ASSERT(src_it >= srcBits.begin() && src_it < src_end);
      ASSERT(dst_it >= dstBits.begin() && dst_it < dst_end);

      blender(*dst_it, *dst_it, *src_it, opacity);

      // Skip source pixels
      for (int delta=0; delta < unbox_w && src_it != src_end; ++delta)
        ++src_it;

      ++dst_it;
    }

    if (++dstBounds.y > bottom)
      break;

    // Skip lines
    for (int delta=0; delta < srcBounds.w * (unbox_h-1) && src_it != src_end; ++delta)
      ++src_it;
  }
}

template<class DstTraits, class SrcTraits>
static void compose_scaled_image(
  Image* dst, const Image* src, const Palette* pal,
  const gfx::Clip& area,
  int opacity, int blend_mode, Zoom zoom)
{
  if (zoom.scale() >= 1.0)
    compose_scaled_image_scale_up<DstTraits, SrcTraits>(dst, src, pal, area, opacity, blend_mode, zoom);
  else
    compose_scaled_image_scale_down<DstTraits, SrcTraits>(dst, src, pal, area, opacity, blend_mode, zoom);
}

Render::Render()
  : m_sprite(NULL)
  , m_currentLayer(NULL)
  , m_currentFrame(0)
  , m_extraCel(NULL)
  , m_extraImage(NULL)
  , m_bgType(BgType::TRANSPARENT)
  , m_bgCheckedSize(16, 16)
  , m_globalOpacity(255)
  , m_onionskinType(OnionskinType::NONE)
{
}

void Render::setBgType(BgType type)
{
  m_bgType = type;
}

void Render::setBgZoom(bool state)
{
  m_bgZoom = state;
}

void Render::setBgColor1(color_t color)
{
  m_bgColor1 = color;
}

void Render::setBgColor2(color_t color)
{
  m_bgColor2 = color;
}

void Render::setBgCheckedSize(const gfx::Size& size)
{
  m_bgCheckedSize = size;
}

void Render::setPreviewImage(const Layer* layer, frame_t frame, Image* image)
{
  m_selectedLayer = layer;
  m_selectedFrame = frame;
  m_previewImage = image;
}

void Render::setExtraImage(
  const Cel* cel, const Image* image, int blendMode,
  const Layer* currentLayer,
  frame_t currentFrame)
{
  m_extraCel = cel;
  m_extraImage = image;
  m_extraBlendMode = blendMode;
  m_currentLayer = currentLayer;
  m_currentFrame = currentFrame;
}

void Render::removePreviewImage()
{
  m_previewImage = NULL;
}

void Render::removeExtraImage()
{
  m_extraCel = NULL;
}

void Render::setOnionskin(OnionskinType type, int prevs, int nexts, int opacityBase, int opacityStep)
{
  m_onionskinType = type;
  m_onionskinPrevs = prevs;
  m_onionskinNexts = nexts;
  m_onionskinOpacityBase = opacityBase;
  m_onionskinOpacityStep = opacityStep;
}

void Render::disableOnionskin()
{
  m_onionskinType = OnionskinType::NONE;
}

void Render::renderSprite(
  Image* dstImage,
  const Sprite* sprite,
  frame_t frame)
{
  renderSprite(dstImage, sprite, frame,
    gfx::Clip(sprite->bounds()), Zoom(1, 1));
}

void Render::renderSprite(
  Image* dstImage,
  const Sprite* sprite,
  frame_t frame,
  const gfx::Clip& area)
{
  renderSprite(dstImage, sprite, frame, area, Zoom(1, 1));
}

void Render::renderLayer(
  Image* dstImage,
  const Layer* layer,
  frame_t frame)
{
  renderLayer(dstImage, layer, frame,
    gfx::Clip(layer->sprite()->bounds()));
}

void Render::renderLayer(
  Image* dstImage,
  const Layer* layer,
  frame_t frame,
  const gfx::Clip& area)
{
  m_sprite = layer->sprite();

  RenderScaledImage scaled_func =
    getRenderScaledImageFunc(
      dstImage->pixelFormat(),
      m_sprite->pixelFormat());
  if (!scaled_func)
    return;

  const LayerImage* background = m_sprite->backgroundLayer();
  bool need_checked_bg = (background != NULL ? !background->isVisible(): true);
  color_t bg_color = 0;

  m_globalOpacity = 255;
  renderLayer(layer, dstImage, area,
    frame, Zoom(1, 1), scaled_func,
    true, true, -1);
}

void Render::renderSprite(
  Image* dstImage,
  const Sprite* sprite,
  frame_t frame,
  const gfx::Clip& area,
  Zoom zoom)
{
  m_sprite = sprite;

  RenderScaledImage scaled_func =
    getRenderScaledImageFunc(
      dstImage->pixelFormat(),
      m_sprite->pixelFormat());
  if (!scaled_func)
    return;

  const LayerImage* bgLayer = m_sprite->backgroundLayer();
  color_t bg_color = 0;
  if (m_sprite->pixelFormat() == IMAGE_INDEXED) {
    switch (dstImage->pixelFormat()) {
      case IMAGE_RGB:
      case IMAGE_GRAYSCALE:
        if (bgLayer && bgLayer->isVisible())
          bg_color = m_sprite->palette(frame)->getEntry(m_sprite->transparentColor());
        break;
      case IMAGE_INDEXED:
        bg_color = m_sprite->transparentColor();
        break;
    }
  }

  // Draw checked background
  switch (m_bgType) {

    case BgType::CHECKED:
      if (bgLayer && bgLayer->isVisible())
        fill_rect(dstImage, area.dstBounds(), bg_color);
      else
        renderBackground(dstImage, area, zoom);
      break;

    case BgType::TRANSPARENT:
      fill_rect(dstImage, area.dstBounds(), bg_color);
      break;
  }

  // Draw the current frame.
  m_globalOpacity = 255;
  renderLayer(
    m_sprite->folder(), dstImage,
    area, frame, zoom, scaled_func,
    true, true, -1);

  // Onion-skin feature: Draw previous/next frames with different
  // opacity (<255)
  if (m_onionskinType != OnionskinType::NONE) {
    for (frame_t f = frame - m_onionskinPrevs;
         f <= frame + m_onionskinNexts; ++f) {
      if (f == frame || f < 0 || f > m_sprite->lastFrame())
        continue;
      else if (f < frame)
        m_globalOpacity = m_onionskinOpacityBase - m_onionskinOpacityStep * ((frame - f)-1);
      else
        m_globalOpacity = m_onionskinOpacityBase - m_onionskinOpacityStep * ((f - frame)-1);

      if (m_globalOpacity > 0) {
        m_globalOpacity = MID(0, m_globalOpacity, 255);

        int blend_mode = -1;
        if (m_onionskinType == OnionskinType::MERGE)
          blend_mode = BLEND_MODE_NORMAL;
        else if (m_onionskinType == OnionskinType::RED_BLUE_TINT)
          blend_mode = (f < frame ? BLEND_MODE_RED_TINT: BLEND_MODE_BLUE_TINT);

        renderLayer(m_sprite->folder(), dstImage,
          area, f, zoom, scaled_func,
          true, true, blend_mode);
      }
    }
  }
}

void Render::renderBackground(Image* image,
  const gfx::Clip& area,
  Zoom zoom)
{
  int x, y, u, v;
  int tile_w = m_bgCheckedSize.w;
  int tile_h = m_bgCheckedSize.h;

  if (m_bgZoom) {
    tile_w = zoom.apply(tile_w);
    tile_h = zoom.apply(tile_h);
  }

  // Tile size
  if (tile_w < zoom.apply(1)) tile_w = zoom.apply(1);
  if (tile_h < zoom.apply(1)) tile_h = zoom.apply(1);

  if (tile_w < 1) tile_w = 1;
  if (tile_h < 1) tile_h = 1;

  // Tile position (u,v) is the number of tile we start in "area.src" coordinate
  u = (area.src.x / tile_w);
  v = (area.src.y / tile_h);

  // Position where we start drawing the first tile in "image"
  int x_start = -(area.src.x % tile_w);
  int y_start = -(area.src.y % tile_h);

  gfx::Rect dstBounds = area.dstBounds();

  // Draw checked background (tile by tile)
  int u_start = u;
  for (y=y_start-tile_h; y<image->height()+tile_h; y+=tile_h) {
    for (x=x_start-tile_w; x<image->width()+tile_w; x+=tile_w) {
      gfx::Rect fillRc = dstBounds.createIntersect(gfx::Rect(x, y, tile_w, tile_h));
      if (!fillRc.isEmpty())
        fill_rect(
          image, fillRc.x, fillRc.y, fillRc.x+fillRc.w-1, fillRc.y+fillRc.h-1,
          (((u+v))&1)? m_bgColor2: m_bgColor1);
      ++u;
    }
    u = u_start;
    ++v;
  }
}

void Render::renderImage(Image* dst_image, const Image* src_image,
  const Palette* pal, int x, int y, Zoom zoom, int opacity, int blend_mode)
{
  RenderScaledImage scaled_func = getRenderScaledImageFunc(
    dst_image->pixelFormat(),
    src_image->pixelFormat());
  if (!scaled_func)
    return;

  scaled_func(dst_image, src_image, pal,
    gfx::Clip(x, y, 0, 0,
      zoom.apply(src_image->width()),
      zoom.apply(src_image->height())),
    opacity, blend_mode, zoom);
}

void Render::renderLayer(
  const Layer* layer,
  Image *image,
  const gfx::Clip& area,
  frame_t frame, Zoom zoom,
  RenderScaledImage scaled_func,
  bool render_background,
  bool render_transparent,
  int blend_mode)
{
  // we can't read from this layer
  if (!layer->isVisible())
    return;

  switch (layer->type()) {

    case ObjectType::LayerImage: {
      if ((!render_background  &&  layer->isBackground()) ||
          (!render_transparent && !layer->isBackground()))
        break;

      const Cel* cel = layer->cel(frame);
      if (cel != NULL) {
        Image* src_image;

        // Is the 'm_previewImage' set to be used with this layer?
        if ((m_previewImage) &&
            (m_selectedLayer == layer) &&
            (m_selectedFrame == frame)) {
          src_image = m_previewImage;
        }
        // If not, we use the original cel-image from the images' stock
        else {
          src_image = cel->image();
        }

        if (src_image) {
          int t, output_opacity;

          output_opacity = MID(0, cel->opacity(), 255);
          output_opacity = INT_MULT(output_opacity, m_globalOpacity, t);

          ASSERT(src_image->maskColor() == m_sprite->transparentColor());

          renderCel(image, src_image,
            m_sprite->palette(frame),
            cel, area, scaled_func,
            output_opacity,
            (blend_mode < 0 ?
              static_cast<const LayerImage*>(layer)->getBlendMode():
              blend_mode),
            zoom);
        }
      }
      break;
    }

    case ObjectType::LayerFolder: {
      LayerConstIterator it = static_cast<const LayerFolder*>(layer)->getLayerBegin();
      LayerConstIterator end = static_cast<const LayerFolder*>(layer)->getLayerEnd();

      for (; it != end; ++it) {
        renderLayer(*it, image,
          area, frame, zoom, scaled_func,
          render_background,
          render_transparent,
          blend_mode);
      }
      break;
    }

  }

  // Draw extras
  if (m_extraCel &&
      m_extraImage &&
      layer == m_currentLayer &&
      frame == m_currentFrame) {
    if (m_extraCel->opacity() > 0) {
      renderCel(image, m_extraImage,
        m_sprite->palette(frame),
        m_extraCel, area, scaled_func,
        m_extraCel->opacity(),
        m_extraBlendMode, zoom);
    }
  }
}

void Render::renderCel(
  Image* dst_image,
  const Image* cel_image,
  const Palette* pal,
  const Cel* cel,
  const gfx::Clip& area,
  RenderScaledImage scaled_func,
  int opacity, int blend_mode, Zoom zoom)
{
  int cel_x = zoom.apply(cel->x());
  int cel_y = zoom.apply(cel->y());

  gfx::Rect src_bounds =
    area.srcBounds().createIntersect(
      gfx::Rect(
        cel_x, cel_y,
        zoom.apply(cel_image->width()),
        zoom.apply(cel_image->height())));
  if (src_bounds.isEmpty())
    return;

  (*scaled_func)(dst_image, cel_image, pal,
    gfx::Clip(
      area.dst.x + src_bounds.x - area.src.x,
      area.dst.y + src_bounds.y - area.src.y,
      src_bounds.x - cel_x,
      src_bounds.y - cel_y,
      src_bounds.w,
      src_bounds.h),
    opacity, blend_mode, zoom);
}

// static
Render::RenderScaledImage Render::getRenderScaledImageFunc(
  PixelFormat dstFormat,
  PixelFormat srcFormat)
{
  switch (srcFormat) {

    case IMAGE_RGB:
      switch (dstFormat) {
        case IMAGE_RGB:       return compose_scaled_image<RgbTraits, RgbTraits>;
        case IMAGE_GRAYSCALE: return compose_scaled_image<GrayscaleTraits, RgbTraits>;
        case IMAGE_INDEXED:   return compose_scaled_image<IndexedTraits, RgbTraits>;
      }
      break;

    case IMAGE_GRAYSCALE:
      switch (dstFormat) {
        case IMAGE_RGB:       return compose_scaled_image<RgbTraits, GrayscaleTraits>;
        case IMAGE_GRAYSCALE: return compose_scaled_image<GrayscaleTraits, GrayscaleTraits>;
        case IMAGE_INDEXED:   return compose_scaled_image<IndexedTraits, GrayscaleTraits>;
      }
      break;

    case IMAGE_INDEXED:
      switch (dstFormat) {
        case IMAGE_RGB:       return compose_scaled_image<RgbTraits, IndexedTraits>;
        case IMAGE_GRAYSCALE: return compose_scaled_image<GrayscaleTraits, IndexedTraits>;
        case IMAGE_INDEXED:   return compose_scaled_image<IndexedTraits, IndexedTraits>;
      }
      break;
  }

  ASSERT(false && "Invalid pixel formats");
  return NULL;
}

void composite_image(Image* dst, const Image* src,
  int x, int y, int opacity, int blend_mode)
{
  // As the background is not rendered in renderImage(), we don't need
  // to configure the Render instance's BgType.
  Render().renderImage(
    dst, src, NULL, x, y, Zoom(1, 1),
    opacity, blend_mode);
}

} // namespace render
