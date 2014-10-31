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
#include "app/context.h"
#include "app/document.h"
#include "app/settings/document_settings.h"
#include "app/settings/settings.h"

namespace app {

using namespace gfx;

class ShowOnionSkinCommand : public Command {
public:
  ShowOnionSkinCommand()
    : Command("ShowOnionSkin",
              "Show Onion Skin",
              CmdUIOnlyFlag)
  {
  }

  Command* clone() const override { return new ShowOnionSkinCommand(*this); }

protected:
  bool onChecked(Context* context)
  {
    IDocumentSettings* docSettings = context->settings()->getDocumentSettings(context->activeDocument());

    return docSettings->getUseOnionskin();
  }

  void onExecute(Context* context)
  {
    IDocumentSettings* docSettings = context->settings()->getDocumentSettings(context->activeDocument());

    docSettings->setUseOnionskin(docSettings->getUseOnionskin() ? false: true);
  }
};

Command* CommandFactory::createShowOnionSkinCommand()
{
  return new ShowOnionSkinCommand;
}

} // namespace app
