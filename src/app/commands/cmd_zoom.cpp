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

#include "app/commands/command.h"
#include "app/commands/params.h"
#include "app/modules/editors.h"
#include "app/ui/editor/editor.h"
#include "base/convert_to.h"
#include "render/zoom.h"

namespace app {

class ZoomCommand : public Command {
public:
  enum Action { In, Out, Set };

  ZoomCommand();
  Command* clone() const override { return new ZoomCommand(*this); }

protected:
  void onLoadParams(Params* params);
  bool onEnabled(Context* context);
  void onExecute(Context* context);
  std::string onGetFriendlyName() const;

private:
  Action m_action;
  int m_percentage;
};

ZoomCommand::ZoomCommand()
  : Command("Zoom",
            "Zoom",
            CmdUIOnlyFlag)
{
}

void ZoomCommand::onLoadParams(Params* params)
{
  std::string action = params->get("action");
  if (action == "in") m_action = In;
  else if (action == "out") m_action = Out;
  else if (action == "set") m_action = Set;

  std::string percentage = params->get("percentage");
  if (!percentage.empty()) {
    m_percentage = std::strtol(percentage.c_str(), NULL, 10);
    m_action = Set;
  }
}

bool ZoomCommand::onEnabled(Context* context)
{
  return current_editor != NULL;
}

void ZoomCommand::onExecute(Context* context)
{
  render::Zoom zoom = current_editor->zoom();

  switch (m_action) {
    case In:
      zoom.in();
      break;
    case Out:
      zoom.out();
      break;
    case Set:
      switch (m_percentage) {
        case 3200: zoom = render::Zoom(32, 1); break;
        case 1600: zoom = render::Zoom(16, 1); break;
        case 800: zoom = render::Zoom(8, 1); break;
        case 400: zoom = render::Zoom(4, 1); break;
        case 200: zoom = render::Zoom(2, 1); break;
        default: zoom = render::Zoom(1, 1); break;
      }
      break;
  }

  current_editor->setEditorZoom(zoom);
}

std::string ZoomCommand::onGetFriendlyName() const
{
  std::string text = "Zoom";

  switch (m_action) {
    case In:
      text += " in";
      break;
    case Out:
      text += " out";
      break;
    case Set:
      text += " " + base::convert_to<std::string>(m_percentage) + "%";
      break;
  }

  return text;
}

Command* CommandFactory::createZoomCommand()
{
  return new ZoomCommand;
}

} // namespace app
