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

#ifndef APP_TOOLS_TOOL_LOOP_MANAGER_H_INCLUDED
#define APP_TOOLS_TOOL_LOOP_MANAGER_H_INCLUDED
#pragma once

#include "gfx/point.h"
#include "gfx/region.h"
#include "ui/keys.h"

#include <vector>

namespace gfx { class Region; }

namespace app {
  namespace tools {

    class ToolLoop;

    // Class to manage the drawing tool (editor <-> tool interface).
    //
    // The flow is this:
    // 1. The user press a mouse button in a Editor widget
    // 2. The Editor creates an implementation of ToolLoop and use it
    //    with the ToolLoopManager constructor
    // 3. The ToolLoopManager is used to call
    //    the following methods:
    //    - ToolLoopManager::prepareLoop
    //    - ToolLoopManager::pressButton
    // 4. If the user moves the mouse, the method
    //    - ToolLoopManager::movement
    //    is called.
    // 5. When the user release the mouse:
    //    - ToolLoopManager::releaseButton
    //    - ToolLoopManager::releaseLoop
    class ToolLoopManager {
    public:

      // Simple container of mouse events information.
      class Pointer {
      public:
        enum Button { Left, Middle, Right };

        Pointer(int x, int y, Button button)
          : m_x(x), m_y(y), m_button(button) { }

        int x() const { return m_x; }
        int y() const { return m_y; }
        Button getButton() const { return m_button; }

      private:
        int m_x, m_y;
        Button m_button;
      };

      // Contructs a manager for the ToolLoop delegate.
      ToolLoopManager(ToolLoop* toolLoop);
      virtual ~ToolLoopManager();

      bool isCanceled() const;

      // Should be called when the user start a tool-trace (pressing the
      // left or right button for first time in the editor).
      void prepareLoop(const Pointer& pointer);

      // Called when the loop is over.
      void releaseLoop(const Pointer& pointer);

      void pressKey(ui::KeyScancode key);
      void releaseKey(ui::KeyScancode key);

      // Should be called each time the user presses a mouse button.
      void pressButton(const Pointer& pointer);

      // Should be called each time the user releases a mouse button.
      //
      // Returns true if the tool-loop should continue, or false
      // if the editor should release the mouse capture.
      bool releaseButton(const Pointer& pointer);

      // Should be called each time the user moves the mouse inside the editor.
      void movement(const Pointer& pointer);

    private:
      typedef std::vector<gfx::Point> Points;

      void doLoopStep(bool last_step);
      void snapToGrid(gfx::Point& point);

      void calculateDirtyArea(const Points& points);
      void calculateMinMax(const Points& points,
        gfx::Point& minpt,
        gfx::Point& maxpt);

      ToolLoop* m_toolLoop;
      Points m_points;
      gfx::Point m_oldPoint;
      gfx::Region& m_dirtyArea;
    };

  } // namespace tools
} // namespace app

#endif  // TOOLS_TOOL_H_INCLUDED
