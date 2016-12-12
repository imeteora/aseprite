// Aseprite Document Library
// Copyright (c) 2001-2015 David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifndef DOC_LAYER_LIST_H_INCLUDED
#define DOC_LAYER_LIST_H_INCLUDED
#pragma once

#include <vector>

namespace doc {

  class Layer;

  typedef std::vector<Layer*> LayerList;
  typedef LayerList::iterator LayerIterator;
  typedef LayerList::const_iterator LayerConstIterator;

} // namespace doc

#endif  // DOC_LAYER_LIST_H_INCLUDED
