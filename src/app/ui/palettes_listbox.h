/* Aseprite
 * Copyright (C) 2014  David Capello
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

#ifndef APP_UI_PALETTES_LISTBOX_H_INCLUDED
#define APP_UI_PALETTES_LISTBOX_H_INCLUDED
#pragma once

#include "app/ui/resources_listbox.h"

namespace doc {
  class Palette;
}

namespace app {

  class PalettesListBox : public ResourcesListBox {
  public:
    PalettesListBox();

    doc::Palette* selectedPalette();

    Signal1<void, doc::Palette*> PalChange;

  protected:
    virtual void onResourceChange(Resource* resource) override;
    virtual void onPaintResource(ui::Graphics* g, const gfx::Rect& bounds, Resource* resource) override;
    virtual void onResourcePreferredSize(Resource* resource, gfx::Size& size) override;
  };

} // namespace app

#endif
