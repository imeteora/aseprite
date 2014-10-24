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

#include "app/undoers/set_mask.h"

#include "app/document.h"
#include "base/unique_ptr.h"
#include "doc/mask.h"
#include "doc/mask_io.h"
#include "undo/objects_container.h"
#include "undo/undoers_collector.h"

namespace app {
namespace undoers {

using namespace undo;

SetMask::SetMask(ObjectsContainer* objects, Document* document)
  : m_documentId(objects->addObject(document))
  , m_isMaskVisible(document->isMaskVisible())
{
  if (m_isMaskVisible)
    doc::write_mask(m_stream, document->mask());
}

void SetMask::dispose()
{
  delete this;
}

void SetMask::revert(ObjectsContainer* objects, UndoersCollector* redoers)
{
  Document* document = objects->getObjectT<Document>(m_documentId);

  // Push another SetMask as redoer
  redoers->pushUndoer(new SetMask(objects, document));

  if (m_isMaskVisible) {
    base::UniquePtr<Mask> mask(doc::read_mask(m_stream));

    document->setMask(mask);

    // Document::setMask() makes a copy of the mask, so here the mask
    // is deleted with the base::UniquePtr.
  }
  else
    document->setMaskVisible(false);
}

} // namespace undoers
} // namespace app
