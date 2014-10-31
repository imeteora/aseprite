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

#include "tests/test.h"

#include "app/document.h"
#include "app/document_api.h"
#include "app/document_range.h"
#include "app/document_range_ops.h"
#include "app/document_undo.h"
#include "app/test_context.h"
#include "base/unique_ptr.h"
#include "doc/doc.h"
#include "undo/undo_history.h"

using namespace app;
using namespace doc;

namespace app {

  std::ostream& operator<<(std::ostream& os, const DocumentRange& range) {
    return os << "{ layers: [" << range.layerBegin() << ", " << range.layerEnd() << "]"
              << ", frames: [" << range.frameBegin() << ", " << range.frameEnd() << "] }";
  }

}

typedef base::UniquePtr<app::Document> DocumentPtr;

#define EXPECT_LAYER_ORDER(a, b, c, d) \
  EXPECT_TRUE(expect_layer(a, 0));     \
  EXPECT_TRUE(expect_layer(b, 1));     \
  EXPECT_TRUE(expect_layer(c, 2));     \
  EXPECT_TRUE(expect_layer(d, 3));

#define EXPECT_FRAME_ORDER(a, b, c, d) \
  EXPECT_TRUE(expect_frame(a, 0));     \
  EXPECT_TRUE(expect_frame(b, 1));     \
  EXPECT_TRUE(expect_frame(c, 2));     \
  EXPECT_TRUE(expect_frame(d, 3));

#define EXPECT_FRAME_COPY1(a, b, c, d, e) \
  EXPECT_TRUE(expect_frame(a, 0));        \
  EXPECT_TRUE(expect_frame(b, 1));        \
  EXPECT_TRUE(expect_frame(c, 2));        \
  EXPECT_TRUE(expect_frame(d, 3));        \
  EXPECT_TRUE(expect_frame(e, 4));

#define EXPECT_FRAME_COPY2(a, b, c, d, e, f)    \
  EXPECT_TRUE(expect_frame(a, 0));              \
  EXPECT_TRUE(expect_frame(b, 1));              \
  EXPECT_TRUE(expect_frame(c, 2));              \
  EXPECT_TRUE(expect_frame(d, 3));              \
  EXPECT_TRUE(expect_frame(e, 4));              \
  EXPECT_TRUE(expect_frame(f, 5));

#define EXPECT_FRAME_COPY3(a, b, c, d, e, f, g) \
  EXPECT_TRUE(expect_frame(a, 0));              \
  EXPECT_TRUE(expect_frame(b, 1));              \
  EXPECT_TRUE(expect_frame(c, 2));              \
  EXPECT_TRUE(expect_frame(d, 3));              \
  EXPECT_TRUE(expect_frame(e, 4));              \
  EXPECT_TRUE(expect_frame(f, 5));              \
  EXPECT_TRUE(expect_frame(g, 6));

class DocRangeOps : public ::testing::Test {
public:
  DocRangeOps() {
    black = rgba(0, 0, 0, 0);
    white = rgba(255, 255, 255, 255);

    doc.reset(static_cast<app::Document*>(ctx.documents().add(4, 4)));
    sprite = doc->sprite();
    layer1 = dynamic_cast<LayerImage*>(sprite->folder()->getFirstLayer());
    layer2 = new LayerImage(sprite);
    layer3 = new LayerImage(sprite);
    layer4 = new LayerImage(sprite);
    sprite->folder()->addLayer(layer2);
    sprite->folder()->addLayer(layer3);
    sprite->folder()->addLayer(layer4);
    EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);

    layer1->setName("layer1");
    layer2->setName("layer2");
    layer3->setName("layer3");
    layer4->setName("layer4");

    sprite->setTotalFrames(FrameNumber(4));
    sprite->setFrameDuration(FrameNumber(0), 1); // These durations can be used to identify
    sprite->setFrameDuration(FrameNumber(1), 2); // frames after a move operation
    sprite->setFrameDuration(FrameNumber(2), 3);
    sprite->setFrameDuration(FrameNumber(3), 4);

    DocumentApi api = doc->getApi();
    for (int i=0; i<4; i++) {
      LayerImage* layer = static_cast<LayerImage*>(sprite->indexToLayer(LayerIndex(i)));

      for (int j=0; j<4; j++) {
        Cel* cel = layer->getCel(FrameNumber(j));
        Image* image;
        if (cel)
          image = cel->image();
        else {
          image = Image::create(IMAGE_RGB, 4, 4);
          int imageIdx = sprite->stock()->addImage(image);
          cel = new Cel(FrameNumber(j), imageIdx);
          layer->addCel(cel);
        }

        clear_image(image, black);
        put_pixel(image, i, j, white);
      }
    }
  }

  ~DocRangeOps() {
    doc->close();
  }

