// Aseprite
// Copyright (C) 2001-2016  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_CMD_ADD_FRAME_TAG_H_INCLUDED
#define APP_CMD_ADD_FRAME_TAG_H_INCLUDED
#pragma once

#include "app/cmd.h"
#include "app/cmd/with_frame_tag.h"
#include "app/cmd/with_sprite.h"

#include <sstream>

namespace app {
namespace cmd {
  using namespace doc;

  class AddFrameTag : public Cmd
                    , public WithSprite
                    , public WithFrameTag {
  public:
    AddFrameTag(Sprite* sprite, FrameTag* tag);

  protected:
    void onExecute() override;
    void onUndo() override;
    void onRedo() override;
    size_t onMemSize() const override {
      return sizeof(*this) + m_size;
    }

  private:
    size_t m_size;
    std::stringstream m_stream;
  };

} // namespace cmd
} // namespace app

#endif
