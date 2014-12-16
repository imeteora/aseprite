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

#ifndef APP_UI_PALETTE_VIEW_H_INCLUDED
#define APP_UI_PALETTE_VIEW_H_INCLUDED
#pragma once

#include "base/connection.h"
#include "base/signal.h"
#include "ui/event.h"
#include "ui/mouse_buttons.h"
#include "ui/widget.h"

#include <vector>

namespace app {

  class PaletteIndexChangeEvent : public ui::Event {
  public:
    PaletteIndexChangeEvent(ui::Widget* source, int index, ui::MouseButtons buttons)
      : Event(source)
      , m_index(index)
      , m_buttons(buttons) { }

    int index() const { return m_index; }
    ui::MouseButtons buttons() const { return m_buttons; }

  private:
    int m_index;
    ui::MouseButtons m_buttons;
  };

  class PaletteView : public ui::Widget {
  public:
    typedef std::vector<bool> SelectedEntries;

    PaletteView(bool editable);

    int getColumns() const { return m_columns; }
    void setColumns(int columns);
    void setBoxSize(int boxsize);

    void clearSelection();
    void selectColor(int index);
    void selectRange(int index1, int index2);

    int getSelectedEntry() const;
    bool getSelectedRange(int& index1, int& index2) const;
    void getSelectedEntries(SelectedEntries& entries) const;

    app::Color getColorByPosition(int x, int y);

    // Signals
    Signal1<void, PaletteIndexChangeEvent&> IndexChange;

  protected:
    bool onProcessMessage(ui::Message* msg) override;
    void onPaint(ui::PaintEvent& ev) override;
    void onResize(ui::ResizeEvent& ev) override;
    void onPreferredSize(ui::PreferredSizeEvent& ev) override;

  private:
    void request_size(int* w, int* h);
    void update_scroll(int color);
    void onAppPaletteChange();

    bool m_editable;
    int m_columns;
    int m_boxsize;
    int m_currentEntry;
    int m_rangeAnchor;
    SelectedEntries m_selectedEntries;
    bool m_isUpdatingColumns;
    ScopedConnection m_conn;
  };

  ui::WidgetType palette_view_type();

} // namespace app

#endif