protected:
  bool expect_layer(Layer* expected_layer, int layer) {
    return expect_layer_frame(sprite->layerToIndex(expected_layer), -1, layer, -1);
  }

  bool expect_frame(int expected_frame, int frame) {
    for (int i=0; i<(int)sprite->countLayers(); ++i) {
      if (!expect_layer_frame(i, expected_frame, i, frame))
        return false;
    }
    return true;
  }

  bool expect_layer_frame(int expected_layer, int expected_frame, int layer, int frame) {
    if (frame >= 0) {
      color_t expected_color = white;

      color_t color = get_pixel(
        static_cast<LayerImage*>(sprite->indexToLayer(LayerIndex(layer)))
          ->getCel(FrameNumber(frame))->image(),
        expected_layer, expected_frame);

      EXPECT_EQ(expected_color, color);
      EXPECT_EQ((expected_frame+1), sprite->getFrameDuration(FrameNumber(frame)));

      return
        (expected_color == color
          && (expected_frame+1) == sprite->getFrameDuration(FrameNumber(frame)));
    }

    if (layer >= 0) {
      Layer* a = sprite->indexToLayer(LayerIndex(expected_layer));
      Layer* b = sprite->indexToLayer(LayerIndex(layer));
      EXPECT_EQ(a, b);
      if (a != b)
        return false;
    }

    return true;
  }

  TestContext ctx;
  DocumentPtr doc;
  Sprite* sprite;
  LayerImage* layer1;
  LayerImage* layer2;
  LayerImage* layer3;
  LayerImage* layer4;
  color_t black;
  color_t white;
};

inline DocumentRange range(Layer* fromLayer, int fromFrNum, Layer* toLayer, int toFrNum, DocumentRange::Type type) {
  DocumentRange r;
  r.startRange(fromLayer->sprite()->layerToIndex(fromLayer), FrameNumber(fromFrNum), type);
  r.endRange(toLayer->sprite()->layerToIndex(toLayer), FrameNumber(toFrNum));
  return r;
}

inline DocumentRange range(int fromLayer, int fromFrNum, int toLayer, int toFrNum, DocumentRange::Type type) {
  DocumentRange r;
  r.startRange(LayerIndex(fromLayer), FrameNumber(fromFrNum), type);
  r.endRange(LayerIndex(toLayer), FrameNumber(toFrNum));
  return r;
}

inline DocumentRange layers_range(Layer* fromLayer, Layer* toLayer) {
  return range(fromLayer, -1, toLayer, -1, DocumentRange::kLayers);
}

inline DocumentRange layers_range(int fromLayer, int toLayer) {
  return range(fromLayer, -1, toLayer, -1, DocumentRange::kLayers);
}

inline DocumentRange layers_range(Layer* layer) {
  return range(layer, -1, layer, -1, DocumentRange::kLayers);
}

inline DocumentRange layers_range(int layer) {
  return range(layer, -1, layer, -1, DocumentRange::kLayers);
}

inline DocumentRange frames_range(int fromFrame, int toFrame) {
  return range(0, fromFrame, 0, toFrame, DocumentRange::kFrames);
}

inline DocumentRange frames_range(int frame) {
  return range(0, frame, 0, frame, DocumentRange::kFrames);
}

