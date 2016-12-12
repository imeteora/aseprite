// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_RES_RESOURCE_H_INCLUDED
#define APP_RES_RESOURCE_H_INCLUDED
#pragma once

namespace app {

  class Resource {
  public:
    virtual ~Resource() { }
    virtual const std::string& name() const = 0;
  };

} // namespace app

#endif
