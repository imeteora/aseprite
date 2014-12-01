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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/commands/cmd_sprite_size.h"
#include "app/commands/command.h"
#include "app/commands/params.h"
#include "app/context_access.h"
#include "app/document_api.h"
#include "app/find_widget.h"
#include "app/ini_file.h"
#include "app/job.h"
#include "app/load_widget.h"
#include "app/modules/gui.h"
#include "app/modules/palettes.h"
#include "app/ui_context.h"
#include "app/undo_transaction.h"
#include "base/bind.h"
#include "base/unique_ptr.h"
#include "doc/algorithm/resize_image.h"
#include "doc/cel.h"
#include "doc/image.h"
#include "doc/mask.h"
#include "doc/primitives.h"
#include "doc/sprite.h"
#include "doc/stock.h"
#include "ui/ui.h"

#define PERC_FORMAT     "%.1f"

namespace app {

using namespace ui;
using doc::algorithm::ResizeMethod;

class SpriteSizeJob : public Job {
  ContextWriter m_writer;
  Document* m_document;
  Sprite* m_sprite;
  int m_new_width;
  int m_new_height;
  ResizeMethod m_resize_method;

  inline int scale_x(int x) const { return x * m_new_width / m_sprite->width(); }
  inline int scale_y(int y) const { return y * m_new_height / m_sprite->height(); }

public:

  SpriteSizeJob(const ContextReader& reader, int new_width, int new_height, ResizeMethod resize_method)
    : Job("Sprite Size")
    , m_writer(reader)
    , m_document(m_writer.document())
    , m_sprite(m_writer.sprite())
  {
    m_new_width = new_width;
    m_new_height = new_height;
    m_resize_method = resize_method;
  }

protected:

