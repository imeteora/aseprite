// Aseprite
// Copyright (C) 2018  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifndef APP_DOC_DIFF_H_INCLUDED
#define APP_DOC_DIFF_H_INCLUDED
#pragma once

namespace app {
  class Doc;

  struct DocDiff {
    bool anything : 1;
    bool canvas : 1;
    bool totalFrames : 1;
    bool frameDuration : 1;
    bool frameTags : 1;
    bool palettes : 1;
    bool layers : 1;
    bool cels : 1;
    bool images : 1;

    DocDiff() :
      anything(false),
      canvas(false),
      totalFrames(false),
      frameDuration(false),
      frameTags(false),
      palettes(false),
      layers(false),
      cels(false),
      images(false) {
    }
  };

  // Useful for testing purposes to detect if two documents (after
  // some kind of operation) are equivalent.
  DocDiff compare_docs(const Doc* a,
                       const Doc* b);

} // namespace app

#endif
