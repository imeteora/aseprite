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

#include <cstdlib>
#include <string>

#include "app/ui/hex_color_entry.h"
#include "gfx/border.h"
#include "ui/theme.h"

namespace app {

using namespace ui;

static inline bool is_hex_digit(char digit)
{
  return ((digit >= '0' && digit <= '9') ||
          (digit >= 'a' && digit <= 'f') ||
          (digit >= 'A' && digit <= 'F'));
}

HexColorEntry::HexColorEntry()
  : Box(JI_HORIZONTAL)
  , m_label("#")
  , m_entry(16, "")
{
  addChild(&m_label);
  addChild(&m_entry);

  m_entry.EntryChange.connect(&HexColorEntry::onEntryChange, this);

  initTheme();

  setBorder(gfx::Border(2*guiscale(), 0, 0, 0));
  child_spacing = 0;
}

void HexColorEntry::setColor(const app::Color& color)
{
  m_entry.setTextf("%02x%02x%02x",
                   color.getRed(),
                   color.getGreen(),
                   color.getBlue());
}

void HexColorEntry::onEntryChange()
{
  std::string text = m_entry.getText();
  int r, g, b;

  // Remove non hex digits
  while (text.size() > 0 && !is_hex_digit(text[0]))
    text.erase(0, 1);

  // Fill with zeros at the end of the text
  while (text.size() < 6)
    text.push_back('0');

  // Convert text (Base 16) to integer
  int hex = std::strtol(text.c_str(), NULL, 16);

  r = (hex & 0xff0000) >> 16;
  g = (hex & 0xff00) >> 8;
  b = (hex & 0xff);

  ColorChange(app::Color::fromRgb(r, g, b));
}

} // namespace app
