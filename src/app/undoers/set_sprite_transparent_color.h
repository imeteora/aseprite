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

#ifndef APP_UNDOERS_SET_SPRITE_TRANSPARENT_COLOR_H_INCLUDED
#define APP_UNDOERS_SET_SPRITE_TRANSPARENT_COLOR_H_INCLUDED
#pragma once

#include "app/undoers/undoer_base.h"
#include "undo/object_id.h"

namespace doc {
  class Sprite;
}

namespace app {
  namespace undoers {
    using namespace doc;
    using namespace undo;

    class SetSpriteTransparentColor : public UndoerBase {
    public:
      SetSpriteTransparentColor(ObjectsContainer* objects, Sprite* sprite);

      void dispose() override;
      size_t getMemSize() const override { return sizeof(*this); }
      void revert(ObjectsContainer* objects, UndoersCollector* redoers) override;

    private:
      undo::ObjectId m_spriteId;
      uint32_t m_maskColor;
    };

  } // namespace undoers
} // namespace app

#endif  // UNDOERS_SET_SPRITE_TRANSPARENT_COLOR_H_INCLUDED