TEST_F(DocRangeOps, MoveLayersNoOp) {
  // Move one layer to the same place

  EXPECT_EQ(layers_range(layer1),
    move_range(doc,
      layers_range(layer1),
      layers_range(layer1), kDocumentRangeAfter));
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(layers_range(layer1),
    move_range(doc,
      layers_range(layer1),
      layers_range(layer2), kDocumentRangeBefore));
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(layers_range(layer4),
    move_range(doc,
      layers_range(layer4),
      layers_range(layer4), kDocumentRangeAfter));
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(layers_range(layer4),
    move_range(doc,
      layers_range(layer4),
      layers_range(layer4), kDocumentRangeBefore));
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(layers_range(layer4),
    move_range(doc,
      layers_range(layer4),
      layers_range(layer3), kDocumentRangeAfter));
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  // Move two layer to the same place

  EXPECT_EQ(layers_range(layer1, layer2),
    move_range(doc,
      layers_range(layer1, layer2),
      layers_range(layer1), kDocumentRangeBefore));
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(layers_range(layer1, layer2),
    move_range(doc,
      layers_range(layer1, layer2),
      layers_range(layer1), kDocumentRangeAfter));
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(layers_range(layer1, layer2),
    move_range(doc,
      layers_range(layer1, layer2),
      layers_range(layer2), kDocumentRangeBefore));
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(layers_range(layer1, layer2),
    move_range(doc,
      layers_range(layer1, layer2),
      layers_range(layer2), kDocumentRangeAfter));
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(layers_range(layer1, layer2),
    move_range(doc,
      layers_range(layer1, layer2),
      layers_range(layer3), kDocumentRangeBefore));
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(layers_range(layer3, layer4),
    move_range(doc,
      layers_range(layer3, layer4),
      layers_range(layer2), kDocumentRangeAfter));
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(layers_range(layer3, layer4),
    move_range(doc,
      layers_range(layer3, layer4),
      layers_range(layer3), kDocumentRangeBefore));
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(layers_range(layer3, layer4),
    move_range(doc,
      layers_range(layer3, layer4),
      layers_range(layer3), kDocumentRangeAfter));
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(layers_range(layer3, layer4),
    move_range(doc,
      layers_range(layer3, layer4),
      layers_range(layer4), kDocumentRangeBefore));
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(layers_range(layer3, layer4),
    move_range(doc,
      layers_range(layer3, layer4),
      layers_range(layer4), kDocumentRangeAfter));
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  // Move four layers

  DocumentRangePlace places[] = { kDocumentRangeBefore, kDocumentRangeAfter };
  for (int i=0; i<2; ++i) {
    for (int layer=0; layer<4; ++layer) {
      EXPECT_EQ(layers_range(layer1, layer4),
        move_range(doc,
          layers_range(layer1, layer4),
          layers_range(layer), places[i]));
      EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);
      EXPECT_FALSE(doc->getUndo()->canUndo());
    }

    for (int layer=0; layer<3; ++layer) {
      EXPECT_EQ(layers_range(layer1, layer4),
        move_range(doc,
          layers_range(layer1, layer4),
          layers_range(layer, layer+1), places[i]));
      EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);
      EXPECT_FALSE(doc->getUndo()->canUndo());
    }

    for (int layer=0; layer<2; ++layer) {
      EXPECT_EQ(layers_range(layer1, layer4),
        move_range(doc,
          layers_range(layer1, layer4),
          layers_range(layer, layer+2), places[i]));
      EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);
      EXPECT_FALSE(doc->getUndo()->canUndo());
    }

    EXPECT_EQ(layers_range(layer1, layer4),
      move_range(doc,
        layers_range(layer1, layer4),
        layers_range(layer1, layer4), places[i]));
    EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);
    EXPECT_FALSE(doc->getUndo()->canUndo());
  }
}

