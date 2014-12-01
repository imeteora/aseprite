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
#include "app/commands/command.h"
#include "app/commands/commands.h"
#include "app/context_access.h"
#include "app/document_api.h"
#include "app/document_location.h"
#include "app/modules/gui.h"
#include "app/ui/color_bar.h"
#include "app/ui/main_window.h"
#include "app/ui/timeline.h"
#include "app/undo_transaction.h"
#include "doc/layer.h"
#include "doc/mask.h"
#include "undo/undo_history.h"

namespace app {

class ClearCommand : public Command {
public:
  ClearCommand();
  Command* clone() const override { return new ClearCommand(*this); }

protected:
  bool onEnabled(Context* context);
  void onExecute(Context* context);
};

ClearCommand::ClearCommand()
  : Command("Clear",
            "Clear",
            CmdUIOnlyFlag)
{
}

bool ClearCommand::onEnabled(Context* context)
{
  return context->checkFlags(ContextFlags::ActiveDocumentIsWritable |
                             ContextFlags::ActiveLayerIsVisible |
                             ContextFlags::ActiveLayerIsEditable |
                             ContextFlags::ActiveLayerIsImage);
}

void ClearCommand::onExecute(Context* context)
{
  // Clear of several frames is handled with ClearCel command.
  DocumentRange range = App::instance()->getMainWindow()->getTimeline()->range();
  if (range.enabled()) {
    Command* subCommand = NULL;

    switch (range.type()) {
      case DocumentRange::kCels:
        subCommand = CommandsModule::instance()->getCommandByName(CommandId::ClearCel);
        break;
      case DocumentRange::kFrames:
        subCommand = CommandsModule::instance()->getCommandByName(CommandId::RemoveFrame);
        break;
      case DocumentRange::kLayers:
        subCommand = CommandsModule::instance()->getCommandByName(CommandId::RemoveLayer);
        break;
    }

    if (subCommand) {
      context->executeCommand(subCommand);
      return;
    }
  }

  // TODO add support to clear the mask in the selected range of frames.
  
  ContextWriter writer(context);
  Document* document = writer.document();
  bool visibleMask = document->isMaskVisible();

  if (!writer.cel())
    return;

  {
    UndoTransaction undoTransaction(writer.context(), "Clear");
    DocumentApi api = document->getApi();
    api.clearMask(writer.cel());
    if (visibleMask)
      api.deselectMask();

    undoTransaction.commit();
  }
  if (visibleMask)
    document->generateMaskBoundaries();
  update_screen_for_document(document);
}

Command* CommandFactory::createClearCommand()
{
  return new ClearCommand;
}

} // namespace app
