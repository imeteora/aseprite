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

#include "app/document_range.h"

namespace app {

using namespace doc;

void DocumentRange::startRange(LayerIndex layer, frame_t frame, Type type)
{
  m_type = type;
  m_layerBegin = m_layerEnd = layer;
  m_frameBegin = m_frameEnd = frame;
}

void DocumentRange::endRange(LayerIndex layer, frame_t frame)
{
  ASSERT(enabled());
  m_layerEnd = layer;
  m_frameEnd = frame;
}

void DocumentRange::disableRange()
{
  m_type = kNone;
}

bool DocumentRange::inRange(LayerIndex layer) const
{
  if (enabled())
    return (layer >= layerBegin() && layer <= layerEnd());
  else
    return false;
}

bool DocumentRange::inRange(frame_t frame) const
{
  if (enabled())
    return (frame >= frameBegin() && frame <= frameEnd());
  else
    return false;
}

bool DocumentRange::inRange(LayerIndex layer, frame_t frame) const
{
  return inRange(layer) && inRange(frame);
}

void DocumentRange::setLayers(int layers)
{
  if (m_layerBegin <= m_layerEnd) m_layerEnd = m_layerBegin + LayerIndex(layers - 1);
  else m_layerBegin = m_layerEnd + LayerIndex(layers - 1);
}

void DocumentRange::setFrames(frame_t frames)
{
  if (m_frameBegin <= m_frameEnd)
    m_frameEnd = (m_frameBegin + frames) - 1;
  else
    m_frameBegin = (m_frameEnd + frames) - 1;
}

void DocumentRange::displace(int layerDelta, int frameDelta)
{
  m_layerBegin += LayerIndex(layerDelta);
  m_layerEnd   += LayerIndex(layerDelta);
  m_frameBegin += frame_t(frameDelta);
  m_frameEnd   += frame_t(frameDelta);
}

} // namespace app