TEST_F(DocRangeOps, MoveFramesNoOp) {
  // Move one frame to the same place

  EXPECT_EQ(frames_range(0),
    move_range(doc,
      frames_range(0),
      frames_range(0), kDocumentRangeAfter));
  EXPECT_FRAME_ORDER(0, 1, 2, 3);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(frames_range(0),
    move_range(doc,
      frames_range(0),
      frames_range(1), kDocumentRangeBefore));
  EXPECT_FRAME_ORDER(0, 1, 2, 3);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(frames_range(3),
    move_range(doc,
      frames_range(3),
      frames_range(3), kDocumentRangeAfter));
  EXPECT_FRAME_ORDER(0, 1, 2, 3);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(frames_range(3),
    move_range(doc,
      frames_range(3),
      frames_range(3), kDocumentRangeBefore));
  EXPECT_FRAME_ORDER(0, 1, 2, 3);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(frames_range(3),
    move_range(doc,
      frames_range(3),
      frames_range(2), kDocumentRangeAfter));
  EXPECT_FRAME_ORDER(0, 1, 2, 3);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  // Move two frame to the same place

  EXPECT_EQ(frames_range(0, 1),
    move_range(doc,
      frames_range(0, 1),
      frames_range(0), kDocumentRangeBefore));
  EXPECT_FRAME_ORDER(0, 1, 2, 3);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(frames_range(0, 1),
    move_range(doc,
      frames_range(0, 1),
      frames_range(0), kDocumentRangeAfter));
  EXPECT_FRAME_ORDER(0, 1, 2, 3);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(frames_range(0, 1),
    move_range(doc,
      frames_range(0, 1),
      frames_range(1), kDocumentRangeBefore));
  EXPECT_FRAME_ORDER(0, 1, 2, 3);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(frames_range(0, 1),
    move_range(doc,
      frames_range(0, 1),
      frames_range(1), kDocumentRangeAfter));
  EXPECT_FRAME_ORDER(0, 1, 2, 3);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(frames_range(0, 1),
    move_range(doc,
      frames_range(0, 1),
      frames_range(2), kDocumentRangeBefore));
  EXPECT_FRAME_ORDER(0, 1, 2, 3);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(frames_range(2, 3),
    move_range(doc,
      frames_range(2, 3),
      frames_range(1), kDocumentRangeAfter));
  EXPECT_FRAME_ORDER(0, 1, 2, 3);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(frames_range(2, 3),
    move_range(doc,
      frames_range(2, 3),
      frames_range(2), kDocumentRangeBefore));
  EXPECT_FRAME_ORDER(0, 1, 2, 3);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(frames_range(2, 3),
    move_range(doc,
      frames_range(2, 3),
      frames_range(2), kDocumentRangeAfter));
  EXPECT_FRAME_ORDER(0, 1, 2, 3);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(frames_range(2, 3),
    move_range(doc,
      frames_range(2, 3),
      frames_range(3), kDocumentRangeBefore));
  EXPECT_FRAME_ORDER(0, 1, 2, 3);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  EXPECT_EQ(frames_range(2, 3),
    move_range(doc,
      frames_range(2, 3),
      frames_range(3), kDocumentRangeAfter));
  EXPECT_FRAME_ORDER(0, 1, 2, 3);
  EXPECT_FALSE(doc->getUndo()->canUndo());

  // Move four frames

  DocumentRangePlace places[] = { kDocumentRangeBefore, kDocumentRangeAfter };
  for (int i=0; i<2; ++i) {
    for (int frame=0; frame<4; ++frame) {
      EXPECT_EQ(frames_range(0, 3),
        move_range(doc,
          frames_range(0, 3),
          frames_range(frame), places[i]));
      EXPECT_FRAME_ORDER(0, 1, 2, 3);
      EXPECT_FALSE(doc->getUndo()->canUndo());
    }

    for (int frame=0; frame<3; ++frame) {
      EXPECT_EQ(frames_range(0, 3),
        move_range(doc,
          frames_range(0, 3),
          frames_range(frame, frame+1), places[i]));
      EXPECT_FRAME_ORDER(0, 1, 2, 3);
      EXPECT_FALSE(doc->getUndo()->canUndo());
    }

    for (int frame=0; frame<2; ++frame) {
      EXPECT_EQ(frames_range(0, 3),
        move_range(doc,
          frames_range(0, 3),
          frames_range(frame, frame+2), places[i]));
      EXPECT_FRAME_ORDER(0, 1, 2, 3);
      EXPECT_FALSE(doc->getUndo()->canUndo());
    }

    EXPECT_EQ(frames_range(0, 3),
      move_range(doc,
        frames_range(0, 3),
        frames_range(0, 3), places[i]));
    EXPECT_FRAME_ORDER(0, 1, 2, 3);
    EXPECT_FALSE(doc->getUndo()->canUndo());
  }
}

TEST_F(DocRangeOps, MoveCelsNoOp) {
  // TODO
}

TEST_F(DocRangeOps, CopyCelsNoOp) {
  // TODO
}

