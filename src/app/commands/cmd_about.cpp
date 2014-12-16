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
#include "app/modules/gui.h"
#include "base/bind.h"
#include "ui/ui.h"

namespace app {

using namespace ui;

class AboutCommand : public Command {
public:
  AboutCommand();
  Command* clone() const override { return new AboutCommand(*this); }

protected:
  void onExecute(Context* context);
};

AboutCommand::AboutCommand()
  : Command("About",
            "About",
            CmdUIOnlyFlag)
{
}

void AboutCommand::onExecute(Context* context)
{
  base::UniquePtr<Window> window(new Window(Window::WithTitleBar, "About " PACKAGE));
  Box* box1 = new Box(JI_VERTICAL);
  Grid* grid = new Grid(2, false);
  Label* title = new Label(PACKAGE " v" VERSION);
  Label* subtitle = new Label("Animated sprite editor && pixel art tool");
  Separator* authors_separator1 = new Separator("Authors:", JI_HORIZONTAL | JI_TOP);
  Separator* authors_separator2 = new Separator("", JI_HORIZONTAL);
  Label* author1 = new LinkLabel("http://dacap.com.ar/", "David Capello");
  Label* author1_desc = new Label("- Lead developer, graphics && maintainer");
  Label* author2 = new LinkLabel("http://ilkke.blogspot.com/", "Ilija Melentijevic");
  Label* author2_desc = new Label("- Default skin && graphics introduced in v0.8");
  Label* author3 = new LinkLabel(WEBSITE_CONTRIBUTORS, "Contributors");
  Box* bottom_box1 = new Box(JI_HORIZONTAL);
  Box* bottom_box2 = new Box(JI_HORIZONTAL);
  Box* bottom_box3 = new Box(JI_HORIZONTAL);
  Label* copyright = new Label(COPYRIGHT);
  Label* website = new LinkLabel(WEBSITE);
  Button* close_button = new Button("&Close");

  grid->addChildInCell(title, 2, 1, 0);
  grid->addChildInCell(subtitle, 2, 1, 0);
  grid->addChildInCell(authors_separator1, 2, 1, 0);
  grid->addChildInCell(author1, 1, 1, 0);
  grid->addChildInCell(author1_desc, 1, 1, 0);
  grid->addChildInCell(author2, 1, 1, 0);
  grid->addChildInCell(author2_desc, 1, 1, 0);
  grid->addChildInCell(author3, 2, 1, 0);
  grid->addChildInCell(authors_separator2, 2, 1, 0);
  grid->addChildInCell(copyright, 2, 1, 0);
  grid->addChildInCell(website, 2, 1, 0);
  grid->addChildInCell(bottom_box1, 2, 1, 0);

  close_button->setFocusMagnet(true);

  bottom_box2->setExpansive(true);
  bottom_box3->setExpansive(true);

  bottom_box1->addChild(bottom_box2);
  bottom_box1->addChild(close_button);
  bottom_box1->addChild(bottom_box3);

  box1->addChild(grid);
  window->addChild(box1);

  close_button->setBorder(gfx::Border(
      close_button->border_width.l + 16*guiscale(),
      close_button->border_width.t,
      close_button->border_width.r + 16*guiscale(),
      close_button->border_width.b));

  close_button->Click.connect(Bind<void>(&Window::closeWindow, window.get(), close_button));

  window->openWindowInForeground();
}

Command* CommandFactory::createAboutCommand()
{
  return new AboutCommand;
}

} // namespace app
