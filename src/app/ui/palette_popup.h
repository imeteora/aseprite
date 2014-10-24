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

#ifndef APP_UI_PALETTE_POPUP_H_INCLUDED
#define APP_UI_PALETTE_POPUP_H_INCLUDED
#pragma once

#include "app/ui/palettes_listbox.h"
#include "ui/popup_window.h"

namespace ui {
  class Button;
  class View;
}

namespace app {

  namespace gen {
    class PalettePopup;
  }

  class PalettePopup : public ui::PopupWindow {
  public:
    PalettePopup();

    void showPopup(const gfx::Rect& bounds);

  protected:
    void onPalChange(doc::Palette* palette);
    void onLoadPal(bool asDefault);
    void onOpenFolder();

  private:
    gen::PalettePopup* m_popup;
    PalettesListBox m_paletteListBox;
  };

} // namespace app

#endif
