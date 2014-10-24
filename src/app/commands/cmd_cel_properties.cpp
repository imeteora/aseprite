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

#include "app/app.h"
#include "app/commands/command.h"
#include "app/context_access.h"
#include "app/document_api.h"
#include "app/find_widget.h"
#include "app/load_widget.h"
#include "app/modules/gui.h"
#include "app/undo_transaction.h"
#include "base/mem_utils.h"
#include "doc/cel.h"
#include "doc/image.h"
#include "doc/layer.h"
#include "doc/sprite.h"
#include "doc/stock.h"
#include "ui/ui.h"

namespace app {

using namespace ui;

class CelPropertiesCommand : public Command {
public:
  CelPropertiesCommand();
  Command* clone() const override { return new CelPropertiesCommand(*this); }

protected:
  bool onEnabled(Context* context);
  void onExecute(Context* context);
};

CelPropertiesCommand::CelPropertiesCommand()
  : Command("CelProperties",
            "Cel Properties",
            CmdUIOnlyFlag)
{
}

bool CelPropertiesCommand::onEnabled(Context* context)
{
  return context->checkFlags(ContextFlags::ActiveDocumentIsWritable |
                             ContextFlags::ActiveLayerIsImage);
}

void CelPropertiesCommand::onExecute(Context* context)
{
  const ContextReader reader(context);
  const Sprite* sprite = reader.sprite();
  const Layer* layer = reader.layer();
  const Cel* cel = reader.cel(); // Get current cel (can be NULL)

  base::UniquePtr<Window> window(app::load_widget<Window>("cel_properties.xml", "cel_properties"));
  Widget* label_frame = app::find_widget<Widget>(window, "frame");
  Widget* label_pos = app::find_widget<Widget>(window, "pos");
  Widget* label_size = app::find_widget<Widget>(window, "size");
  Slider* slider_opacity = app::find_widget<Slider>(window, "opacity");
  Widget* button_ok = app::find_widget<Widget>(window, "ok");
  ui::TooltipManager* tooltipManager = window->findFirstChildByType<ui::TooltipManager>();

  // Mini look for the opacity slider
  setup_mini_look(slider_opacity);

  /* if the layer isn't writable */
  if (!layer->isWritable()) {
    button_ok->setText("Locked");
    button_ok->setEnabled(false);
  }

  label_frame->setTextf("%d/%d",
                        (int)reader.frame()+1,
                        (int)sprite->totalFrames());

  if (cel != NULL) {
    // Position
    label_pos->setTextf("%d, %d", cel->x(), cel->y());

    // Dimension (and memory size)
    Image* image = cel->image();
    int memsize = image->getRowStrideSize() * image->height();

    label_size->setTextf("%dx%d (%s)",
      image->width(),
      image->height(),
      base::get_pretty_memory_size(memsize).c_str());

    // Opacity
    slider_opacity->setValue(cel->opacity());
    if (layer->isBackground()) {
      slider_opacity->setEnabled(false);
      tooltipManager->addTooltipFor(slider_opacity,
        "The `Background' layer is opaque,\n"
        "its opacity can't be changed.",
        JI_LEFT);
    }
    else if (sprite->pixelFormat() == IMAGE_INDEXED) {
      slider_opacity->setEnabled(false);
      tooltipManager->addTooltipFor(slider_opacity,
        "Cel opacity of Indexed images\n"
        "cannot be changed.",
        JI_LEFT);
    }
  }
  else {
    label_pos->setText("None");
    label_size->setText("Empty (0 bytes)");
    slider_opacity->setValue(0);
    slider_opacity->setEnabled(false);
  }

  window->openWindowInForeground();

  if (window->getKiller() == button_ok) {
    ContextWriter writer(reader);
    Document* document_writer = writer.document();
    Sprite* sprite_writer = writer.sprite();
    Cel* cel_writer = writer.cel();

    int newOpacity = slider_opacity->getValue();

    // The opacity was changed?
    if (cel_writer != NULL &&
        cel_writer->opacity() != newOpacity) {
      DocumentApi api = document_writer->getApi();
      {
        UndoTransaction undo(writer.context(), "Cel Opacity Change", undo::ModifyDocument);
        api.setCelOpacity(sprite_writer, cel_writer, newOpacity);
        undo.commit();
      }

      update_screen_for_document(document_writer);
    }
  }
}

Command* CommandFactory::createCelPropertiesCommand()
{
  return new CelPropertiesCommand;
}

} // namespace app