TEST_F(DocRangeOps, MoveLayers) {
  DocumentRange result;

  // One layer at the bottom of another
  result = move_range(doc,
    layers_range(layer1),
    layers_range(layer2), kDocumentRangeAfter);
  EXPECT_LAYER_ORDER(layer2, layer1, layer3, layer4);
  EXPECT_EQ(layers_range(layer1), result);

  doc->getUndo()->doUndo();
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);

  // One layer at the bottom
  result = move_range(doc,
    layers_range(layer2),
    layers_range(layer1), kDocumentRangeBefore);
  EXPECT_LAYER_ORDER(layer2, layer1, layer3, layer4);
  EXPECT_EQ(layers_range(layer2), result);

  doc->getUndo()->doUndo();
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);

  // Try with a background
  layer1->setBackground(true);
  EXPECT_ANY_THROW({
      move_range(doc,
        layers_range(layer1),
        layers_range(layer2), kDocumentRangeAfter);
    });
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);
  layer1->setBackground(false);

  // Move one layer to the top
  result = move_range(doc,
    layers_range(layer2),
    layers_range(layer4), kDocumentRangeAfter);
  EXPECT_LAYER_ORDER(layer1, layer3, layer4, layer2);
  EXPECT_EQ(layers_range(layer2), result);

  doc->getUndo()->doUndo();
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);

  // Move one layers before other
  result = move_range(doc,
    layers_range(layer2),
    layers_range(layer4), kDocumentRangeBefore);
  EXPECT_LAYER_ORDER(layer1, layer3, layer2, layer4);
  EXPECT_EQ(layers_range(layer2), result);

  doc->getUndo()->doUndo();
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);

  result = move_range(doc,
    layers_range(layer1),
    layers_range(layer3, layer4), kDocumentRangeBefore);
  EXPECT_LAYER_ORDER(layer2, layer1, layer3, layer4);
  EXPECT_EQ(layers_range(layer1), result);

  doc->getUndo()->doUndo();
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);

  // Move two layers on top of other
  result = move_range(doc,
    layers_range(layer2, layer3),
    layers_range(layer4), kDocumentRangeAfter);
  EXPECT_LAYER_ORDER(layer1, layer4, layer2, layer3);
  EXPECT_EQ(layers_range(layer2, layer3), result);

  result = move_range(doc,
    layers_range(layer2, layer3),
    layers_range(layer1), kDocumentRangeAfter);
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);
  EXPECT_EQ(layers_range(layer2, layer3), result);

  // Move three layers at the bottom (but we cannot because the bottom is a background layer)
  layer1->setBackground(true);
  EXPECT_ANY_THROW({
      move_range(doc,
        layers_range(layer2, layer4),
        layers_range(layer1), kDocumentRangeBefore);
    });
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);
  layer1->setBackground(false);

  // Move three layers at the top
  result = move_range(doc,
    layers_range(layer1, layer3),
    layers_range(layer4), kDocumentRangeAfter);
  EXPECT_LAYER_ORDER(layer4, layer1, layer2, layer3);
  EXPECT_EQ(layers_range(layer1, layer3), result);

  doc->getUndo()->doUndo();
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);

  // Move three layers at the bottom
  result = move_range(doc,
    layers_range(layer2, layer4),
    layers_range(layer1), kDocumentRangeBefore);
  EXPECT_LAYER_ORDER(layer2, layer3, layer4, layer1);
  EXPECT_EQ(layers_range(layer2, layer4), result);

  doc->getUndo()->doUndo();
  EXPECT_LAYER_ORDER(layer1, layer2, layer3, layer4);
}

TEST_F(DocRangeOps, MoveFrames) {
  move_range(doc,
    frames_range(0, 0),
    frames_range(1, 1), kDocumentRangeAfter);
  EXPECT_FRAME_ORDER(1, 0, 2, 3);

  doc->getUndo()->doUndo();
  EXPECT_FRAME_ORDER(0, 1, 2, 3);

  // Move one frame at the end
  move_range(doc,
    frames_range(1, 1),
    frames_range(3, 3), kDocumentRangeAfter);
  EXPECT_FRAME_ORDER(0, 2, 3, 1);

  doc->getUndo()->doUndo();
  EXPECT_FRAME_ORDER(0, 1, 2, 3);

  // Move two frames after other
  move_range(doc,
    frames_range(1, 2),
    frames_range(3, 3), kDocumentRangeAfter);
  EXPECT_FRAME_ORDER(0, 3, 1, 2);

  doc->getUndo()->doUndo();
  EXPECT_FRAME_ORDER(0, 1, 2, 3);

  move_range(doc,
    frames_range(1, 2),
    frames_range(0, 0), kDocumentRangeAfter);
  EXPECT_FRAME_ORDER(0, 1, 2, 3);

  // Move three frames at the beginning
  move_range(doc,
    frames_range(1, 3),
    frames_range(0, 0), kDocumentRangeBefore);
  EXPECT_FRAME_ORDER(1, 2, 3, 0);

  doc->getUndo()->doUndo();
  EXPECT_FRAME_ORDER(0, 1, 2, 3);
}

TEST_F(DocRangeOps, MoveCels) {
  // TODO
}

TEST_F(DocRangeOps, CopyLayers) {
  // TODO
}

