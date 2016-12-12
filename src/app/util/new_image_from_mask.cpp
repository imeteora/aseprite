// Aseprite
// Copyright (C) 2001-2016  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/util/new_image_from_mask.h"

#include "app/document.h"
#include "base/unique_ptr.h"
#include "doc/image_impl.h"
#include "doc/mask.h"
#include "doc/primitives.h"
#include "doc/site.h"
#include "render/render.h"

namespace app {

using namespace doc;

Image* new_image_from_mask(const Site& site)
{
  const Mask* srcMask = static_cast<const app::Document*>(site.document())->mask();
  return new_image_from_mask(site, srcMask);
}

doc::Image* new_image_from_mask(const doc::Site& site,
                                const doc::Mask* srcMask,
                                bool merged)
{
  const Sprite* srcSprite = site.sprite();
  ASSERT(srcSprite);
  ASSERT(srcMask);

  const Image* srcMaskBitmap = srcMask->bitmap();
  const gfx::Rect& srcBounds = srcMask->bounds();

  ASSERT(srcMaskBitmap);
  ASSERT(!srcBounds.isEmpty());

  base::UniquePtr<Image> dst(Image::create(srcSprite->pixelFormat(), srcBounds.w, srcBounds.h));
  if (!dst)
    return nullptr;

  // Clear the new image
  dst->setMaskColor(srcSprite->transparentColor());
  clear_image(dst, dst->maskColor());

  const Image* src = nullptr;
  int x = 0, y = 0;
  if (merged) {
    render::Render render;
    render.renderSprite(dst, srcSprite, site.frame(),
                        gfx::Clip(0, 0, srcBounds));

    src = dst;
  }
  else {
    src = site.image(&x, &y);
  }

  // Copy the masked zones
  if (src) {
    if (srcMaskBitmap) {
      // Copy active layer with mask
      const LockImageBits<BitmapTraits> maskBits(srcMaskBitmap, gfx::Rect(0, 0, srcBounds.w, srcBounds.h));
      LockImageBits<BitmapTraits>::const_iterator mask_it = maskBits.begin();

      for (int v=0; v<srcBounds.h; ++v) {
        for (int u=0; u<srcBounds.w; ++u, ++mask_it) {
          ASSERT(mask_it != maskBits.end());

          if (src != dst) {
            if (*mask_it) {
              int getx = u+srcBounds.x-x;
              int gety = v+srcBounds.y-y;

              if ((getx >= 0) && (getx < src->width()) &&
                  (gety >= 0) && (gety < src->height()))
                dst->putPixel(u, v, src->getPixel(getx, gety));
            }
          }
          else {
            if (!*mask_it) {
              dst->putPixel(u, v, dst->maskColor());
            }
          }
        }
      }
    }
    else if (src != dst) {
      copy_image(dst, src, -srcBounds.x, -srcBounds.y);
    }
  }

  return dst.release();
}

} // namespace app
