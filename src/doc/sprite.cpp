// Aseprite Document Library
// Copyright (c) 2001-2014 David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "doc/sprite.h"

#include "base/memory.h"
#include "base/remove_from_container.h"
#include "base/unique_ptr.h"
#include "doc/image_bits.h"
#include "doc/primitives.h"
#include "doc/doc.h"

#include <cstring>
#include <vector>

namespace doc {

static Layer* index2layer(const Layer* layer, const LayerIndex& index, int* index_count);
static LayerIndex layer2index(const Layer* layer, const Layer* find_layer, int* index_count);

//////////////////////////////////////////////////////////////////////
// Constructors/Destructor

Sprite::Sprite(PixelFormat format, int width, int height, int ncolors)
  : Object(ObjectType::Sprite)
  , m_format(format)
  , m_width(width)
  , m_height(height)
  , m_frames(1)
{
  ASSERT(width > 0 && height > 0);

  m_frlens.push_back(100);      // First frame with 100 msecs of duration
  m_stock = new Stock(this, format);
  m_folder = new LayerFolder(this);

  // Generate palette
  Palette pal(FrameNumber(0), ncolors);

  switch (format) {

    // For colored images
    case IMAGE_RGB:
    case IMAGE_INDEXED:
      pal.resize(ncolors);
      break;

    // For black and white images
    case IMAGE_GRAYSCALE:
    case IMAGE_BITMAP:
      for (int c=0; c<ncolors; c++) {
        int g = 255 * c / (ncolors-1);
        g = MID(0, g, 255);
        pal.setEntry(c, rgba(g, g, g, 255));
      }
      break;
  }

  // Initial RGB map
  m_rgbMap = NULL;

  // The transparent color for indexed images is 0 by default
  m_transparentColor = 0;

  setPalette(&pal, true);
}

Sprite::~Sprite()
{
  // Destroy layers
  delete m_folder;

  // Destroy images' stock
  if (m_stock)
    delete m_stock;

  // Destroy palettes
  {
    PalettesList::iterator end = m_palettes.end();
    PalettesList::iterator it = m_palettes.begin();
    for (; it != end; ++it)
      delete *it;               // palette
  }

  // Destroy RGB map
  delete m_rgbMap;
}

// static
Sprite* Sprite::createBasicSprite(doc::PixelFormat format, int width, int height, int ncolors)
{
  // Create the sprite.
  base::UniquePtr<doc::Sprite> sprite(new doc::Sprite(format, width, height, ncolors));
  sprite->setTotalFrames(doc::FrameNumber(1));

  // Create the main image.
  int indexInStock;
  {
    base::UniquePtr<doc::Image> image(doc::Image::create(format, width, height));

    // Clear the image with mask color.
    doc::clear_image(image, 0);

    // Add image in the sprite's stock.
    indexInStock = sprite->stock()->addImage(image);
    image.release();            // Release the image because it is in the sprite's stock.
  }

  // Create the first transparent layer.
  {
    base::UniquePtr<doc::LayerImage> layer(new doc::LayerImage(sprite));
    layer->setName("Layer 1");

    // Create the cel.
    {
      base::UniquePtr<doc::Cel> cel(new doc::Cel(doc::FrameNumber(0), indexInStock));
      cel->setPosition(0, 0);

      // Add the cel in the layer.
      layer->addCel(cel);
      cel.release();            // Release the cel because it is in the layer
    }

    // Add the layer in the sprite.
    sprite->folder()->addLayer(layer.release()); // Release the layer because it's owned by the sprite
  }

  return sprite.release();
}

//////////////////////////////////////////////////////////////////////
// Main properties

void Sprite::setPixelFormat(PixelFormat format)
{
  m_format = format;
}

void Sprite::setSize(int width, int height)
{
  ASSERT(width > 0);
  ASSERT(height > 0);

  m_width = width;
  m_height = height;
}

bool Sprite::needAlpha() const
{
  switch (m_format) {
    case IMAGE_RGB:
    case IMAGE_GRAYSCALE:
      return (backgroundLayer() == NULL);
  }
  return false;
}

bool Sprite::supportAlpha() const
{
  switch (m_format) {
    case IMAGE_RGB:
    case IMAGE_GRAYSCALE:
      return true;
  }
  return false;
}

void Sprite::setTransparentColor(color_t color)
{
  m_transparentColor = color;

  // Change the mask color of all images.
  for (int i=0; i<m_stock->size(); i++) {
    Image* image = m_stock->getImage(i);
    if (image != NULL)
      image->setMaskColor(color);
  }
}

int Sprite::getMemSize() const
{
  int size = 0;

  for (int i=0; i<m_stock->size(); i++) {
    Image* image = m_stock->getImage(i);
    if (image != NULL)
      size += image->getRowStrideSize() * image->height();
  }

  return size;
}

//////////////////////////////////////////////////////////////////////
// Layers

LayerFolder* Sprite::folder() const
{
  return m_folder;
}

LayerImage* Sprite::backgroundLayer() const
{
  if (folder()->getLayersCount() > 0) {
    Layer* bglayer = *folder()->getLayerBegin();

    if (bglayer->isBackground()) {
      ASSERT(bglayer->isImage());
      return static_cast<LayerImage*>(bglayer);
    }
  }
  return NULL;
}

LayerIndex Sprite::countLayers() const
{
  return LayerIndex(folder()->getLayersCount());
}

Layer* Sprite::indexToLayer(LayerIndex index) const
{
  if (index < LayerIndex(0))
    return NULL;

  int index_count = -1;
  return index2layer(folder(), index, &index_count);
}

LayerIndex Sprite::layerToIndex(const Layer* layer) const
{
  int index_count = -1;
  return layer2index(folder(), layer, &index_count);
}

void Sprite::getLayersList(std::vector<Layer*>& layers) const
{
  // TODO support subfolders
  LayerConstIterator it = m_folder->getLayerBegin();
  LayerConstIterator end = m_folder->getLayerEnd();

  for (; it != end; ++it) {
    layers.push_back(*it);
  }
}

//////////////////////////////////////////////////////////////////////
// Palettes

Palette* Sprite::getPalette(FrameNumber frame) const
{
  ASSERT(frame >= 0);

  Palette* found = NULL;

  PalettesList::const_iterator end = m_palettes.end();
  PalettesList::const_iterator it = m_palettes.begin();
  for (; it != end; ++it) {
    Palette* pal = *it;
    if (frame < pal->frame())
      break;

    found = pal;
    if (frame == pal->frame())
      break;
  }

  ASSERT(found != NULL);
  return found;
}

const PalettesList& Sprite::getPalettes() const
{
  return m_palettes;
}

void Sprite::setPalette(const Palette* pal, bool truncate)
{
  ASSERT(pal != NULL);

  if (!truncate) {
    Palette* sprite_pal = getPalette(pal->frame());
    pal->copyColorsTo(sprite_pal);
  }
  else {
    Palette* other;

    PalettesList::iterator end = m_palettes.end();
    PalettesList::iterator it = m_palettes.begin();
    for (; it != end; ++it) {
      other = *it;

      if (pal->frame() == other->frame()) {
        pal->copyColorsTo(other);
        return;
      }
      else if (pal->frame() < other->frame())
        break;
    }

    m_palettes.insert(it, new Palette(*pal));
  }
}

void Sprite::resetPalettes()
{
  PalettesList::iterator end = m_palettes.end();
  PalettesList::iterator it = m_palettes.begin();

  if (it != end) {
    ++it;                       // Leave the first palette only.
    while (it != end) {
      delete *it;               // palette
      it = m_palettes.erase(it);
      end = m_palettes.end();
    }
  }
}

void Sprite::deletePalette(Palette* pal)
{
  ASSERT(pal != NULL);

  base::remove_from_container(m_palettes, pal);
  delete pal;                   // palette
}

RgbMap* Sprite::getRgbMap(FrameNumber frame)
{
  int mask_color = (backgroundLayer() ? -1: transparentColor());

  if (m_rgbMap == NULL) {
    m_rgbMap = new RgbMap();
    m_rgbMap->regenerate(getPalette(frame), mask_color);
  }
  else if (!m_rgbMap->match(getPalette(frame))) {
    m_rgbMap->regenerate(getPalette(frame), mask_color);
  }

  return m_rgbMap;
}

//////////////////////////////////////////////////////////////////////
// Frames

void Sprite::addFrame(FrameNumber newFrame)
{
  setTotalFrames(m_frames.next());
  for (FrameNumber i=m_frames.previous(); i>=newFrame; i=i.previous())
    setFrameDuration(i, getFrameDuration(i.previous()));
}

void Sprite::removeFrame(FrameNumber newFrame)
{
  FrameNumber newTotal = m_frames.previous();
  for (FrameNumber i=newFrame; i<newTotal; i=i.next())
    setFrameDuration(i, getFrameDuration(i.next()));
  setTotalFrames(newTotal);
}

void Sprite::setTotalFrames(FrameNumber frames)
{
  frames = MAX(FrameNumber(1), frames);
  m_frlens.resize(frames);

  if (frames > m_frames) {
    for (FrameNumber c=m_frames; c<frames; ++c)
      m_frlens[c] = m_frlens[m_frames.previous()];
  }

  m_frames = frames;
}

int Sprite::getFrameDuration(FrameNumber frame) const
{
  if (frame >= 0 && frame < m_frames)
    return m_frlens[frame];
  else
    return 0;
}

void Sprite::setFrameDuration(FrameNumber frame, int msecs)
{
  if (frame >= 0 && frame < m_frames)
    m_frlens[frame] = MID(1, msecs, 65535);
}

void Sprite::setFrameRangeDuration(FrameNumber from, FrameNumber to, int msecs)
{
  std::fill(
    m_frlens.begin()+(size_t)from,
    m_frlens.begin()+(size_t)to+1, MID(1, msecs, 65535));
}

void Sprite::setDurationForAllFrames(int msecs)
{
  std::fill(m_frlens.begin(), m_frlens.end(), MID(1, msecs, 65535));
}

//////////////////////////////////////////////////////////////////////
// Images

Stock* Sprite::stock() const
{
  return m_stock;
}

void Sprite::getCels(CelList& cels) const
{
  folder()->getCels(cels);
}

size_t Sprite::getImageRefs(int imageIndex) const
{
  CelList cels;
  getCels(cels);

  size_t refs = 0;
  for (CelList::iterator it=cels.begin(), end=cels.end(); it != end; ++it)
    if ((*it)->imageIndex() == imageIndex)
      ++refs;

  return refs;
}

void Sprite::remapImages(FrameNumber frameFrom, FrameNumber frameTo, const std::vector<uint8_t>& mapping)
{
  ASSERT(m_format == IMAGE_INDEXED);
  ASSERT(mapping.size() == 256);

  CelList cels;
  getCels(cels);

  for (CelIterator it = cels.begin(); it != cels.end(); ++it) {
    Cel* cel = *it;

    // Remap this Cel because is inside the specified range
    if (cel->frame() >= frameFrom &&
        cel->frame() <= frameTo) {
      Image* image = cel->image();
      LockImageBits<IndexedTraits> bits(image);
      LockImageBits<IndexedTraits>::iterator
        it = bits.begin(),
        end = bits.end();

      for (; it != end; ++it)
        *it = mapping[*it];
    }
  }
}

//////////////////////////////////////////////////////////////////////
// Drawing

void Sprite::render(Image* image, int x, int y, FrameNumber frame) const
{
  fill_rect(image, x, y, x+m_width-1, y+m_height-1,
            (m_format == IMAGE_INDEXED ? transparentColor(): 0));

  layer_render(folder(), image, x, y, frame);
}

int Sprite::getPixel(int x, int y, FrameNumber frame) const
{
  int color = 0;

  if ((x >= 0) && (y >= 0) && (x < m_width) && (y < m_height)) {
    base::UniquePtr<Image> image(Image::create(m_format, 1, 1));
    clear_image(image, (m_format == IMAGE_INDEXED ? transparentColor(): 0));
    render(image, -x, -y, frame);
    color = get_pixel(image, 0, 0);
  }

  return color;
}

void Sprite::pickCels(int x, int y, FrameNumber frame, int opacityThreshold, CelList& cels) const
{
  std::vector<Layer*> layers;
  getLayersList(layers);

  for (int i=(int)layers.size()-1; i>=0; --i) {
    Layer* layer = layers[i];
    if (!layer->isImage() || !layer->isVisible())
      continue;

    Cel* cel = static_cast<LayerImage*>(layer)->getCel(frame);
    if (!cel)
      continue;

    Image* image = cel->image();
    if (!image)
      continue;

    if (!cel->bounds().contains(gfx::Point(x, y)))
      continue;

    color_t color = get_pixel(image,
      x - cel->x(),
      y - cel->y());

    bool isOpaque = true;

    switch (image->pixelFormat()) {
      case IMAGE_RGB:
        isOpaque = (rgba_geta(color) >= opacityThreshold);
        break;
      case IMAGE_INDEXED:
        isOpaque = (color != image->maskColor());
        break;
      case IMAGE_GRAYSCALE:
        isOpaque = (graya_geta(color) >= opacityThreshold);
        break;
    }

    if (!isOpaque)
      continue;

    cels.push_back(cel);
  }
  fflush(stdout);
}

//////////////////////////////////////////////////////////////////////

static Layer* index2layer(const Layer* layer, const LayerIndex& index, int* index_count)
{
  if (index == *index_count)
    return (Layer*)layer;
  else {
    (*index_count)++;

    if (layer->isFolder()) {
      Layer *found;

      LayerConstIterator it = static_cast<const LayerFolder*>(layer)->getLayerBegin();
      LayerConstIterator end = static_cast<const LayerFolder*>(layer)->getLayerEnd();

      for (; it != end; ++it) {
        if ((found = index2layer(*it, index, index_count)))
          return found;
      }
    }

    return NULL;
  }
}

static LayerIndex layer2index(const Layer* layer, const Layer* find_layer, int* index_count)
{
  if (layer == find_layer)
    return LayerIndex(*index_count);
  else {
    (*index_count)++;

    if (layer->isFolder()) {
      int found;

      LayerConstIterator it = static_cast<const LayerFolder*>(layer)->getLayerBegin();
      LayerConstIterator end = static_cast<const LayerFolder*>(layer)->getLayerEnd();

      for (; it != end; ++it) {
        if ((found = layer2index(*it, find_layer, index_count)) >= 0)
          return LayerIndex(found);
      }
    }

    return LayerIndex(-1);
  }
}

} // namespace doc