TEST_F(DocRangeOps, CopyFrames) {
  // Copy one frame
  copy_range(doc,
    frames_range(0),
    frames_range(2, 3), kDocumentRangeBefore);
  EXPECT_FRAME_COPY1(0, 1, 0, 2, 3);
  doc->getUndo()->doUndo();
  EXPECT_FRAME_ORDER(0, 1, 2, 3);

  copy_range(doc,
    frames_range(0),
    frames_range(2, 3), kDocumentRangeAfter);
  EXPECT_FRAME_COPY1(0, 1, 2, 3, 0);
  doc->getUndo()->doUndo();
  EXPECT_FRAME_ORDER(0, 1, 2, 3);

  copy_range(doc,
    frames_range(3),
    frames_range(0, 1), kDocumentRangeBefore);
  EXPECT_FRAME_COPY1(3, 0, 1, 2, 3);
  doc->getUndo()->doUndo();
  EXPECT_FRAME_ORDER(0, 1, 2, 3);

  copy_range(doc,
    frames_range(3),
    frames_range(0, 1), kDocumentRangeAfter);
  EXPECT_FRAME_COPY1(0, 1, 3, 2, 3);
  doc->getUndo()->doUndo();
  EXPECT_FRAME_ORDER(0, 1, 2, 3);

  // Copy three frames

  copy_range(doc,
    frames_range(0, 2),
    frames_range(3), kDocumentRangeBefore);
  EXPECT_FRAME_COPY3(0, 1, 2, 0, 1, 2, 3);
  doc->getUndo()->doUndo();
  EXPECT_FRAME_ORDER(0, 1, 2, 3);

  copy_range(doc,
    frames_range(0, 2),
    frames_range(3), kDocumentRangeAfter);
  EXPECT_FRAME_COPY3(0, 1, 2, 3, 0, 1, 2);
  doc->getUndo()->doUndo();
  EXPECT_FRAME_ORDER(0, 1, 2, 3);

  copy_range(doc,
    frames_range(1, 3),
    frames_range(0), kDocumentRangeBefore);
  EXPECT_FRAME_COPY3(1, 2, 3, 0, 1, 2, 3);
  doc->getUndo()->doUndo();
  EXPECT_FRAME_ORDER(0, 1, 2, 3);

  copy_range(doc,
    frames_range(1, 3),
    frames_range(0), kDocumentRangeAfter);
  EXPECT_FRAME_COPY3(0, 1, 2, 3, 1, 2, 3);
  doc->getUndo()->doUndo();
  EXPECT_FRAME_ORDER(0, 1, 2, 3);

  copy_range(doc,
    frames_range(0, 2),
    frames_range(0, 2), kDocumentRangeBefore);
  EXPECT_FRAME_COPY3(0, 1, 2, 0, 1, 2, 3);
  doc->getUndo()->doUndo();
  EXPECT_FRAME_ORDER(0, 1, 2, 3);

  copy_range(doc,
    frames_range(0, 2),
    frames_range(0, 2), kDocumentRangeAfter);
  EXPECT_FRAME_COPY3(0, 1, 2, 0, 1, 2, 3);
  doc->getUndo()->doUndo();
  EXPECT_FRAME_ORDER(0, 1, 2, 3);
}

TEST_F(DocRangeOps, CopyCels) {
  // TODO
}

TEST_F(DocRangeOps, ReverseFrames) {
  reverse_frames(doc, frames_range(0, 0));
  EXPECT_FRAME_ORDER(0, 1, 2, 3);

  reverse_frames(doc, frames_range(1, 1));
  EXPECT_FRAME_ORDER(0, 1, 2, 3);

  reverse_frames(doc, frames_range(1, 2));
  EXPECT_FRAME_ORDER(0, 2, 1, 3);
  doc->getUndo()->doUndo();
  EXPECT_FRAME_ORDER(0, 1, 2, 3);

  reverse_frames(doc, frames_range(0, 2));
  EXPECT_FRAME_ORDER(2, 1, 0, 3);
  doc->getUndo()->doUndo();
  EXPECT_FRAME_ORDER(0, 1, 2, 3);

  reverse_frames(doc, frames_range(1, 3));
  EXPECT_FRAME_ORDER(0, 3, 2, 1);
  doc->getUndo()->doUndo();
  EXPECT_FRAME_ORDER(0, 1, 2, 3);

  reverse_frames(doc, frames_range(0, 3));
  EXPECT_FRAME_ORDER(3, 2, 1, 0);
  doc->getUndo()->doUndo();
  EXPECT_FRAME_ORDER(0, 1, 2, 3);
}

TEST_F(DocRangeOps, ReverseCels) {
  // TODO
}
