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

#include "app/app.h"
#include "app/color.h"
#include "app/color_utils.h"
#include "app/commands/command.h"
#include "app/context.h"
#include "app/context_access.h"
#include "app/document.h"
#include "app/ini_file.h"
#include "app/modules/editors.h"
#include "app/modules/gui.h"
#include "app/ui/color_bar.h"
#include "app/ui/color_button.h"
#include "app/undo_transaction.h"
#include "app/undoers/set_mask.h"
#include "app/util/misc.h"
#include "base/bind.h"
#include "base/unique_ptr.h"
#include "doc/image.h"
#include "doc/mask.h"
#include "doc/sprite.h"
#include "ui/box.h"
#include "ui/button.h"
#include "ui/label.h"
#include "ui/slider.h"
#include "ui/widget.h"
#include "ui/window.h"

namespace app {

using namespace ui;

class MaskByColorCommand : public Command {
public:
  MaskByColorCommand();
  Command* clone() const override { return new MaskByColorCommand(*this); }

protected:
  bool onEnabled(Context* context);
  void onExecute(Context* context);

private:
  Mask* generateMask(const Sprite* sprite, const Image* image, int xpos, int ypos);
  void maskPreview(const ContextReader& reader);

  ColorButton* m_buttonColor;
  CheckBox* m_checkPreview;
  Slider* m_sliderTolerance;
};

MaskByColorCommand::MaskByColorCommand()
  : Command("MaskByColor",
            "Mask By Color",
            CmdUIOnlyFlag)
{
}

bool MaskByColorCommand::onEnabled(Context* context)
{
  return context->checkFlags(ContextFlags::ActiveDocumentIsWritable |
                             ContextFlags::HasActiveSprite);
}

void MaskByColorCommand::onExecute(Context* context)
{
  const ContextReader reader(context);
  const Sprite* sprite = reader.sprite();
  Box* box1, *box2, *box3, *box4;
  Widget* label_color;
  Widget* label_tolerance;
  Button* button_ok;
  Button* button_cancel;

  if (!App::instance()->isGui() || !sprite)
    return;

  int xpos, ypos;
  const Image* image = reader.image(&xpos, &ypos);
  if (!image)
    return;

  base::UniquePtr<Window> window(new Window(Window::WithTitleBar, "Mask by Color"));
  box1 = new Box(JI_VERTICAL);
  box2 = new Box(JI_HORIZONTAL);
  box3 = new Box(JI_HORIZONTAL);
  box4 = new Box(JI_HORIZONTAL | JI_HOMOGENEOUS);
  label_color = new Label("Color:");
  m_buttonColor = new ColorButton
   (get_config_color("MaskColor", "Color",
                     ColorBar::instance()->getFgColor()),
    sprite->pixelFormat());
  label_tolerance = new Label("Tolerance:");
  m_sliderTolerance = new Slider(0, 255, get_config_int("MaskColor", "Tolerance", 0));
  m_checkPreview = new CheckBox("&Preview");
  button_ok = new Button("&OK");
  button_cancel = new Button("&Cancel");

  if (get_config_bool("MaskColor", "Preview", true))
    m_checkPreview->setSelected(true);

  button_ok->Click.connect(Bind<void>(&Window::closeWindow, window.get(), button_ok));
  button_cancel->Click.connect(Bind<void>(&Window::closeWindow, window.get(), button_cancel));

  m_buttonColor->Change.connect(Bind<void>(&MaskByColorCommand::maskPreview, this, Ref(reader)));
  m_sliderTolerance->Change.connect(Bind<void>(&MaskByColorCommand::maskPreview, this, Ref(reader)));
  m_checkPreview->Click.connect(Bind<void>(&MaskByColorCommand::maskPreview, this, Ref(reader)));

  button_ok->setFocusMagnet(true);
  m_buttonColor->setExpansive(true);
  m_sliderTolerance->setExpansive(true);
  box2->setExpansive(true);

  window->addChild(box1);
  box1->addChild(box2);
  box1->addChild(box3);
  box1->addChild(m_checkPreview);
  box1->addChild(box4);
  box2->addChild(label_color);
  box2->addChild(m_buttonColor);
  box3->addChild(label_tolerance);
  box3->addChild(m_sliderTolerance);
  box4->addChild(button_ok);
  box4->addChild(button_cancel);

  // Default position
  window->remapWindow();
  window->centerWindow();

  // Mask first preview
  maskPreview(reader);

  // Load window configuration
  load_window_pos(window, "MaskColor");

  // Open the window
  window->openWindowInForeground();

  bool apply = (window->getKiller() == button_ok);

  ContextWriter writer(reader);
  Document* document(writer.document());

  if (apply) {
    UndoTransaction undo(writer.context(), "Mask by Color", undo::DoesntModifyDocument);

    if (undo.isEnabled())
      undo.pushUndoer(new undoers::SetMask(undo.getObjects(), document));

    // Change the mask
    {
      base::UniquePtr<Mask> mask(generateMask(sprite, image, xpos, ypos));
      document->setMask(mask);
    }

    undo.commit();

    set_config_color("MaskColor", "Color", m_buttonColor->getColor());
    set_config_int("MaskColor", "Tolerance", m_sliderTolerance->getValue());
    set_config_bool("MaskColor", "Preview", m_checkPreview->isSelected());
  }

  // Update boundaries and editors.
  document->generateMaskBoundaries();
  update_screen_for_document(document);

  // Save window configuration.
  save_window_pos(window, "MaskColor");
}

Mask* MaskByColorCommand::generateMask(const Sprite* sprite, const Image* image, int xpos, int ypos)
{
  int color, tolerance;

  color = color_utils::color_for_image(m_buttonColor->getColor(), sprite->pixelFormat());
  tolerance = m_sliderTolerance->getValue();

  base::UniquePtr<Mask> mask(new Mask());
  mask->byColor(image, color, tolerance);
  mask->offsetOrigin(xpos, ypos);

  return mask.release();
}

void MaskByColorCommand::maskPreview(const ContextReader& reader)
{
  if (m_checkPreview->isSelected()) {
    int xpos, ypos;
    const Image* image = reader.image(&xpos, &ypos);
    base::UniquePtr<Mask> mask(generateMask(reader.sprite(), image, xpos, ypos));
    {
      ContextWriter writer(reader);
      writer.document()->generateMaskBoundaries(mask);
      update_screen_for_document(writer.document());
    }
  }
}

Command* CommandFactory::createMaskByColorCommand()
{
  return new MaskByColorCommand;
}

} // namespace app
