// Aseprite
// Copyright (C) 2016  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/app.h"
#include "app/commands/command.h"
#include "app/commands/params.h"
#include "app/context.h"
#include "app/ui/main_window.h"

namespace app {

class OpenBrowserCommand : public Command {
public:
  OpenBrowserCommand();
  Command* clone() const override { return new OpenBrowserCommand(*this); }

protected:
  void onLoadParams(const Params& params) override;
  void onExecute(Context* context) override;

private:
  std::string m_filename;
};

OpenBrowserCommand::OpenBrowserCommand()
  : Command("OpenBrowser",
            "Open Browser",
            CmdUIOnlyFlag)
{
}

void OpenBrowserCommand::onLoadParams(const Params& params)
{
  m_filename = params.get("filename");
}

void OpenBrowserCommand::onExecute(Context* context)
{
  App::instance()->mainWindow()->showBrowser(m_filename);
}

Command* CommandFactory::createOpenBrowserCommand()
{
  return new OpenBrowserCommand;
}

} // namespace app
