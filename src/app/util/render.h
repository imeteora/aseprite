/* Aseprite
 * Copyright (C) 2001-2013  David Capello
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef APP_UTIL_RENDER_H_INCLUDED
#define APP_UTIL_RENDER_H_INCLUDED
#pragma once

#include "app/color.h"
#include "app/zoom.h"
#include "doc/frame_number.h"
#include "doc/image_buffer.h"
#include "gfx/rect.h"

namespace doc {
  class Image;
  class Layer;
  class Palette;
  class Sprite;
}

namespace app {
  class Document;

  using namespace doc;

  class RenderEngine {
  public:
    RenderEngine(const Document* document,
                 const Sprite* sprite,
                 const Layer* currentLayer,
                 FrameNumber currentFrame);
  
    //////////////////////////////////////////////////////////////////////
    // Checked background configuration

    enum CheckedBgType { CHECKED_BG_16X16,
                         CHECKED_BG_8X8,
                         CHECKED_BG_4X4,
                         CHECKED_BG_2X2 };

    static void loadConfig();
    static CheckedBgType getCheckedBgType();
    static void setCheckedBgType(CheckedBgType type);
    static bool getCheckedBgZoom();
    static void setCheckedBgZoom(bool state);
    static app::Color getCheckedBgColor1();
    static void setCheckedBgColor1(const app::Color& color);
    static app::Color getCheckedBgColor2();
    static void setCheckedBgColor2(const app::Color& color);

    //////////////////////////////////////////////////////////////////////
    // Preview image

    static void setPreviewImage(const Layer* layer, FrameNumber frame, Image* drawable);

    //////////////////////////////////////////////////////////////////////
    // Main function used by sprite-editors to render the sprite.
    // Draws the given sprite frame in a new image and return it.
    // Note: zoomedRect must have the zoom applied (zoomedRect = zoom.apply(spriteRect)).
    Image* renderSprite(const gfx::Rect& zoomedRect,
      FrameNumber frame, Zoom zoom,
      bool draw_tiled_bg,
      bool enable_onionskin,
      ImageBufferPtr& buffer);

    //////////////////////////////////////////////////////////////////////
    // Extra functions

    static void renderCheckedBackground(Image* image,
                                        int source_x, int source_y,
                                        Zoom zoom);

    static void renderImage(Image* rgb_image, Image* src_image, const Palette* pal,
                            int x, int y, Zoom zoom);

  private:
    void renderLayer(
      const Layer* layer,
      Image* image,
      int source_x, int source_y,
      FrameNumber frame, Zoom zoom,
      void (*zoomed_func)(Image*, const Image*, const Palette*, int, int, int, int, Zoom),
      bool render_background,
      bool render_transparent,
      int blend_mode);

    const Document* m_document;
    const Sprite* m_sprite;
    const Layer* m_currentLayer;
    FrameNumber m_currentFrame;
  };

} // namespace app

#endif
