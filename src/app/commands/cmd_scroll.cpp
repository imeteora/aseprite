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

#include "app/commands/command.h"
#include "app/commands/params.h"
#include "app/context_access.h"
#include "app/modules/editors.h"
#include "app/settings/document_settings.h"
#include "app/settings/settings.h"
#include "app/ui/editor/editor.h"
#include "base/convert_to.h"
#include "ui/view.h"

namespace app {

class ScrollCommand : public Command {
public:
  enum Direction { Left, Up, Right, Down, };
  enum Units {
    Pixel,
    TileWidth,
    TileHeight,
    ZoomedPixel,
    ZoomedTileWidth,
    ZoomedTileHeight,
    ViewportWidth,
    ViewportHeight
  };

  ScrollCommand();
  Command* clone() const override { return new ScrollCommand(*this); }

protected:
  void onLoadParams(Params* params);
  bool onEnabled(Context* context);
  void onExecute(Context* context);
  std::string onGetFriendlyName() const;

private:
  Direction m_direction;
  Units m_units;
  int m_quantity;
};

ScrollCommand::ScrollCommand()
  : Command("Scroll",
            "Scroll",
            CmdUIOnlyFlag)
{
}

void ScrollCommand::onLoadParams(Params* params)
{
  std::string direction = params->get("direction");
  if (direction == "left") m_direction = Left;
  else if (direction == "right") m_direction = Right;
  else if (direction == "up") m_direction = Up;
  else if (direction == "down") m_direction = Down;

  std::string units = params->get("units");
  if (units == "pixel") m_units = Pixel;
  else if (units == "tile-width") m_units = TileWidth;
  else if (units == "tile-height") m_units = TileHeight;
  else if (units == "zoomed-pixel") m_units = ZoomedPixel;
  else if (units == "zoomed-tile-width") m_units = ZoomedTileWidth;
  else if (units == "zoomed-tile-height") m_units = ZoomedTileHeight;
  else if (units == "viewport-width") m_units = ViewportWidth;
  else if (units == "viewport-height") m_units = ViewportHeight;

  int quantity = params->get_as<int>("quantity");
  m_quantity = std::max<int>(1, quantity);
}

bool ScrollCommand::onEnabled(Context* context)
{
  ContextWriter writer(context);
  return (writer.document() != NULL);
}

void ScrollCommand::onExecute(Context* context)
{
  IDocumentSettings* docSettings = context->settings()->getDocumentSettings(context->activeDocument());
  ui::View* view = ui::View::getView(current_editor);
  gfx::Rect vp = view->getViewportBounds();
  gfx::Point scroll = view->getViewScroll();
  gfx::Rect gridBounds = docSettings->getGridBounds();
  int dx = 0;
  int dy = 0;
  int pixels = 0;

  switch (m_units) {
    case Pixel:
      pixels = 1;
      break;
    case TileWidth:
      pixels = gridBounds.w;
      break;
    case TileHeight:
      pixels = gridBounds.h;
      break;
    case ZoomedPixel:
      pixels = 1 << current_editor->zoom();
      break;
    case ZoomedTileWidth:
      pixels = gridBounds.w << current_editor->zoom();
      break;
    case ZoomedTileHeight:
      pixels = gridBounds.h << current_editor->zoom();
      break;
    case ViewportWidth:
      pixels = vp.h;
      break;
    case ViewportHeight:
      pixels = vp.w;
      break;
  }

  switch (m_direction) {
    case Left:  dx = -m_quantity * pixels; break;
    case Right: dx = +m_quantity * pixels; break;
    case Up:    dy = -m_quantity * pixels; break;
    case Down:  dy = +m_quantity * pixels; break;
  }

  current_editor->setEditorScroll(scroll.x+dx, scroll.y+dy, true);
}

std::string ScrollCommand::onGetFriendlyName() const
{
  std::string text = "Scroll " + base::convert_to<std::string>(m_quantity);

  switch (m_units) {
    case Pixel:
      text += " pixel";
      break;
    case TileWidth:
      text += " horizontal tile";
      break;
    case TileHeight:
      text += " vertical tile";
      break;
    case ZoomedPixel:
      text += " zoomed pixel";
      break;
    case ZoomedTileWidth:
      text += " zoomed horizontal tile";
      break;
    case ZoomedTileHeight:
      text += " zoomed vertical tile";
      break;
    case ViewportWidth:
      text += " viewport width";
      break;
    case ViewportHeight:
      text += " viewport height";
      break;
  }
  if (m_quantity != 1)
    text += "s";

  switch (m_direction) {
    case Left:  text += " left"; break;
    case Right: text += " right"; break;
    case Up:    text += " up"; break;
    case Down:  text += " down"; break;
  }

  return text;
}

Command* CommandFactory::createScrollCommand()
{
  return new ScrollCommand;
}

} // namespace app
