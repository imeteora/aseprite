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

#include "app/app_menus.h"

#include "app/app.h"
#include "app/commands/command.h"
#include "app/commands/commands.h"
#include "app/commands/params.h"
#include "app/console.h"
#include "app/gui_xml.h"
#include "app/recent_files.h"
#include "app/resource_finder.h"
#include "app/tools/tool_box.h"
#include "app/ui/app_menuitem.h"
#include "app/ui/keyboard_shortcuts.h"
#include "app/ui/main_window.h"
#include "app/util/filetoks.h"
#include "base/bind.h"
#include "base/fs.h"
#include "base/path.h"
#include "ui/ui.h"

#include "tinyxml.h"

#include <cstdio>
#include <cstring>

namespace app {

using namespace ui;

static void destroy_instance(AppMenus* instance)
{
  delete instance;
}

// static
AppMenus* AppMenus::instance()
{
  static AppMenus* instance = NULL;
  if (!instance) {
    instance = new AppMenus;
    App::instance()->Exit.connect(Bind<void>(&destroy_instance, instance));
  }
  return instance;
}

AppMenus::AppMenus()
  : m_recentListMenuitem(NULL)
{
}

void AppMenus::reload()
{
  XmlDocumentRef doc(GuiXml::instance()->doc());
  TiXmlHandle handle(doc);
  const char* path = GuiXml::instance()->filename();

  ////////////////////////////////////////
  // Load menus

  PRINTF(" - Loading menus from \"%s\"...\n", path);

  m_rootMenu.reset(loadMenuById(handle, "main_menu"));

  // Add a warning element because the user is not using the last well-known gui.xml file.
  if (GuiXml::instance()->version() != VERSION)
    m_rootMenu->insertChild(0, createInvalidVersionMenuitem());

  PRINTF("Main menu loaded.\n");

  m_documentTabPopupMenu.reset(loadMenuById(handle, "document_tab_popup"));
  m_layerPopupMenu.reset(loadMenuById(handle, "layer_popup"));
  m_framePopupMenu.reset(loadMenuById(handle, "frame_popup"));
  m_celPopupMenu.reset(loadMenuById(handle, "cel_popup"));
  m_celMovementPopupMenu.reset(loadMenuById(handle, "cel_movement_popup"));

  ////////////////////////////////////////
  // Load keyboard shortcuts for commands

  PRINTF(" - Loading commands keyboard shortcuts from \"%s\"...\n", path);

  TiXmlElement* xmlKey = handle
    .FirstChild("gui")
    .FirstChild("keyboard").ToElement();

  KeyboardShortcuts::instance()->clear();
  KeyboardShortcuts::instance()->importFile(xmlKey, KeySource::Original);

  // Load user settings
  {
    ResourceFinder rf;
    rf.includeUserDir("user.aseprite-keys");
    std::string fn = rf.getFirstOrCreateDefault();
    if (base::is_file(fn))
      KeyboardShortcuts::instance()->importFile(fn, KeySource::UserDefined);
  }
}

bool AppMenus::rebuildRecentList()
{
  MenuItem* list_menuitem = m_recentListMenuitem;
  MenuItem* menuitem;

  // Update the recent file list menu item
  if (list_menuitem) {
    if (list_menuitem->hasSubmenuOpened())
      return false;

    Command* cmd_open_file = CommandsModule::instance()->getCommandByName(CommandId::OpenFile);

    Menu* submenu = list_menuitem->getSubmenu();
    if (submenu) {
      list_menuitem->setSubmenu(NULL);
      submenu->deferDelete();
    }

    // Build the menu of recent files
    submenu = new Menu();
    list_menuitem->setSubmenu(submenu);

    RecentFiles::const_iterator it = App::instance()->getRecentFiles()->files_begin();
    RecentFiles::const_iterator end = App::instance()->getRecentFiles()->files_end();

    if (it != end) {
      Params params;

      for (; it != end; ++it) {
        const char* filename = it->c_str();

        params.set("filename", filename);

        menuitem = new AppMenuItem(
          base::get_file_name(filename).c_str(),
          cmd_open_file, &params);
        submenu->addChild(menuitem);
      }
    }
    else {
      menuitem = new AppMenuItem("Nothing", NULL, NULL);
      menuitem->setEnabled(false);
      submenu->addChild(menuitem);
    }
  }

  return true;
}

Menu* AppMenus::loadMenuById(TiXmlHandle& handle, const char* id)
{
  ASSERT(id != NULL);

  //PRINTF("loadMenuById(%s)\n", id);

  // <gui><menus><menu>
  TiXmlElement* xmlMenu = handle
    .FirstChild("gui")
    .FirstChild("menus")
    .FirstChild("menu").ToElement();
  while (xmlMenu) {
    const char* menu_id = xmlMenu->Attribute("id");

    if (menu_id && strcmp(menu_id, id) == 0)
      return convertXmlelemToMenu(xmlMenu);

    xmlMenu = xmlMenu->NextSiblingElement();
  }

  throw base::Exception("Error loading menu '%s'\nReinstall the application.", id);
}

Menu* AppMenus::convertXmlelemToMenu(TiXmlElement* elem)
{
  Menu* menu = new Menu();

  //PRINTF("convertXmlelemToMenu(%s, %s, %s)\n", elem->Value(), elem->Attribute("id"), elem->Attribute("text"));

  TiXmlElement* child = elem->FirstChildElement();
  while (child) {
    Widget* menuitem = convertXmlelemToMenuitem(child);
    if (menuitem)
      menu->addChild(menuitem);
    else
      throw base::Exception("Error converting the element \"%s\" to a menu-item.\n",
                            static_cast<const char*>(child->Value()));

    child = child->NextSiblingElement();
  }

  return menu;
}

Widget* AppMenus::convertXmlelemToMenuitem(TiXmlElement* elem)
{
  // is it a <separator>?
  if (strcmp(elem->Value(), "separator") == 0)
    return new Separator("", JI_HORIZONTAL);

  const char* command_name = elem->Attribute("command");
  Command* command =
    command_name ? CommandsModule::instance()->getCommandByName(command_name):
                   NULL;

  // load params
  Params params;
  if (command) {
    TiXmlElement* xmlParam = elem->FirstChildElement("param");
    while (xmlParam) {
      const char* param_name = xmlParam->Attribute("name");
      const char* param_value = xmlParam->Attribute("value");

      if (param_name && param_value)
        params.set(param_name, param_value);

      xmlParam = xmlParam->NextSiblingElement();
    }
  }

  // Create the item
  AppMenuItem* menuitem = new AppMenuItem(elem->Attribute("text"),
                                          command, command ? &params: NULL);
  if (!menuitem)
    return NULL;

  /* has it a ID? */
  const char* id = elem->Attribute("id");
  if (id) {
    /* recent list menu */
    if (strcmp(id, "recent_list") == 0) {
      m_recentListMenuitem = menuitem;
    }
  }

  // Has it a sub-menu (<menu>)?
  if (strcmp(elem->Value(), "menu") == 0) {
    // Create the sub-menu
    Menu* subMenu = convertXmlelemToMenu(elem);
    if (!subMenu)
      throw base::Exception("Error reading the sub-menu\n");

    menuitem->setSubmenu(subMenu);
  }

  return menuitem;
}

Widget* AppMenus::createInvalidVersionMenuitem()
{
  AppMenuItem* menuitem = new AppMenuItem("WARNING!", NULL, NULL);
  Menu* subMenu = new Menu();
  subMenu->addChild(new AppMenuItem(PACKAGE " is using a customized gui.xml (maybe from your HOME directory).", NULL, NULL));
  subMenu->addChild(new AppMenuItem("You should update your customized gui.xml file to the new version to get", NULL, NULL));
  subMenu->addChild(new AppMenuItem("the latest commands available.", NULL, NULL));
  subMenu->addChild(new Separator("", JI_HORIZONTAL));
  subMenu->addChild(new AppMenuItem("You can bypass this validation adding the correct version", NULL, NULL));
  subMenu->addChild(new AppMenuItem("number in <gui version=\"" VERSION "\"> element.", NULL, NULL));
  menuitem->setSubmenu(subMenu);
  return menuitem;
}

void AppMenus::applyShortcutToMenuitemsWithCommand(Command* command, Params* params, Key* key)
{
  applyShortcutToMenuitemsWithCommand(m_rootMenu, command, params, key);
}

void AppMenus::applyShortcutToMenuitemsWithCommand(Menu* menu, Command* command, Params* params, Key* key)
{
  UI_FOREACH_WIDGET(menu->getChildren(), it) {
    Widget* child = *it;

    if (child->getType() == kMenuItemWidget) {
      ASSERT(dynamic_cast<AppMenuItem*>(child) != NULL);

      AppMenuItem* menuitem = static_cast<AppMenuItem*>(child);
      Command* mi_command = menuitem->getCommand();
      Params* mi_params = menuitem->getParams();

      if ((mi_command) &&
          (base::string_to_lower(mi_command->short_name()) ==
           base::string_to_lower(command->short_name())) &&
          ((mi_params && *mi_params == *params) ||
           (Params() == *params))) {
        // Set the keyboard shortcut to be shown in this menu-item
        menuitem->setKey(key);
      }

      if (Menu* submenu = menuitem->getSubmenu())
        applyShortcutToMenuitemsWithCommand(submenu, command, params, key);
    }
  }
}

} // namespace app
