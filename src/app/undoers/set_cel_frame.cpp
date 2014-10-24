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

#include "app/undoers/set_cel_frame.h"

#include "doc/cel.h"
#include "doc/layer.h"
#include "undo/objects_container.h"
#include "undo/undoers_collector.h"

namespace app {
namespace undoers {

using namespace undo;

SetCelFrame::SetCelFrame(ObjectsContainer* objects, LayerImage* layer, Cel* cel)
  : m_layerId(objects->addObject(layer))
  , m_celId(objects->addObject(cel))
  , m_frame(cel->frame())
{
}

void SetCelFrame::dispose()
{
  delete this;
}

void SetCelFrame::revert(ObjectsContainer* objects, UndoersCollector* redoers)
{
  LayerImage* layer = objects->getObjectT<LayerImage>(m_layerId);
  Cel* cel = objects->getObjectT<Cel>(m_celId);

  // Push another SetCelFrame as redoer
  redoers->pushUndoer(new SetCelFrame(objects, layer, cel));

  layer->moveCel(cel, m_frame);
}

} // namespace undoers
} // namespace app
