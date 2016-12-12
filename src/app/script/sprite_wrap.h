// Aseprite
// Copyright (C) 2016  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_SCRIPT_SPRITE_WRAP_H_INCLUDED
#define APP_SCRIPT_SPRITE_WRAP_H_INCLUDED
#pragma once

#include "doc/object_id.h"

#include <map>

namespace doc {
  class Image;
  class Sprite;
}

namespace app {
  class Document;
  class DocumentView;
  class ImageWrap;
  class Transaction;

  class SpriteWrap {
    typedef std::map<doc::ObjectId, ImageWrap*> Images;

  public:
    SpriteWrap(app::Document* doc);
    ~SpriteWrap();

    void commit();
    void commitImages();
    Transaction& transaction();

    app::Document* document();
    doc::Sprite* sprite();
    ImageWrap* activeImage();

    ImageWrap* wrapImage(doc::Image* img);

  private:
    app::Document* m_doc;
    app::DocumentView* m_view;
    app::Transaction* m_transaction;

    Images m_images;
  };

} // namespace app

#endif
