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

#include "app/ui/keyboard_shortcuts.h"

#include "app/app.h"
#include "app/app_menus.h"
#include "app/commands/command.h"
#include "app/commands/commands.h"
#include "app/commands/params.h"
#include "app/document.h"
#include "app/document_location.h"
#include "app/settings/settings.h"
#include "app/tools/ink.h"
#include "app/tools/tool.h"
#include "app/tools/tool_box.h"
#include "app/ui_context.h"
#include "app/xml_document.h"
#include "app/xml_exception.h"
#include "ui/accelerator.h"
#include "ui/message.h"

#define XML_KEYBOARD_FILE_VERSION "1"

namespace {

  static struct {
    const char* name;
    const char* userfriendly;
    app::KeyAction action;
  } actions[] = {
    { "CopySelection"       , "Copy Selection"     , app::KeyAction::CopySelection },
    { "SnapToGrid"          , "Snap To Grid"       , app::KeyAction::SnapToGrid },
    { "AngleSnap"           , "Angle Snap"         , app::KeyAction::AngleSnap },
    { "MaintainAspectRatio" , "Maintain Aspect Ratio", app::KeyAction::MaintainAspectRatio },
    { "LockAxis"            , "Lock Axis"          , app::KeyAction::LockAxis },
    { "AddSelection"        , "Add Selection"      , app::KeyAction::AddSelection },
    { "SubtractSelection"   , "Subtract Selection" , app::KeyAction::SubtractSelection },
    { "AutoSelectLayer"     , "Auto Select Layer" , app::KeyAction::AutoSelectLayer },
    { "LeftMouseButton"     , "Trigger Left Mouse Button" , app::KeyAction::LeftMouseButton },
    { "RightMouseButton"    , "Trigger Right Mouse Button" , app::KeyAction::RightMouseButton },
    { NULL                  , NULL                 , app::KeyAction::None }
  };

  const char* get_shortcut(TiXmlElement* elem)
  {
    const char* shortcut = NULL;

#if defined ALLEGRO_WINDOWS
    if (!shortcut) shortcut = elem->Attribute("win");
#elif defined ALLEGRO_MACOSX
    if (!shortcut) shortcut = elem->Attribute("mac");
#elif defined ALLEGRO_UNIX
    if (!shortcut) shortcut = elem->Attribute("linux");
#endif

    if (!shortcut)
      shortcut = elem->Attribute("shortcut");

    return shortcut;
  }

  bool bool_attr_is_true(const TiXmlElement* elem, const char* attribute_name)
  {
    const char* value = elem->Attribute(attribute_name);

    return (value != NULL) && (strcmp(value, "true") == 0);
  }

  std::string get_user_friendly_string_for_keyaction(app::KeyAction action)
  {
    for (int c=0; actions[c].name; ++c) {
      if (action == actions[c].action)
        return actions[c].userfriendly;
    }
    return "";
  }
  
} // anonymous namespace

namespace base {

  template<> app::KeyAction convert_to(const std::string& from) {
    app::KeyAction action = app::KeyAction::None;

    for (int c=0; actions[c].name; ++c) {
      if (from == actions[c].name)
        return actions[c].action;
    }

    return action;
  }

  template<> std::string convert_to(const app::KeyAction& from) {
    for (int c=0; actions[c].name; ++c) {
      if (from == actions[c].action)
        return actions[c].name;
    }
    return "";
  }
  
} // namespace base

