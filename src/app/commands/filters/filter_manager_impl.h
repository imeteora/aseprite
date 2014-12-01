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

#ifndef APP_COMMANDS_FILTERS_FILTER_MANAGER_IMPL_H_INCLUDED
#define APP_COMMANDS_FILTERS_FILTER_MANAGER_IMPL_H_INCLUDED
#pragma once

#include "app/document_location.h"
#include "base/exception.h"
#include "base/unique_ptr.h"
#include "filters/filter_indexed_data.h"
#include "filters/filter_manager.h"
#include "doc/image_bits.h"
#include "doc/image_traits.h"
#include "doc/pixel_format.h"

#include <cstring>

namespace doc {
  class Image;
  class Layer;
  class Mask;
  class Sprite;
}

namespace filters {
  class Filter;
}

namespace app {
  class Context;
  class Document;

  using namespace filters;

  class InvalidAreaException : public base::Exception {
  public:
    InvalidAreaException() throw()
    : base::Exception("The current mask/area to apply the effect is completelly invalid.") { }
  };

  class NoImageException : public base::Exception {
  public:
    NoImageException() throw()
    : base::Exception("There is not an active image to apply the effect.\n"
                      "Please select a layer/cel with an image and try again.") { }
  };

  class FilterManagerImpl : public FilterManager
                          , public FilterIndexedData {
  public:
    // Interface to report progress to the user and take input from him
    // to cancel the whole process.
    class IProgressDelegate {
    public:
      virtual ~IProgressDelegate() { }

      // Called to report the progress of the filter (with progress from 0.0 to 1.0).
      virtual void reportProgress(float progress) = 0;

      // Should return true if the user wants to cancel the filter.
      virtual bool isCancelled() = 0;
    };

    FilterManagerImpl(Context* context, Filter* filter);
    ~FilterManagerImpl();

    void setProgressDelegate(IProgressDelegate* progressDelegate);

    PixelFormat pixelFormat() const;

    void setTarget(Target target);

    void begin();
    void beginForPreview();
    void end();
    bool applyStep();
    void applyToTarget();

    Document* document() { return m_location.document(); }
    Sprite* sprite() { return m_location.sprite(); }
    Layer* layer() { return m_location.layer(); }
    FrameNumber frame() { return m_location.frame(); }
    Image* destinationImage() const { return m_dst; }

    // Updates the current editor to show the progress of the preview.
    void flush();

    // FilterManager implementation
    const void* getSourceAddress();
    void* getDestinationAddress();
    int getWidth() { return m_w; }
    Target getTarget() { return m_target; }
    FilterIndexedData* getIndexedData() { return this; }
    bool skipPixel();
    const Image* getSourceImage() { return m_src; }
    int x() { return m_x; }
    int y() { return m_y+m_row; }

    // FilterIndexedData implementation
    Palette* getPalette();
    RgbMap* getRgbMap();

  private:
    void init(const Layer* layer, Image* image, int offset_x, int offset_y);
    void apply();
    void applyToImage(Layer* layer, Image* image, int x, int y);
    bool updateMask(Mask* mask, const Image* image);

    Context* m_context;
    DocumentLocation m_location;
    Filter* m_filter;
    Image* m_src;
    base::UniquePtr<Image> m_dst;
    int m_row;
    int m_x, m_y, m_w, m_h;
    int m_offset_x, m_offset_y;
    Mask* m_mask;
    base::UniquePtr<Mask> m_preview_mask;
    doc::ImageBits<doc::BitmapTraits> m_maskBits;
    doc::ImageBits<doc::BitmapTraits>::iterator m_maskIterator;
    Target m_targetOrig;          // Original targets
    Target m_target;              // Filtered targets

    // Hooks
    float m_progressBase;
    float m_progressWidth;
    IProgressDelegate* m_progressDelegate;
  };

} // namespace app

#endif
