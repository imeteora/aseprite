/* Aseprite
 * Copyright (C) 2001-2014  David Capello
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/document_api.h"

#include "app/color_target.h"
#include "app/color_utils.h"
#include "app/document.h"
#include "app/document_undo.h"
#include "app/settings/settings.h"
#include "app/undoers/add_cel.h"
#include "app/undoers/add_frame.h"
#include "app/undoers/add_layer.h"
#include "app/undoers/add_palette.h"
#include "app/undoers/dirty_area.h"
#include "app/undoers/flip_image.h"
#include "app/undoers/image_area.h"
#include "app/undoers/move_layer.h"
#include "app/undoers/move_layer.h"
#include "app/undoers/remove_cel.h"
#include "app/undoers/remove_frame.h"
#include "app/undoers/remove_layer.h"
#include "app/undoers/remove_palette.h"
#include "app/undoers/replace_image.h"
#include "app/undoers/set_cel_frame.h"
#include "app/undoers/set_cel_opacity.h"
#include "app/undoers/set_cel_position.h"
#include "app/undoers/set_frame_duration.h"
#include "app/undoers/set_layer_flags.h"
#include "app/undoers/set_layer_flags.h"
#include "app/undoers/set_layer_name.h"
#include "app/undoers/set_layer_name.h"
#include "app/undoers/set_mask.h"
#include "app/undoers/set_mask_position.h"
#include "app/undoers/set_palette_colors.h"
#include "app/undoers/set_sprite_pixel_format.h"
#include "app/undoers/set_sprite_size.h"
#include "app/undoers/set_sprite_transparent_color.h"
#include "app/undoers/set_total_frames.h"
#include "base/unique_ptr.h"
#include "doc/algorithm/flip_image.h"
#include "doc/algorithm/shrink_bounds.h"
#include "doc/blend.h"
#include "doc/cel.h"
#include "doc/context.h"
#include "doc/dirty.h"
#include "doc/document_event.h"
#include "doc/document_observer.h"
#include "doc/image.h"
#include "doc/image_bits.h"
#include "doc/layer.h"
#include "doc/mask.h"
#include "doc/palette.h"
#include "doc/sprite.h"
#include "render/quantization.h"
#include "render/render.h"

namespace app {

DocumentApi::DocumentApi(Document* document, undo::UndoersCollector* undoers)
  : m_document(document)
  , m_undoers(undoers)
{
}

undo::ObjectsContainer* DocumentApi::getObjects() const
{
  return m_document->getUndo()->getObjects();
}

void DocumentApi::setSpriteSize(Sprite* sprite, int w, int h)
{
  ASSERT(w > 0);
  ASSERT(h > 0);

  if (undoEnabled())
    m_undoers->pushUndoer(new undoers::SetSpriteSize(getObjects(), sprite));

  sprite->setSize(w, h);

  doc::DocumentEvent ev(m_document);
  ev.sprite(sprite);
  m_document->notifyObservers<doc::DocumentEvent&>(&doc::DocumentObserver::onSpriteSizeChanged, ev);
}

void DocumentApi::setSpriteTransparentColor(Sprite* sprite, color_t maskColor)
{
  if (undoEnabled())
    m_undoers->pushUndoer(new undoers::SetSpriteTransparentColor(getObjects(), sprite));

  sprite->setTransparentColor(maskColor);

  doc::DocumentEvent ev(m_document);
  ev.sprite(sprite);
  m_document->notifyObservers<doc::DocumentEvent&>(&doc::DocumentObserver::onSpriteTransparentColorChanged, ev);
}

void DocumentApi::cropSprite(Sprite* sprite, const gfx::Rect& bounds)
{
  setSpriteSize(sprite, bounds.w, bounds.h);
  displaceLayers(sprite->folder(), -bounds.x, -bounds.y);

  Layer *background_layer = sprite->backgroundLayer();
  if (background_layer)
    cropLayer(background_layer, 0, 0, sprite->width(), sprite->height());

  if (!m_document->mask()->isEmpty())
    setMaskPosition(m_document->mask()->bounds().x-bounds.x,
                    m_document->mask()->bounds().y-bounds.y);
}

void DocumentApi::trimSprite(Sprite* sprite)
{
  gfx::Rect bounds;

  base::UniquePtr<Image> image_wrap(Image::create(sprite->pixelFormat(),
                                                  sprite->width(),
                                                  sprite->height()));
  Image* image = image_wrap.get();
  render::Render render;

  for (frame_t frame(0); frame<sprite->totalFrames(); ++frame) {
    render.renderSprite(image, sprite, frame);

    // TODO configurable (what color pixel to use as "refpixel",
    // here we are using the top-left pixel by default)
    gfx::Rect frameBounds;
    if (doc::algorithm::shrink_bounds(image, frameBounds, get_pixel(image, 0, 0)))
      bounds = bounds.createUnion(frameBounds);
  }

  if (!bounds.isEmpty())
    cropSprite(sprite, bounds);
}

void DocumentApi::setPixelFormat(Sprite* sprite, PixelFormat newFormat, DitheringMethod dithering_method)
{
  if (sprite->pixelFormat() == newFormat)
    return;

  // TODO Review this, why we use the palette in frame 0?
  frame_t frame(0);

  // Use the rgbmap for the specified sprite
  const RgbMap* rgbmap = sprite->rgbMap(frame);

  // Get the list of cels from the background layer (if it
  // exists). This list will be used to check if each image belong to
  // the background layer.
  CelList bgCels;
  if (sprite->backgroundLayer() != NULL)
    sprite->backgroundLayer()->getCels(bgCels);

  std::vector<Image*> images;
  sprite->getImages(images);
  for (auto& old_image : images) {
    bool is_image_from_background = false;
    for (CelList::iterator it=bgCels.begin(), end=bgCels.end(); it != end; ++it) {
      if ((*it)->image()->id() == old_image->id()) {
        is_image_from_background = true;
        break;
      }
    }

    ImageRef new_image(render::convert_pixel_format
      (old_image, NULL, newFormat, dithering_method, rgbmap,
        sprite->palette(frame),
        is_image_from_background));

    replaceImage(sprite, sprite->getImage(old_image->id()), new_image);
  }

  // Set all cels opacity to 100% if we are converting to indexed.
  if (newFormat == IMAGE_INDEXED) {
    CelList cels;
    sprite->getCels(cels);
    for (CelIterator it = cels.begin(), end = cels.end(); it != end; ++it) {
      Cel* cel = *it;
      if (cel->opacity() < 255)
        setCelOpacity(sprite, *it, 255);
    }
  }

  // Change sprite's pixel format.
  if (undoEnabled())
    m_undoers->pushUndoer(new undoers::SetSpritePixelFormat(getObjects(), sprite));

  sprite->setPixelFormat(newFormat);

  // Regenerate extras
  m_document->destroyExtraCel();

  // When we are converting to grayscale color mode, we've to destroy
  // all palettes and put only one grayscaled-palette at the first
  // frame.
  if (newFormat == IMAGE_GRAYSCALE) {
    // Add undoers to revert all palette changes.
    if (undoEnabled()) {
      PalettesList palettes = sprite->getPalettes();
      for (PalettesList::iterator it = palettes.begin(); it != palettes.end(); ++it) {
        Palette* palette = *it;
        m_undoers->pushUndoer(new undoers::RemovePalette(
            getObjects(), sprite, palette->frame()));
      }

      m_undoers->pushUndoer(new undoers::AddPalette(
        getObjects(), sprite, frame_t(0)));
    }

    // It's a base::UniquePtr because setPalette'll create a copy of "graypal".
    base::UniquePtr<Palette> graypal(Palette::createGrayscale());

    sprite->resetPalettes();
    sprite->setPalette(graypal, true);
  }
}

void DocumentApi::addFrame(Sprite* sprite, frame_t newFrame)
{
  copyFrame(sprite, newFrame-1, newFrame);
}

void DocumentApi::addEmptyFrame(Sprite* sprite, frame_t newFrame)
{
  int duration = sprite->frameDuration(newFrame-1);

  // Add the frame in the sprite structure, it adjusts the total
  // number of frames in the sprite.
  if (undoEnabled())
    m_undoers->pushUndoer(new undoers::AddFrame(getObjects(), m_document, sprite, newFrame));

  sprite->addFrame(newFrame);
  setFrameDuration(sprite, newFrame, duration);

  // Move cels.
  displaceFrames(sprite->folder(), newFrame);

  // Notify observers about the new frame.
  doc::DocumentEvent ev(m_document);
  ev.sprite(sprite);
  ev.frame(newFrame);
  m_document->notifyObservers<doc::DocumentEvent&>(&doc::DocumentObserver::onAddFrame, ev);
}

void DocumentApi::addEmptyFramesTo(Sprite* sprite, frame_t newFrame)
{
  while (sprite->totalFrames() <= newFrame)
    addEmptyFrame(sprite, sprite->totalFrames());
}

void DocumentApi::copyFrame(Sprite* sprite, frame_t fromFrame, frame_t newFrame)
{
  int duration = sprite->frameDuration(fromFrame);

  addEmptyFrame(sprite, newFrame);

  if (fromFrame >= newFrame)
    ++fromFrame;

  copyFrameForLayer(sprite->folder(), fromFrame, newFrame);

  setFrameDuration(sprite, newFrame, duration);
}

void DocumentApi::displaceFrames(Layer* layer, frame_t frame)
{
  ASSERT(layer);
  ASSERT(frame >= 0);

  Sprite* sprite = layer->sprite();

  switch (layer->type()) {

    case ObjectType::LayerImage: {
      LayerImage* imglayer = static_cast<LayerImage*>(layer);

      // Displace all cels in '>=frame' to the next frame.
      for (frame_t c=sprite->lastFrame(); c>=frame; --c) {
        if (Cel* cel = imglayer->cel(c))
          setCelFramePosition(imglayer, cel, cel->frame()+1);
      }

      // Add background cel
      if (imglayer->isBackground()) {
        ImageRef bgimage(Image::create(sprite->pixelFormat(), sprite->width(), sprite->height()));
        clear_image(bgimage, bgColor(layer));
        addCel(imglayer, frame, bgimage);
      }
      break;
    }

    case ObjectType::LayerFolder: {
      LayerIterator it = static_cast<LayerFolder*>(layer)->getLayerBegin();
      LayerIterator end = static_cast<LayerFolder*>(layer)->getLayerEnd();

      for (; it != end; ++it)
        displaceFrames(*it, frame);
      break;
    }

  }
}

void DocumentApi::copyFrameForLayer(Layer* layer, frame_t fromFrame, frame_t frame)
{
  ASSERT(layer);
  ASSERT(frame >= 0);

  switch (layer->type()) {

    case ObjectType::LayerImage: {
      LayerImage* imglayer = static_cast<LayerImage*>(layer);
      copyCel(imglayer, fromFrame, imglayer, frame);
      break;
    }

    case ObjectType::LayerFolder: {
      LayerIterator it = static_cast<LayerFolder*>(layer)->getLayerBegin();
      LayerIterator end = static_cast<LayerFolder*>(layer)->getLayerEnd();

      for (; it != end; ++it)
        copyFrameForLayer(*it, fromFrame, frame);
      break;
    }

  }
}

void DocumentApi::removeFrame(Sprite* sprite, frame_t frame)
{
  ASSERT(frame >= 0);

  // Remove cels from this frame (and displace one position backward
  // all next frames)
  removeFrameOfLayer(sprite->folder(), frame);

  // Add undoers to restore the removed frame from the sprite (to
  // restore the number and durations of frames).
  if (undoEnabled())
    m_undoers->pushUndoer(new undoers::RemoveFrame(getObjects(), m_document, sprite, frame));

  // Remove the frame from the sprite. This is the low level
  // operation, it modifies the number and duration of frames.
  sprite->removeFrame(frame);

  // Notify observers.
  doc::DocumentEvent ev(m_document);
  ev.sprite(sprite);
  ev.frame(frame);
  m_document->notifyObservers<doc::DocumentEvent&>(&doc::DocumentObserver::onRemoveFrame, ev);
}

// Does the hard part of removing a frame: Removes all cels located in
// the given frame, and moves all following cels one frame position back.
void DocumentApi::removeFrameOfLayer(Layer* layer, frame_t frame)
{
  ASSERT(layer);
  ASSERT(frame >= 0);

  Sprite* sprite = layer->sprite();

  switch (layer->type()) {

    case ObjectType::LayerImage: {
      LayerImage* imglayer = static_cast<LayerImage*>(layer);
      if (Cel* cel = imglayer->cel(frame))
        removeCel(cel);

      for (++frame; frame<sprite->totalFrames(); ++frame)
        if (Cel* cel = imglayer->cel(frame))
          setCelFramePosition(imglayer, cel, cel->frame()-1);
      break;
    }

    case ObjectType::LayerFolder: {
      LayerIterator it = static_cast<LayerFolder*>(layer)->getLayerBegin();
      LayerIterator end = static_cast<LayerFolder*>(layer)->getLayerEnd();

      for (; it != end; ++it)
        removeFrameOfLayer(*it, frame);
      break;
    }

  }
}

void DocumentApi::setTotalFrames(Sprite* sprite, frame_t frames)
{
  ASSERT(frames >= 1);

  // Add undoers.
  if (undoEnabled())
    m_undoers->pushUndoer(new undoers::SetTotalFrames(getObjects(), m_document, sprite));

  // Do the action.
  sprite->setTotalFrames(frames);

  // Notify observers.
  doc::DocumentEvent ev(m_document);
  ev.sprite(sprite);
  ev.frame(frames);
  m_document->notifyObservers<doc::DocumentEvent&>(&doc::DocumentObserver::onTotalFramesChanged, ev);
}

void DocumentApi::setFrameDuration(Sprite* sprite, frame_t frame, int msecs)
{
  // Add undoers.
  if (undoEnabled())
    m_undoers->pushUndoer(new undoers::SetFrameDuration(
        getObjects(), sprite, frame));

  // Do the action.
  sprite->setFrameDuration(frame, msecs);

  // Notify observers.
  doc::DocumentEvent ev(m_document);
  ev.sprite(sprite);
  ev.frame(frame);
  m_document->notifyObservers<doc::DocumentEvent&>(&doc::DocumentObserver::onFrameDurationChanged, ev);
}

void DocumentApi::setFrameRangeDuration(Sprite* sprite, frame_t from, frame_t to, int msecs)
{
  ASSERT(from >= frame_t(0));
  ASSERT(from < to);
  ASSERT(to <= sprite->lastFrame());

  // Add undoers.
  if (undoEnabled()) {
    for (frame_t fr(from); fr<=to; ++fr)
      m_undoers->pushUndoer(new undoers::SetFrameDuration(
          getObjects(), sprite, fr));
  }

  // Do the action.
  sprite->setFrameRangeDuration(from, to, msecs);
}

void DocumentApi::moveFrame(Sprite* sprite, frame_t frame, frame_t beforeFrame)
{
  if (frame != beforeFrame &&
      frame >= 0 &&
      frame <= sprite->lastFrame() &&
      beforeFrame >= 0 &&
      beforeFrame <= sprite->lastFrame()+1) {
    // Change the frame-lengths.
    int frlen_aux = sprite->frameDuration(frame);

    // Moving the frame to the future.
    if (frame < beforeFrame) {
      for (frame_t c=frame; c<beforeFrame-1; ++c)
        setFrameDuration(sprite, c, sprite->frameDuration(c+1));

      setFrameDuration(sprite, beforeFrame-1, frlen_aux);
    }
    // Moving the frame to the past.
    else if (beforeFrame < frame) {
      for (frame_t c=frame; c>beforeFrame; --c)
        setFrameDuration(sprite, c, sprite->frameDuration(c-1));

      setFrameDuration(sprite, beforeFrame, frlen_aux);
    }

    // Change cel positions.
    moveFrameLayer(sprite->folder(), frame, beforeFrame);
  }
}

void DocumentApi::moveFrameLayer(Layer* layer, frame_t frame, frame_t beforeFrame)
{
  ASSERT(layer);

  switch (layer->type()) {

    case ObjectType::LayerImage: {
      LayerImage* imglayer = static_cast<LayerImage*>(layer);

      CelList cels;
      imglayer->getCels(cels);

      CelIterator it = cels.begin();
      CelIterator end = cels.end();

      for (; it != end; ++it) {
        Cel* cel = *it;
        frame_t celFrame = cel->frame();
        frame_t newFrame = celFrame;

        // fthe frame to the future
        if (frame < beforeFrame) {
          if (celFrame == frame) {
            newFrame = beforeFrame-1;
          }
          else if (celFrame > frame &&
                   celFrame < beforeFrame) {
            --newFrame;
          }
        }
        // moving the frame to the past
        else if (beforeFrame < frame) {
          if (celFrame == frame) {
            newFrame = beforeFrame;
          }
          else if (celFrame >= beforeFrame &&
                   celFrame < frame) {
            ++newFrame;
          }
        }

        if (celFrame != newFrame)
          setCelFramePosition(imglayer, cel, newFrame);
      }
      break;
    }

    case ObjectType::LayerFolder: {
      LayerIterator it = static_cast<LayerFolder*>(layer)->getLayerBegin();
      LayerIterator end = static_cast<LayerFolder*>(layer)->getLayerEnd();

      for (; it != end; ++it)
        moveFrameLayer(*it, frame, beforeFrame);
      break;
    }

  }
}

void DocumentApi::addCel(LayerImage* layer, Cel* cel)
{
  ASSERT(layer);
  ASSERT(cel);

  if (undoEnabled())
    m_undoers->pushUndoer(new undoers::AddCel(getObjects(), layer, cel));

  layer->addCel(cel);

  doc::DocumentEvent ev(m_document);
  ev.sprite(layer->sprite());
  ev.layer(layer);
  ev.cel(cel);
  m_document->notifyObservers<doc::DocumentEvent&>(&doc::DocumentObserver::onAddCel, ev);
}

void DocumentApi::removeCel(Cel* cel)
{
  ASSERT(cel);

  LayerImage* layer = cel->layer();
  Sprite* sprite = layer->sprite();

  doc::DocumentEvent ev(m_document);
  ev.sprite(sprite);
  ev.layer(layer);
  ev.cel(cel);
  m_document->notifyObservers<doc::DocumentEvent&>(&doc::DocumentObserver::onRemoveCel, ev);

  if (undoEnabled())
    m_undoers->pushUndoer(new undoers::RemoveCel(getObjects(), layer, cel));

  // Remove the cel from the layer.
  layer->removeCel(cel);

  // and here we destroy the cel
  delete cel;
}

void DocumentApi::setCelFramePosition(LayerImage* layer, Cel* cel, frame_t frame)
{
  ASSERT(cel);
  ASSERT(frame >= 0);

  if (undoEnabled())
    m_undoers->pushUndoer(new undoers::SetCelFrame(getObjects(), layer, cel));

  layer->moveCel(cel, frame);

  doc::DocumentEvent ev(m_document);
  ev.sprite(layer->sprite());
  ev.layer(layer);
  ev.cel(cel);
  ev.frame(frame);
  m_document->notifyObservers<doc::DocumentEvent&>(&doc::DocumentObserver::onCelFrameChanged, ev);
}

void DocumentApi::setCelPosition(Sprite* sprite, Cel* cel, int x, int y)
{
  ASSERT(cel);

  if (undoEnabled())
    m_undoers->pushUndoer(new undoers::SetCelPosition(getObjects(), cel));

  cel->setPosition(x, y);

  doc::DocumentEvent ev(m_document);
  ev.sprite(sprite);
  ev.cel(cel);
  m_document->notifyObservers<doc::DocumentEvent&>(&doc::DocumentObserver::onCelPositionChanged, ev);
}

void DocumentApi::setCelOpacity(Sprite* sprite, Cel* cel, int newOpacity)
{
  ASSERT(cel);
  ASSERT(sprite->supportAlpha());

  if (undoEnabled())
    m_undoers->pushUndoer(new undoers::SetCelOpacity(getObjects(), cel));

  cel->setOpacity(newOpacity);

  doc::DocumentEvent ev(m_document);
  ev.sprite(sprite);
  ev.cel(cel);
  m_document->notifyObservers<doc::DocumentEvent&>(&doc::DocumentObserver::onCelOpacityChanged, ev);
}

void DocumentApi::cropCel(Sprite* sprite, Cel* cel, int x, int y, int w, int h)
{
  Image* cel_image = cel->image();
  ASSERT(cel_image);

  // create the new image through a crop
  ImageRef new_image(crop_image(cel_image,
      x-cel->x(), y-cel->y(), w, h, bgColor(cel->layer())));

  // replace the image in the stock that is pointed by the cel
  replaceImage(sprite, cel->imageRef(), new_image);

  // update the cel's position
  setCelPosition(sprite, cel, x, y);
}

void DocumentApi::clearCel(LayerImage* layer, frame_t frame)
{
  if (Cel* cel = layer->cel(frame))
    clearCel(cel);
}

void DocumentApi::clearCel(Cel* cel)
{
  ASSERT(cel);

  Image* image = cel->image();
  ASSERT(image);
  if (!image)
    return;

  if (cel->layer()->isBackground()) {
    ASSERT(image);
    if (image)
      clearImage(image, bgColor(cel->layer()));
  }
  else {
    removeCel(cel);
  }
}

void DocumentApi::moveCel(
  LayerImage* srcLayer, frame_t srcFrame,
  LayerImage* dstLayer, frame_t dstFrame)
{
  ASSERT(srcLayer != NULL);
  ASSERT(dstLayer != NULL);

  Sprite* srcSprite = srcLayer->sprite();
  Sprite* dstSprite = dstLayer->sprite();
  ASSERT(srcSprite != NULL);
  ASSERT(dstSprite != NULL);
  ASSERT(srcFrame >= 0 && srcFrame < srcSprite->totalFrames());
  ASSERT(dstFrame >= 0);
  (void)srcSprite;              // To avoid unused variable warning on Release mode

  clearCel(dstLayer, dstFrame);
  addEmptyFramesTo(dstSprite, dstFrame);

  Cel* srcCel = srcLayer->cel(srcFrame);
  Cel* dstCel = dstLayer->cel(dstFrame);
  Image* srcImage = (srcCel ? srcCel->image(): NULL);
  ImageRef dstImage;
  if (dstCel)
    dstImage = dstCel->imageRef();

  if (srcCel) {
    if (srcLayer == dstLayer) {
      if (dstLayer->isBackground()) {
        ASSERT(dstImage);
        if (dstImage) {
          int blend = (srcLayer->isBackground() ?
            BLEND_MODE_COPY: BLEND_MODE_NORMAL);

          render::composite_image(dstImage, srcImage,
            srcCel->x(), srcCel->y(), 255, blend);
        }

        clearImage(srcImage, bgColor(srcLayer));
      }
      // Move the cel in the same layer.
      else {
        setCelFramePosition(srcLayer, srcCel, dstFrame);
      }
    }
    // Move the cel between different layers.
    else {
      if (!dstCel) {
        dstImage.reset(Image::createCopy(srcImage));

        dstCel = new Cel(*srcCel);
        dstCel->setFrame(dstFrame);
        dstCel->setImage(dstImage);
      }

      if (dstLayer->isBackground()) {
        render::composite_image(dstImage, srcImage,
          srcCel->x(), srcCel->y(), 255, BLEND_MODE_NORMAL);
      }
      else {
        addCel(dstLayer, dstCel);
      }

      clearCel(srcCel);
    }
  }

  m_document->notifyCelMoved(srcLayer, srcFrame, dstLayer, dstFrame);
}

void DocumentApi::copyCel(
  LayerImage* srcLayer, frame_t srcFrame,
  LayerImage* dstLayer, frame_t dstFrame)
{
  ASSERT(srcLayer != NULL);
  ASSERT(dstLayer != NULL);

  Sprite* srcSprite = srcLayer->sprite();
  Sprite* dstSprite = dstLayer->sprite();
  ASSERT(srcSprite != NULL);
  ASSERT(dstSprite != NULL);
  ASSERT(srcFrame >= 0 && srcFrame < srcSprite->totalFrames());
  ASSERT(dstFrame >= 0);
  (void)srcSprite;              // To avoid unused variable warning on Release mode

  clearCel(dstLayer, dstFrame);
  addEmptyFramesTo(dstSprite, dstFrame);

  Cel* srcCel = srcLayer->cel(srcFrame);
  Cel* dstCel = dstLayer->cel(dstFrame);
  Image* srcImage = (srcCel ? srcCel->image(): NULL);
  ImageRef dstImage;
  if (dstCel)
    dstImage = dstCel->imageRef();

  if (dstLayer->isBackground()) {
    if (srcCel) {
      ASSERT(dstImage);
      if (dstImage) {
        int blend = (srcLayer->isBackground() ?
          BLEND_MODE_COPY: BLEND_MODE_NORMAL);

        render::composite_image(dstImage, srcImage,
          srcCel->x(), srcCel->y(), 255, blend);
      }
    }
  }
  else {
    if (dstCel)
      removeCel(dstCel);

    if (srcCel) {
      // Create a new image in the stock
      dstImage.reset(Image::createCopy(srcImage));

      dstCel = new Cel(*srcCel);
      dstCel->setFrame(dstFrame);
      dstCel->setImage(dstImage);

      addCel(dstLayer, dstCel);
    }
  }

  m_document->notifyCelCopied(srcLayer, srcFrame, dstLayer, dstFrame);
}

void DocumentApi::swapCel(
  LayerImage* layer, frame_t frame1, frame_t frame2)
{
  Sprite* sprite = layer->sprite();
  ASSERT(sprite != NULL);
  ASSERT(frame1 >= 0 && frame1 < sprite->totalFrames());
  ASSERT(frame2 >= 0 && frame2 < sprite->totalFrames());
  (void)sprite;              // To avoid unused variable warning on Release mode

  Cel* cel1 = layer->cel(frame1);
  Cel* cel2 = layer->cel(frame2);

  if (cel1) setCelFramePosition(layer, cel1, frame2);
  if (cel2) setCelFramePosition(layer, cel2, frame1);
}

LayerImage* DocumentApi::newLayer(Sprite* sprite)
{
  LayerImage* layer = new LayerImage(sprite);

  addLayer(sprite->folder(), layer,
           sprite->folder()->getLastLayer());

  return layer;
}

LayerFolder* DocumentApi::newLayerFolder(Sprite* sprite)
{
  LayerFolder* layer = new LayerFolder(sprite);

  addLayer(sprite->folder(), layer,
           sprite->folder()->getLastLayer());

  return layer;
}

void DocumentApi::addLayer(LayerFolder* folder, Layer* newLayer, Layer* afterThis)
{
  // Add undoers.
  if (undoEnabled())
    m_undoers->pushUndoer(new undoers::AddLayer(getObjects(),
                                                m_document, newLayer));

  // Do the action.
  folder->addLayer(newLayer);
  folder->stackLayer(newLayer, afterThis);

  // Notify observers.
  doc::DocumentEvent ev(m_document);
  ev.sprite(folder->sprite());
  ev.layer(newLayer);
  m_document->notifyObservers<doc::DocumentEvent&>(&doc::DocumentObserver::onAddLayer, ev);
}

void DocumentApi::removeLayer(Layer* layer)
{
  ASSERT(layer != NULL);

  // Notify observers that a layer will be removed (e.g. an Editor can
  // select another layer if the removed layer is the active one).
  doc::DocumentEvent ev(m_document);
  ev.sprite(layer->sprite());
  ev.layer(layer);
  m_document->notifyObservers<doc::DocumentEvent&>(&doc::DocumentObserver::onBeforeRemoveLayer, ev);

  // Add undoers.
  if (undoEnabled())
    m_undoers->pushUndoer(new undoers::RemoveLayer(getObjects(), m_document, layer));

  // Do the action.
  layer->parent()->removeLayer(layer);

  m_document->notifyObservers<doc::DocumentEvent&>(&doc::DocumentObserver::onAfterRemoveLayer, ev);

  delete layer;
}

void DocumentApi::configureLayerAsBackground(LayerImage* layer)
{
  // Add undoers.
  if (undoEnabled()) {
    m_undoers->pushUndoer(new undoers::SetLayerFlags(getObjects(), layer));
    m_undoers->pushUndoer(new undoers::SetLayerName(getObjects(), layer));
    m_undoers->pushUndoer(new undoers::MoveLayer(getObjects(), layer));
  }

  // Do the action.
  layer->configureAsBackground();
}

void DocumentApi::restackLayerAfter(Layer* layer, Layer* afterThis)
{
  if (undoEnabled())
    m_undoers->pushUndoer(new undoers::MoveLayer(getObjects(), layer));

  layer->parent()->stackLayer(layer, afterThis);

  doc::DocumentEvent ev(m_document);
  ev.sprite(layer->sprite());
  ev.layer(layer);
  m_document->notifyObservers<doc::DocumentEvent&>(&doc::DocumentObserver::onLayerRestacked, ev);
}

void DocumentApi::restackLayerBefore(Layer* layer, Layer* beforeThis)
{
  LayerIndex beforeThisIdx = layer->sprite()->layerToIndex(beforeThis);
  LayerIndex afterThisIdx = beforeThisIdx.previous();

  restackLayerAfter(layer, layer->sprite()->indexToLayer(afterThisIdx));
}

void DocumentApi::cropLayer(Layer* layer, int x, int y, int w, int h)
{
  if (!layer->isImage())
    return;

  Sprite* sprite = layer->sprite();
  CelIterator it = ((LayerImage*)layer)->getCelBegin();
  CelIterator end = ((LayerImage*)layer)->getCelEnd();
  for (; it != end; ++it)
    cropCel(sprite, *it, x, y, w, h);
}

// Moves every frame in @a layer with the offset (@a dx, @a dy).
void DocumentApi::displaceLayers(Layer* layer, int dx, int dy)
{
  switch (layer->type()) {

    case ObjectType::LayerImage: {
      CelIterator it = ((LayerImage*)layer)->getCelBegin();
      CelIterator end = ((LayerImage*)layer)->getCelEnd();
      for (; it != end; ++it) {
        Cel* cel = *it;
        setCelPosition(layer->sprite(), cel, cel->x()+dx, cel->y()+dy);
      }
      break;
    }

    case ObjectType::LayerFolder: {
      LayerIterator it = ((LayerFolder*)layer)->getLayerBegin();
      LayerIterator end = ((LayerFolder*)layer)->getLayerEnd();
      for (; it != end; ++it)
        displaceLayers(*it, dx, dy);
      break;
    }

  }
}

void DocumentApi::backgroundFromLayer(LayerImage* layer)
{
  ASSERT(layer);
  ASSERT(layer->isImage());
  ASSERT(layer->isVisible());
  ASSERT(layer->isEditable());
  ASSERT(layer->sprite() != NULL);
  ASSERT(layer->sprite()->backgroundLayer() == NULL);

  Sprite* sprite = layer->sprite();
  color_t bgcolor = bgColor();

  // create a temporary image to draw each frame of the new
  // `Background' layer
  ImageRef bg_image(Image::create(sprite->pixelFormat(),
      sprite->width(),
      sprite->height()));

  CelIterator it = layer->getCelBegin();
  CelIterator end = layer->getCelEnd();

  for (; it != end; ++it) {
    Cel* cel = *it;

    // get the image from the sprite's stock of images
    Image* cel_image = cel->image();
    ASSERT(cel_image);

    clear_image(bg_image, bgcolor);
    render::composite_image(bg_image, cel_image,
      cel->x(), cel->y(),
      MID(0, cel->opacity(), 255),
      layer->getBlendMode());

    // now we have to copy the new image (bg_image) to the cel...
    setCelPosition(sprite, cel, 0, 0);

    // same size of cel-image and bg-image
    if (bg_image->width() == cel_image->width() &&
        bg_image->height() == cel_image->height()) {
      if (undoEnabled())
        m_undoers->pushUndoer(new undoers::ImageArea(getObjects(),
          cel_image, 0, 0, cel_image->width(), cel_image->height()));

      copy_image(cel_image, bg_image, 0, 0);
    }
    else {
      ImageRef bg_image2(Image::createCopy(bg_image));
      replaceImage(sprite, cel->imageRef(), bg_image2);
    }
  }

  // Fill all empty cels with a flat-image filled with bgcolor
  for (frame_t frame(0); frame<sprite->totalFrames(); ++frame) {
    Cel* cel = layer->cel(frame);
    if (!cel) {
      ImageRef cel_image(Image::create(sprite->pixelFormat(),
          sprite->width(), sprite->height()));
      clear_image(cel_image, bgcolor);

      // Create the new cel and add it to the new background layer
      cel = new Cel(frame, cel_image);
      addCel(layer, cel);
    }
  }

  configureLayerAsBackground(layer);
}

void DocumentApi::layerFromBackground(Layer* layer)
{
  ASSERT(layer != NULL);
  ASSERT(layer->isImage());
  ASSERT(layer->isVisible());
  ASSERT(layer->isEditable());
  ASSERT(layer->isBackground());
  ASSERT(layer->sprite() != NULL);
  ASSERT(layer->sprite()->backgroundLayer() != NULL);

  if (undoEnabled()) {
    m_undoers->pushUndoer(new undoers::SetLayerFlags(getObjects(), layer));
    m_undoers->pushUndoer(new undoers::SetLayerName(getObjects(), layer));
  }

  layer->setBackground(false);
  layer->setMovable(true);
  layer->setName("Layer 0");
}

void DocumentApi::flattenLayers(Sprite* sprite)
{
  ImageRef cel_image;
  Cel* cel;

  // Create a temporary image.
  base::UniquePtr<Image> image_wrap(Image::create(sprite->pixelFormat(),
                                            sprite->width(),
                                            sprite->height()));
  Image* image = image_wrap.get();

  // Get the background layer from the sprite.
  LayerImage* background = sprite->backgroundLayer();
  if (!background) {
    // If there aren't a background layer we must to create the background.
    background = new LayerImage(sprite);

    addLayer(sprite->folder(), background, NULL);
    configureLayerAsBackground(background);
  }

  render::Render render;
  render.setBgType(render::BgType::NONE);
  color_t bgcolor = bgColor(background);

  // Copy all frames to the background.
  for (frame_t frame(0); frame<sprite->totalFrames(); ++frame) {
    // Clear the image and render this frame.
    clear_image(image, bgcolor);
    render.renderSprite(image, sprite, frame);

    cel = background->cel(frame);
    if (cel) {
      cel_image = cel->imageRef();
      ASSERT(cel_image != NULL);

      // We have to save the current state of `cel_image' in the undo.
      if (undoEnabled()) {
        Dirty dirty(cel_image, image, image->bounds());
        dirty.saveImagePixels(cel_image);
        m_undoers->pushUndoer(new undoers::DirtyArea(
            getObjects(), cel_image, &dirty));
      }
    }
    else {
      // If there aren't a cel in this frame in the background, we
      // have to create a copy of the image for the new cel.
      cel_image.reset(Image::createCopy(image));
      // TODO error handling: if createCopy throws

      // Here we create the new cel (with the new image `cel_image').
      cel = new Cel(frame, cel_image);
      // TODO error handling: if new Cel throws

      // And finally we add the cel in the background.
      background->addCel(cel);
    }

    copy_image(cel_image, image, 0, 0);
  }

  // Delete old layers.
  LayerList layers = sprite->folder()->getLayersList();
  LayerIterator it = layers.begin();
  LayerIterator end = layers.end();
  for (; it != end; ++it)
    if (*it != background)
      removeLayer(*it);
}

void DocumentApi::duplicateLayerAfter(Layer* sourceLayer, Layer* afterLayer)
{
  base::UniquePtr<LayerImage> newLayerPtr(new LayerImage(sourceLayer->sprite()));

  m_document->copyLayerContent(sourceLayer, m_document, newLayerPtr);

  newLayerPtr->setName(newLayerPtr->name() + " Copy");

  addLayer(sourceLayer->parent(), newLayerPtr, afterLayer);

  // Release the pointer as it is owned by the sprite now.
  newLayerPtr.release();
}

void DocumentApi::duplicateLayerBefore(Layer* sourceLayer, Layer* beforeLayer)
{
  LayerIndex beforeThisIdx = sourceLayer->sprite()->layerToIndex(beforeLayer);
  LayerIndex afterThisIdx = beforeThisIdx.previous();

  duplicateLayerAfter(sourceLayer, sourceLayer->sprite()->indexToLayer(afterThisIdx));
}

Cel* DocumentApi::addCel(LayerImage* layer, frame_t frameNumber, const ImageRef& image)
{
  ASSERT(layer->cel(frameNumber) == NULL);

  base::UniquePtr<Cel> cel(new Cel(frameNumber, image));

  addCel(layer, cel);
  cel.release();

  return cel;
}

void DocumentApi::replaceImage(Sprite* sprite, const ImageRef& oldImage, const ImageRef& newImage)
{
  if (undoEnabled())
    m_undoers->pushUndoer(new undoers::ReplaceImage(getObjects(),
        sprite, oldImage, newImage));

  sprite->replaceImage(oldImage->id(), newImage);
}

void DocumentApi::clearImage(Image* image, color_t bgcolor)
{
  if (undoEnabled())
    m_undoers->pushUndoer(new undoers::ImageArea(getObjects(),
        image, 0, 0, image->width(), image->height()));

  // clear all
  clear_image(image, bgcolor);
}

// Clears the mask region in the current sprite with the specified background color.
void DocumentApi::clearMask(Cel* cel)
{
  ASSERT(cel);

  // If the mask is empty or is not visible then we have to clear the
  // entire image in the cel.
  if (!m_document->isMaskVisible()) {
    clearCel(cel);
    return;
  }

  Image* image = (cel ? cel->image(): NULL);
  if (!image)
    return;

  Mask* mask = m_document->mask();
  color_t bgcolor = bgColor(cel->layer());
  int offset_x = mask->bounds().x-cel->x();
  int offset_y = mask->bounds().y-cel->y();
  int u, v, putx, puty;
  int x1 = MAX(0, offset_x);
  int y1 = MAX(0, offset_y);
  int x2 = MIN(image->width()-1, offset_x+mask->bounds().w-1);
  int y2 = MIN(image->height()-1, offset_y+mask->bounds().h-1);

  // Do nothing
  if (x1 > x2 || y1 > y2)
    return;

  if (undoEnabled())
    m_undoers->pushUndoer(new undoers::ImageArea(getObjects(),
        image, x1, y1, x2-x1+1, y2-y1+1));

  const LockImageBits<BitmapTraits> maskBits(mask->bitmap());
  LockImageBits<BitmapTraits>::const_iterator it = maskBits.begin();

  // Clear the masked zones
  for (v=0; v<mask->bounds().h; ++v) {
    for (u=0; u<mask->bounds().w; ++u, ++it) {
      ASSERT(it != maskBits.end());
      if (*it) {
        putx = u + offset_x;
        puty = v + offset_y;
        put_pixel(image, putx, puty, bgcolor);
      }
    }
  }

  ASSERT(it == maskBits.end());
}

void DocumentApi::flipImage(Image* image, const gfx::Rect& bounds,
  doc::algorithm::FlipType flipType)
{
  // Insert the undo operation.
  if (undoEnabled()) {
    m_undoers->pushUndoer
      (new undoers::FlipImage
       (getObjects(), image, bounds, flipType));
  }

  // Flip the portion of the bitmap.
  doc::algorithm::flip_image(image, bounds, flipType);
}

void DocumentApi::flipImageWithMask(Layer* layer, Image* image, const Mask* mask, doc::algorithm::FlipType flipType)
{
  base::UniquePtr<Image> flippedImage((Image::createCopy(image)));
  color_t bgcolor = bgColor(layer);

  // Flip the portion of the bitmap.
  doc::algorithm::flip_image_with_mask(flippedImage, mask, flipType, bgcolor);

  // Insert the undo operation.
  if (undoEnabled()) {
    Dirty dirty(image, flippedImage, image->bounds());
    dirty.saveImagePixels(image);
    m_undoers->pushUndoer(new undoers::DirtyArea(getObjects(), image, &dirty));
  }

  // Copy the flipped image into the image specified as argument.
  copy_image(image, flippedImage, 0, 0);
}

void DocumentApi::copyToCurrentMask(Mask* mask)
{
  ASSERT(m_document->mask());
  ASSERT(mask);

  if (undoEnabled())
    m_undoers->pushUndoer(new undoers::SetMask(getObjects(),
        m_document));

  m_document->mask()->copyFrom(mask);
}

void DocumentApi::setMaskPosition(int x, int y)
{
  ASSERT(m_document->mask());

  if (undoEnabled())
    m_undoers->pushUndoer(new undoers::SetMaskPosition(getObjects(), m_document));

  m_document->mask()->setOrigin(x, y);
  m_document->resetTransformation();
}

void DocumentApi::deselectMask()
{
  if (undoEnabled())
    m_undoers->pushUndoer(new undoers::SetMask(getObjects(),
        m_document));

  m_document->setMaskVisible(false);
}

void DocumentApi::setPalette(Sprite* sprite, frame_t frame, Palette* newPalette)
{
  Palette* currentSpritePalette = sprite->palette(frame); // Sprite current pal
  int from, to;

  // Check differences between current sprite palette and current system palette
  from = to = -1;
  currentSpritePalette->countDiff(newPalette, &from, &to);

  if (from >= 0 && to >= from) {
    // Add undo information to save the range of pal entries that will be modified.
    if (undoEnabled()) {
      m_undoers->pushUndoer
        (new undoers::SetPaletteColors(getObjects(),
                                       sprite, currentSpritePalette,
                                       frame, from, to));
    }

    // Change the sprite palette
    sprite->setPalette(newPalette, false);
  }
}

bool DocumentApi::undoEnabled()
{
  return
    m_undoers != NULL &&
    m_document->getUndo()->isEnabled();
}

doc::color_t DocumentApi::bgColor()
{
  app::ISettings* appSettings =
    dynamic_cast<app::ISettings*>(m_document->context()->settings());

  return color_utils::color_for_target(
    appSettings ? appSettings->getBgColor(): Color::fromMask(),
    ColorTarget(ColorTarget::BackgroundLayer,
      m_document->sprite()->pixelFormat(),
      m_document->sprite()->transparentColor()));
}

doc::color_t DocumentApi::bgColor(Layer* layer)
{
  app::ISettings* appSettings =
    dynamic_cast<app::ISettings*>(m_document->context()->settings());

  if (layer->isBackground())
    return color_utils::color_for_layer(
      appSettings ? appSettings->getBgColor(): Color::fromMask(),
      layer);
  else
    return layer->sprite()->transparentColor();
}

} // namespace app
