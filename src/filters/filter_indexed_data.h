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

#ifndef FILTERS_FILTER_INDEXED_DATA_H_INCLUDED
#define FILTERS_FILTER_INDEXED_DATA_H_INCLUDED
#pragma once

namespace doc {
  class Palette;
  class RgbMap;
}

namespace filters {

  // Provides a Palette and a RgbMap to help a Filter which operate
  // over an indexed image.
  class FilterIndexedData {
  public:
    virtual ~FilterIndexedData() { }
    virtual doc::Palette* getPalette() = 0;
    virtual doc::RgbMap* getRgbMap() = 0;
  };

} // namespace filters

#endif
