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

#ifndef APP_UNDOERS_OPEN_GROUP_H_INCLUDED
#define APP_UNDOERS_OPEN_GROUP_H_INCLUDED
#pragma once

#include "doc/sprite_position.h"
#include "undo/object_id.h"
#include "undo/undoer.h"

namespace doc {
  class Layer;
}

namespace app {
  namespace undoers {
    using namespace doc;
    using namespace undo;

    class OpenGroup : public Undoer {
    public:
      OpenGroup(ObjectsContainer* objects,
                const char* label,
                Modification modification,
                Sprite* sprite,
                const SpritePosition& pos);

      void dispose() override;
      size_t getMemSize() const override { return sizeof(*this); }
      Modification getModification() const { return m_modification; }
      bool isOpenGroup() const override { return true; }
      bool isCloseGroup() const override { return false; }
      void revert(ObjectsContainer* objects, UndoersCollector* redoers) override;

      const SpritePosition& getSpritePosition() { return m_spritePosition; }

    private:
      const char* m_label;
      Modification m_modification;
      undo::ObjectId m_spriteId;
      SpritePosition m_spritePosition;
    };

  } // namespace undoers
} // namespace app

#endif  // UNDOERS_OPEN_GROUP_H_INCLUDED