namespace app {

using namespace ui;

//////////////////////////////////////////////////////////////////////
// Key

Key::Key(Command* command, const Params* params, KeyContext keyContext)
  : m_type(KeyType::Command)
  , m_useUsers(false)
  , m_keycontext(keyContext)
  , m_command(command)
  , m_params(params ? params->clone(): NULL)
{
}

Key::Key(KeyType type, tools::Tool* tool)
  : m_type(type)
  , m_useUsers(false)
  , m_keycontext(KeyContext::Any)
  , m_tool(tool)
{
}

Key::Key(KeyAction action)
  : m_type(KeyType::Action)
  , m_useUsers(false)
  , m_keycontext(KeyContext::Any)
  , m_action(action)
{
  switch (action) {
    case KeyAction::None:
      m_keycontext = KeyContext::Any;
      break;
    case KeyAction::CopySelection:
      m_keycontext = KeyContext::MovingPixels;
      break;
    case KeyAction::SnapToGrid:
      m_keycontext = KeyContext::MovingPixels;
      break;
    case KeyAction::AngleSnap:
      m_keycontext = KeyContext::MovingPixels;
      break;
    case KeyAction::MaintainAspectRatio:
      m_keycontext = KeyContext::MovingPixels;
      break;
    case KeyAction::LockAxis:
      m_keycontext = KeyContext::MovingPixels;
      break;
    case KeyAction::AddSelection:
      m_keycontext = KeyContext::Selection;
      break;
    case KeyAction::SubtractSelection:
      m_keycontext = KeyContext::Selection;
      break;
    case KeyAction::AutoSelectLayer:
      m_keycontext = KeyContext::MoveTool;
      break;
    case KeyAction::LeftMouseButton:
      m_keycontext = KeyContext::Any;
      break;
    case KeyAction::RightMouseButton:
      m_keycontext = KeyContext::Any;
      break;
  }
}

void Key::add(const ui::Accelerator& accel, KeySource source)
{
  Accelerators* accels = &m_accels;

  if (source == KeySource::UserDefined) {
    if (!m_useUsers) {
      m_useUsers = true;
      m_users = m_accels;
    }
    accels = &m_users;
  }

  // Remove the accelerator from other commands
  if (source == KeySource::UserDefined) {
    KeyboardShortcuts::instance()->disableAccel(accel, m_keycontext);
    m_userRemoved.remove(accel);
  }

  // Add the accelerator
  accels->add(accel);
}

bool Key::isPressed(Message* msg) const
{
  ASSERT(dynamic_cast<KeyMessage*>(msg) != NULL);

  for (const Accelerator& accel : accels()) {
    if (accel.check(msg->keyModifiers(),
          static_cast<KeyMessage*>(msg)->scancode(),
          static_cast<KeyMessage*>(msg)->unicodeChar()) &&
        (m_keycontext == KeyContext::Any ||
         m_keycontext == KeyboardShortcuts::instance()->getCurrentKeyContext())) {
      return true;
    }
  }

  return false;
}

bool Key::checkFromAllegroKeyArray()
{
  for (const Accelerator& accel : this->accels()) {
    if (accel.checkFromAllegroKeyArray())
      return true;
  }
  return false;
}

bool Key::hasAccel(const ui::Accelerator& accel) const
{
  return accels().has(accel);
}

void Key::disableAccel(const ui::Accelerator& accel)
{
  if (!m_useUsers) {
    m_useUsers = true;
    m_users = m_accels;
  }

  m_users.remove(accel);

  if (m_accels.has(accel))
    m_userRemoved.add(accel);
}

void Key::reset()
{
  m_users.clear();
  m_userRemoved.clear();
  m_useUsers = false;
}

std::string Key::triggerString() const
{
  switch (m_type) {
    case KeyType::Command:
      if (m_params)
        m_command->loadParams(m_params);
      return m_command->friendlyName();
    case KeyType::Tool:
    case KeyType::Quicktool: {
      std::string text = m_tool->getText();
      if (m_type == KeyType::Quicktool)
        text += " (quick)";
      return text;
    }
    case KeyType::Action:
      return get_user_friendly_string_for_keyaction(m_action);
  }
  return "Unknown";
}

//////////////////////////////////////////////////////////////////////
// KeyboardShortcuts

KeyboardShortcuts* KeyboardShortcuts::instance()
{
  static KeyboardShortcuts* singleton = NULL;
  if (!singleton)
    singleton = new KeyboardShortcuts();
  return singleton;
}

KeyboardShortcuts::KeyboardShortcuts()
{
}

KeyboardShortcuts::~KeyboardShortcuts()
{
  clear();
}

void KeyboardShortcuts::clear()
{
  for (Key* key : m_keys) {
    delete key;
  }
  m_keys.clear();
}

void KeyboardShortcuts::importFile(TiXmlElement* rootElement, KeySource source)
{
  // <keyboard><commands><key>
  TiXmlHandle handle(rootElement);
  TiXmlElement* xmlKey = handle
    .FirstChild("commands")
    .FirstChild("key").ToElement();
  while (xmlKey) {
    const char* command_name = xmlKey->Attribute("command");
    const char* command_key = get_shortcut(xmlKey);
    bool removed = bool_attr_is_true(xmlKey, "removed");

    if (command_name && command_key) {
      Command* command = CommandsModule::instance()->getCommandByName(command_name);
      if (command) {
        // Read context
        KeyContext keycontext = KeyContext::Any;
        const char* keycontextstr = xmlKey->Attribute("context");
        if (keycontextstr) {
          if (strcmp(keycontextstr, "Selection") == 0)
            keycontext = KeyContext::Selection;
          else if (strcmp(keycontextstr, "Normal") == 0)
            keycontext = KeyContext::Normal;
        }

        // Read params
        Params params;

        TiXmlElement* xmlParam = xmlKey->FirstChildElement("param");
        while (xmlParam) {
          const char* param_name = xmlParam->Attribute("name");
          const char* param_value = xmlParam->Attribute("value");

          if (param_name && param_value)
            params.set(param_name, param_value);

          xmlParam = xmlParam->NextSiblingElement();
        }

        PRINTF(" - Shortcut for command `%s' <%s>\n", command_name, command_key);

        // add the keyboard shortcut to the command
        Key* key = this->command(command_name, &params, keycontext);
        if (key) {
          Accelerator accel(command_key);

          if (!removed) {
            key->add(accel, source);

            // Add the shortcut to the menuitems with this
            // command (this is only visual, the "manager_msg_proc"
            // is the only one that process keyboard shortcuts)
            if (key->accels().size() == 1) {
              AppMenus::instance()->applyShortcutToMenuitemsWithCommand(
                command, &params, key);
            }
          }
          else
            key->disableAccel(accel);
        }
      }
    }

    xmlKey = xmlKey->NextSiblingElement();
  }

  // Load keyboard shortcuts for tools
  // <gui><keyboard><tools><key>
  xmlKey = handle
    .FirstChild("tools")
    .FirstChild("key").ToElement();
  while (xmlKey) {
    const char* tool_id = xmlKey->Attribute("tool");
    const char* tool_key = get_shortcut(xmlKey);
    bool removed = bool_attr_is_true(xmlKey, "removed");

    if (tool_id && tool_key) {
      tools::Tool* tool = App::instance()->getToolBox()->getToolById(tool_id);
      if (tool) {
        PRINTF(" - Shortcut for tool `%s': <%s>\n", tool_id, tool_key);

        Key* key = this->tool(tool);
        if (key) {
          Accelerator accel(tool_key);

          if (!removed)
            key->add(accel, source);
          else
            key->disableAccel(accel);
        }
      }
    }
    xmlKey = xmlKey->NextSiblingElement();
  }

  // Load keyboard shortcuts for quicktools
  // <gui><keyboard><quicktools><key>
  xmlKey = handle
    .FirstChild("quicktools")
    .FirstChild("key").ToElement();
  while (xmlKey) {
    const char* tool_id = xmlKey->Attribute("tool");
    const char* tool_key = get_shortcut(xmlKey);
    bool removed = bool_attr_is_true(xmlKey, "removed");

    if (tool_id && tool_key) {
      tools::Tool* tool = App::instance()->getToolBox()->getToolById(tool_id);
      if (tool) {
        PRINTF(" - Shortcut for quicktool `%s': <%s>\n", tool_id, tool_key);

        Key* key = this->quicktool(tool);
        if (key) {
          Accelerator accel(tool_key);

          if (!removed)
            key->add(accel, source);
          else
            key->disableAccel(accel);
        }
      }
    }
    xmlKey = xmlKey->NextSiblingElement();
  }

  // Load special keyboard shortcuts for sprite editor customization
  // <gui><keyboard><spriteeditor>
  xmlKey = handle
    .FirstChild("actions")
    .FirstChild("key").ToElement();
  while (xmlKey) {
    const char* tool_action = xmlKey->Attribute("action");
    const char* tool_key = get_shortcut(xmlKey);
    bool removed = bool_attr_is_true(xmlKey, "removed");

    if (tool_action && tool_key) {
      PRINTF(" - Shortcut for sprite editor `%s': <%s>\n", tool_action, tool_key);

      KeyAction action = base::convert_to<KeyAction, std::string>(tool_action);

      if (action != KeyAction::None) {
        Key* key = this->action(action);
        if (key) {
          Accelerator accel(tool_key);

          if (!removed)
            key->add(accel, source);
          else
            key->disableAccel(accel);
        }
      }
    }
    xmlKey = xmlKey->NextSiblingElement();
  }
}

void KeyboardShortcuts::importFile(const std::string& filename, KeySource source)
{
  XmlDocumentRef doc = app::open_xml(filename);
  TiXmlHandle handle(doc);
  TiXmlElement* xmlKey = handle.FirstChild("keyboard").ToElement();

  importFile(xmlKey, source);
}

void KeyboardShortcuts::exportFile(const std::string& filename)
{
  XmlDocumentRef doc(new TiXmlDocument());

  TiXmlElement keyboard("keyboard");
  TiXmlElement commands("commands");
  TiXmlElement tools("tools");
  TiXmlElement quicktools("quicktools");
  TiXmlElement actions("actions");

  keyboard.SetAttribute("version", XML_KEYBOARD_FILE_VERSION);

  exportKeys(commands, KeyType::Command);
  exportKeys(tools, KeyType::Tool);
  exportKeys(quicktools, KeyType::Quicktool);
  exportKeys(actions, KeyType::Action);
  
  keyboard.InsertEndChild(commands);
  keyboard.InsertEndChild(tools);
  keyboard.InsertEndChild(quicktools);
  keyboard.InsertEndChild(actions);
  doc->InsertEndChild(keyboard);
  save_xml(doc, filename);
}

void KeyboardShortcuts::exportKeys(TiXmlElement& parent, KeyType type)
{
  for (Key* key : m_keys) {
    // Save only user defined accelerators.
    if (key->type() != type)
      continue;

    for (const ui::Accelerator& accel : key->userRemovedAccels())
      exportAccel(parent, key, accel, true);

    for (const ui::Accelerator& accel : key->userAccels())
      exportAccel(parent, key, accel, false);
  }
}

void KeyboardShortcuts::exportAccel(TiXmlElement& parent, Key* key, const ui::Accelerator& accel, bool removed)
{
  TiXmlElement elem("key");

  switch (key->type()) {

    case KeyType::Command: {
      const char* keycontextStr = NULL;

      elem.SetAttribute("command", key->command()->short_name());

      switch (key->keycontext()) {
        case KeyContext::Any:
          // Without "context" attribute
          break;
        case KeyContext::Normal:
          keycontextStr = "Normal";
          break;
        case KeyContext::Selection:
          keycontextStr = "Selection";
          break;
        case KeyContext::MovingPixels:
          keycontextStr = "MovingPixels";
          break;
        case KeyContext::MoveTool:
          keycontextStr = "MoveTool";
          break;
      }

      if (keycontextStr)
        elem.SetAttribute("context", keycontextStr);

      if (key->params()) {
        for (const auto& param : *key->params()) {
          if (param.second.empty())
            continue;

          TiXmlElement paramElem("param");
          paramElem.SetAttribute("name", param.first.c_str());
          paramElem.SetAttribute("value", param.second.c_str());
          elem.InsertEndChild(paramElem);
        }
      }
      break;
    }

    case KeyType::Tool:
    case KeyType::Quicktool:
      elem.SetAttribute("tool", key->tool()->getId().c_str());
      break;

    case KeyType::Action:
      elem.SetAttribute("action",
        base::convert_to<std::string>(key->action()).c_str());
      break;
  }

  elem.SetAttribute("shortcut", accel.toString().c_str());

  if (removed)
    elem.SetAttribute("removed", "true");

  parent.InsertEndChild(elem);
}

void KeyboardShortcuts::reset()
{
  for (Key* key : m_keys)
    key->reset();
}

Key* KeyboardShortcuts::command(const char* commandName,
  Params* params, KeyContext keyContext)
{
  Command* command = CommandsModule::instance()->getCommandByName(commandName);
  if (!command)
    return NULL;

  for (Key* key : m_keys) {
    if (key->type() == KeyType::Command &&
        key->keycontext() == keyContext &&
        key->command() == command &&
        ((!params && key->params()->empty()) ||
          (params && *key->params() == *params))) {
      return key;
    }
  }

  Key* key = new Key(command, params, keyContext);
  m_keys.push_back(key);
  return key;
}

Key* KeyboardShortcuts::tool(tools::Tool* tool)
{
  for (Key* key : m_keys) {
    if (key->type() == KeyType::Tool &&
        key->tool() == tool) {
      return key;
    }
  }

  Key* key = new Key(KeyType::Tool, tool);
  m_keys.push_back(key);
  return key;
}

Key* KeyboardShortcuts::quicktool(tools::Tool* tool)
{
  for (Key* key : m_keys) {
    if (key->type() == KeyType::Quicktool &&
        key->tool() == tool) {
      return key;
    }
  }

  Key* key = new Key(KeyType::Quicktool, tool);
  m_keys.push_back(key);
  return key;
}

Key* KeyboardShortcuts::action(KeyAction action)
{
  for (Key* key : m_keys) {
    if (key->type() == KeyType::Action &&
        key->action() == action) {
      return key;
    }
  }

  Key* key = new Key(action);
  m_keys.push_back(key);
  return key;
}

void KeyboardShortcuts::disableAccel(const ui::Accelerator& accel, KeyContext keyContext)
{
  for (Key* key : m_keys) {
    if (key->keycontext() == keyContext && key->hasAccel(accel))
      key->disableAccel(accel);
  }
}

KeyContext KeyboardShortcuts::getCurrentKeyContext()
{
  app::Context* ctx = UIContext::instance();
  DocumentLocation location = ctx->activeLocation();
  Document* doc = location.document();

  if (doc &&
      doc->isMaskVisible() &&
      ctx->settings()->getCurrentTool()->getInk(0)->isSelection())
    return KeyContext::Selection;
  else
    return KeyContext::Normal;
}

bool KeyboardShortcuts::getCommandFromKeyMessage(Message* msg, Command** command, Params** params)
{
  for (Key* key : m_keys) {
    if (key->type() == KeyType::Command && key->isPressed(msg)) {
      if (command) *command = key->command();
      if (params) *params = key->params();
      return true;
    }
  }
  return false;
}

tools::Tool* KeyboardShortcuts::getCurrentQuicktool(tools::Tool* currentTool)
{
  if (currentTool && currentTool->getInk(0)->isSelection()) {
    Key* key = action(KeyAction::CopySelection);
    if (key && key->checkFromAllegroKeyArray())
      return NULL;
  }

  tools::ToolBox* toolbox = App::instance()->getToolBox();

  // Iterate over all tools
  for (tools::Tool* tool : *toolbox) {
    Key* key = quicktool(tool);

    // Collect all tools with the pressed keyboard-shortcut
    if (key && key->checkFromAllegroKeyArray()) {
      return tool;
    }
  }

  return NULL;
}

} // namespace app