  /**
   * [working thread]
   */
  virtual void onJob()
  {
    UndoTransaction undoTransaction(m_writer.context(), "Sprite Size");
    DocumentApi api = m_writer.document()->getApi();

    // Get all sprite cels
    CelList cels;
    m_sprite->getCels(cels);

    // For each cel...
    int progress = 0;
    for (CelIterator it = cels.begin(); it != cels.end(); ++it, ++progress) {
      Cel* cel = *it;

      // Change its location
      api.setCelPosition(m_sprite, cel, scale_x(cel->x()), scale_y(cel->y()));

      // Get cel's image
      Image* image = cel->image();
      if (!image)
        continue;

      // Resize the image
      int w = scale_x(image->width());
      int h = scale_y(image->height());
      Image* new_image = Image::create(image->pixelFormat(), MAX(1, w), MAX(1, h));

      doc::algorithm::fixup_image_transparent_colors(image);
      doc::algorithm::resize_image(image, new_image,
                                      m_resize_method,
                                      m_sprite->getPalette(cel->frame()),
                                      m_sprite->getRgbMap(cel->frame()));

      api.replaceStockImage(m_sprite, cel->imageIndex(), new_image);

      jobProgress((float)progress / cels.size());

      // cancel all the operation?
      if (isCanceled())
        return;        // UndoTransaction destructor will undo all operations
    }

    // Resize mask
    if (m_document->isMaskVisible()) {
      base::UniquePtr<Image> old_bitmap
        (crop_image(m_document->mask()->bitmap(), -1, -1,
                    m_document->mask()->bitmap()->width()+2,
                    m_document->mask()->bitmap()->height()+2, 0));

      int w = scale_x(old_bitmap->width());
      int h = scale_y(old_bitmap->height());
      base::UniquePtr<Mask> new_mask(new Mask);
      new_mask->replace(scale_x(m_document->mask()->bounds().x-1),
                        scale_y(m_document->mask()->bounds().y-1), MAX(1, w), MAX(1, h));
      algorithm::resize_image(old_bitmap, new_mask->bitmap(),
                              m_resize_method,
                              m_sprite->getPalette(FrameNumber(0)), // Ignored
                              m_sprite->getRgbMap(FrameNumber(0))); // Ignored

      // Reshrink
      new_mask->intersect(new_mask->bounds());

      // Copy new mask
      api.copyToCurrentMask(new_mask);

      // Regenerate mask
      m_document->resetTransformation();
      m_document->generateMaskBoundaries();
    }

    // resize sprite
    api.setSpriteSize(m_sprite, m_new_width, m_new_height);

    // commit changes
    undoTransaction.commit();
  }

};

SpriteSizeCommand::SpriteSizeCommand()
  : Command("SpriteSize",
            "Sprite Size",
            CmdRecordableFlag)
{
  m_width = 0;
  m_height = 0;
  m_scaleX = 1.0;
  m_scaleY = 1.0;
  m_resizeMethod = doc::algorithm::RESIZE_METHOD_NEAREST_NEIGHBOR;
}

Command* SpriteSizeCommand::clone() const
{
  return new SpriteSizeCommand(*this);
}

void SpriteSizeCommand::onLoadParams(Params* params)
{
  std::string width = params->get("width");
  if (!width.empty()) {
    m_width = std::strtol(width.c_str(), NULL, 10);
  }
  else
    m_width = 0;

  std::string height = params->get("height");
  if (!height.empty()) {
    m_height = std::strtol(height.c_str(), NULL, 10);
  }
  else
    m_height = 0;

  std::string resize_method = params->get("resize-method");
  if (!resize_method.empty()) {
    if (resize_method == "bilinear")
      m_resizeMethod = doc::algorithm::RESIZE_METHOD_BILINEAR;
    else
      m_resizeMethod = doc::algorithm::RESIZE_METHOD_NEAREST_NEIGHBOR;
  }
  else
    m_resizeMethod = doc::algorithm::RESIZE_METHOD_NEAREST_NEIGHBOR;
}

bool SpriteSizeCommand::onEnabled(Context* context)
{
  return context->checkFlags(ContextFlags::ActiveDocumentIsWritable |
                             ContextFlags::HasActiveSprite);
}

void SpriteSizeCommand::onExecute(Context* context)
{
  const ContextReader reader(context);
  const Sprite* sprite(reader.sprite());
  int new_width = (m_width ? m_width: sprite->width()*m_scaleX);
  int new_height = (m_height ? m_height: sprite->height()*m_scaleY);
  ResizeMethod resize_method = m_resizeMethod;

  if (context->isUiAvailable()) {
    // load the window widget
    base::UniquePtr<Window> window(app::load_widget<Window>("sprite_size.xml", "sprite_size"));
    m_widthPx = app::find_widget<Entry>(window, "width_px");
    m_heightPx = app::find_widget<Entry>(window, "height_px");
    m_widthPerc = app::find_widget<Entry>(window, "width_perc");
    m_heightPerc = app::find_widget<Entry>(window, "height_perc");
    m_lockRatio = app::find_widget<CheckBox>(window, "lock_ratio");
    ComboBox* method = app::find_widget<ComboBox>(window, "method");
    Widget* ok = app::find_widget<Widget>(window, "ok");

    m_widthPx->setTextf("%d", new_width);
    m_heightPx->setTextf("%d", new_height);

    m_lockRatio->Click.connect(Bind<void>(&SpriteSizeCommand::onLockRatioClick, this));
    m_widthPx->EntryChange.connect(Bind<void>(&SpriteSizeCommand::onWidthPxChange, this));
    m_heightPx->EntryChange.connect(Bind<void>(&SpriteSizeCommand::onHeightPxChange, this));
    m_widthPerc->EntryChange.connect(Bind<void>(&SpriteSizeCommand::onWidthPercChange, this));
    m_heightPerc->EntryChange.connect(Bind<void>(&SpriteSizeCommand::onHeightPercChange, this));

    method->addItem("Nearest-neighbor");
    method->addItem("Bilinear");
    method->setSelectedItemIndex(get_config_int("SpriteSize", "Method",
        doc::algorithm::RESIZE_METHOD_NEAREST_NEIGHBOR));

    window->remapWindow();
    window->centerWindow();

    load_window_pos(window, "SpriteSize");
    window->setVisible(true);
    window->openWindowInForeground();
    save_window_pos(window, "SpriteSize");

    if (window->getKiller() != ok)
      return;

    new_width = m_widthPx->getTextInt();
    new_height = m_heightPx->getTextInt();
    resize_method = (ResizeMethod)method->getSelectedItemIndex();

    set_config_int("SpriteSize", "Method", resize_method);
  }

  {
    SpriteSizeJob job(reader, new_width, new_height, resize_method);
    job.startJob();
    job.waitJob();
  }

  ContextWriter writer(reader);
  update_screen_for_document(writer.document());
}

void SpriteSizeCommand::onLockRatioClick()
{
  const ContextReader reader(UIContext::instance()); // TODO use the context in sprite size command

  onWidthPxChange();
}

void SpriteSizeCommand::onWidthPxChange()
{
  const ContextReader reader(UIContext::instance()); // TODO use the context in sprite size command
  const Sprite* sprite(reader.sprite());
  int width = m_widthPx->getTextInt();
  double perc = 100.0 * width / sprite->width();

  m_widthPerc->setTextf(PERC_FORMAT, perc);

  if (m_lockRatio->isSelected()) {
    m_heightPerc->setTextf(PERC_FORMAT, perc);
    m_heightPx->setTextf("%d", sprite->height() * width / sprite->width());
  }
}

void SpriteSizeCommand::onHeightPxChange()
{
  const ContextReader reader(UIContext::instance()); // TODO use the context in sprite size command
  const Sprite* sprite(reader.sprite());
  int height = m_heightPx->getTextInt();
  double perc = 100.0 * height / sprite->height();

  m_heightPerc->setTextf(PERC_FORMAT, perc);

  if (m_lockRatio->isSelected()) {
    m_widthPerc->setTextf(PERC_FORMAT, perc);
    m_widthPx->setTextf("%d", sprite->width() * height / sprite->height());
  }
}

void SpriteSizeCommand::onWidthPercChange()
{
  const ContextReader reader(UIContext::instance()); // TODO use the context in sprite size command
  const Sprite* sprite(reader.sprite());
  double width = m_widthPerc->getTextDouble();

  m_widthPx->setTextf("%d", (int)(sprite->width() * width / 100));

  if (m_lockRatio->isSelected()) {
    m_heightPx->setTextf("%d", (int)(sprite->height() * width / 100));
    m_heightPerc->setText(m_widthPerc->getText());
  }
}

void SpriteSizeCommand::onHeightPercChange()
{
  const ContextReader reader(UIContext::instance()); // TODO use the context in sprite size command
  const Sprite* sprite(reader.sprite());
  double height = m_heightPerc->getTextDouble();

  m_heightPx->setTextf("%d", (int)(sprite->height() * height / 100));

  if (m_lockRatio->isSelected()) {
    m_widthPx->setTextf("%d", (int)(sprite->width() * height / 100));
    m_widthPerc->setText(m_heightPerc->getText());
  }
}

Command* CommandFactory::createSpriteSizeCommand()
{
  return new SpriteSizeCommand;
}

} // namespace app
