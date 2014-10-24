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

#ifndef APP_UI_TIMELINE_H_INCLUDED
#define APP_UI_TIMELINE_H_INCLUDED
#pragma once

#include "app/document_range.h"
#include "app/ui/editor/editor_observer.h"
#include "app/ui/skin/style.h"
#include "base/connection.h"
#include "doc/document_observer.h"
#include "doc/documents_observer.h"
#include "doc/frame_number.h"
#include "doc/layer_index.h"
#include "doc/sprite.h"
#include "ui/timer.h"
#include "ui/widget.h"

#include <vector>

namespace doc {
  class Cel;
  class Layer;
  class LayerImage;
  class Sprite;
}

namespace ui {
  class Graphics;
}

namespace app {
  using namespace doc;

  class Command;
  class ConfigureTimelinePopup;
  class Context;
  class Document;
  class Editor;

  class Timeline : public ui::Widget
                 , public doc::DocumentsObserver
                 , public doc::DocumentObserver
                 , public app::EditorObserver {
  public:
    typedef DocumentRange Range;

    enum State {
      STATE_STANDBY,
      STATE_SCROLLING,
      STATE_SELECTING_LAYERS,
      STATE_SELECTING_FRAMES,
      STATE_SELECTING_CELS,
      STATE_MOVING_SEPARATOR,
      STATE_MOVING_RANGE,
      STATE_MOVING_ONIONSKIN_RANGE_LEFT,
      STATE_MOVING_ONIONSKIN_RANGE_RIGHT
    };

    enum DropOp { kMove, kCopy };

    Timeline();
    ~Timeline();

    void updateUsingEditor(Editor* editor);

    Sprite* sprite() { return m_sprite; }
    Layer* getLayer() { return m_layer; }
    FrameNumber getFrame() { return m_frame; }

    void setLayer(Layer* layer);
    void setFrame(FrameNumber frame);

    State getState() const { return m_state; }
    bool isMovingCel() const;

    Range range() const { return m_range; }

    void activateClipboardRange();

    // Drag-and-drop operations. These actions are used by commands
    // called from popup menus.
    void dropRange(DropOp op);

  protected:
    bool onProcessMessage(ui::Message* msg) override;
    void onPreferredSize(ui::PreferredSizeEvent& ev) override;
    void onPaint(ui::PaintEvent& ev) override;

    // DocumentObserver impl.
    void onAddLayer(doc::DocumentEvent& ev) override;
    void onAfterRemoveLayer(doc::DocumentEvent& ev) override;
    void onAddFrame(doc::DocumentEvent& ev) override;
    void onRemoveFrame(doc::DocumentEvent& ev) override;

    // app::Context slots.
    void onAfterCommandExecution(Command* command);

    // DocumentsObserver impl.
    void onRemoveDocument(doc::Document* document) override;

    // EditorObserver impl.
    void onAfterFrameChanged(Editor* editor) override;
    void onAfterLayerChanged(Editor* editor) override;

  private:
    struct DropTarget {
      enum HHit { HNone, Before, After };
      enum VHit { VNone, Bottom, Top };

      DropTarget() {
        hhit = HNone;
        vhit = VNone;
      }

      HHit hhit;
      VHit vhit;
      Layer* layer;
      LayerIndex layerIdx;
      FrameNumber frame;
      int xpos, ypos;
    };

    bool allLayersVisible();
    bool allLayersInvisible();
    bool allLayersLocked();
    bool allLayersUnlocked();
    void detachDocument();
    void setCursor(ui::Message* msg, const gfx::Point& mousePos);
    void getDrawableLayers(ui::Graphics* g, LayerIndex* first_layer, LayerIndex* last_layer);
    void getDrawableFrames(ui::Graphics* g, FrameNumber* first_frame, FrameNumber* last_frame);
    void drawPart(ui::Graphics* g, const gfx::Rect& bounds,
      const char* text, skin::Style* style,
      bool is_active = false, bool is_hover = false, bool is_clicked = false);
    void drawHeader(ui::Graphics* g);
    void drawHeaderFrame(ui::Graphics* g, FrameNumber frame);
    void drawLayer(ui::Graphics* g, LayerIndex layerIdx);
    void drawCel(ui::Graphics* g, LayerIndex layerIdx, FrameNumber frame, Cel* cel);
    void drawLoopRange(ui::Graphics* g);
    void drawRangeOutline(ui::Graphics* g);
    void drawPaddings(ui::Graphics* g);
    bool drawPart(ui::Graphics* g, int part, LayerIndex layer, FrameNumber frame);
    void drawClipboardRange(ui::Graphics* g);
    gfx::Rect getLayerHeadersBounds() const;
    gfx::Rect getFrameHeadersBounds() const;
    gfx::Rect getOnionskinFramesBounds() const;
    gfx::Rect getCelsBounds() const;
    gfx::Rect getPartBounds(int part, LayerIndex layer = LayerIndex(0), FrameNumber frame = FrameNumber(0)) const;
    gfx::Rect getRangeBounds(const Range& range) const;
    void invalidatePart(int part, LayerIndex layer, FrameNumber frame);
    void regenerateLayers();
    void updateHotByMousePos(ui::Message* msg, const gfx::Point& mousePos);
    void updateHot(ui::Message* msg, const gfx::Point& mousePos, int& hot_part, LayerIndex& hot_layer, FrameNumber& hot_frame);
    void hotThis(int hot_part, LayerIndex hot_layer, FrameNumber hot_frame);
    void centerCel(LayerIndex layer, FrameNumber frame);
    void showCel(LayerIndex layer, FrameNumber frame);
    void showCurrentCel();
    void cleanClk();
    void setScroll(int x, int y);
    LayerIndex getLayerIndex(const Layer* layer) const;
    bool isLayerActive(LayerIndex layerIdx) const;
    bool isFrameActive(FrameNumber frame) const;
    void updateStatusBar(ui::Message* msg);
    void updateDropRange(const gfx::Point& pt);
    void clearClipboardRange();

    bool isCopyKeyPressed(ui::Message* msg);

    // The layer of the bottom (e.g. Background layer)
    LayerIndex firstLayer() const { return LayerIndex(0); }

    // The layer of the top.
    LayerIndex lastLayer() const { return LayerIndex(m_layers.size()-1); }

    FrameNumber firstFrame() const { return FrameNumber(0); }
    FrameNumber lastFrame() const { return m_sprite->lastFrame(); }

    bool validLayer(LayerIndex layer) const { return layer >= firstLayer() && layer <= lastLayer(); }
    bool validFrame(FrameNumber frame) const { return frame >= firstFrame() && frame <= lastFrame(); }

    skin::Style* m_timelineStyle;
    skin::Style* m_timelineBoxStyle;
    skin::Style* m_timelineOpenEyeStyle;
    skin::Style* m_timelineClosedEyeStyle;
    skin::Style* m_timelineOpenPadlockStyle;
    skin::Style* m_timelineClosedPadlockStyle;
    skin::Style* m_timelineLayerStyle;
    skin::Style* m_timelineEmptyFrameStyle;
    skin::Style* m_timelineKeyframeStyle;
    skin::Style* m_timelineFromLeftStyle;
    skin::Style* m_timelineFromRightStyle;
    skin::Style* m_timelineFromBothStyle;
    skin::Style* m_timelineGearStyle;
    skin::Style* m_timelineOnionskinStyle;
    skin::Style* m_timelineOnionskinRangeStyle;
    skin::Style* m_timelinePaddingStyle;
    skin::Style* m_timelinePaddingTrStyle;
    skin::Style* m_timelinePaddingBlStyle;
    skin::Style* m_timelinePaddingBrStyle;
    skin::Style* m_timelineSelectedCelStyle;
    skin::Style* m_timelineRangeOutlineStyle;
    skin::Style* m_timelineDropLayerDecoStyle;
    skin::Style* m_timelineDropFrameDecoStyle;
    skin::Style* m_timelineLoopRangeStyle;
    Context* m_context;
    Editor* m_editor;
    Document* m_document;
    Sprite* m_sprite;
    Layer* m_layer;
    FrameNumber m_frame;
    Range m_range;
    Range m_dropRange;
    State m_state;
    std::vector<Layer*> m_layers;
    int m_scroll_x;
    int m_scroll_y;
    int m_separator_x;
    int m_separator_w;
    int m_origFrames;
    // The 'hot' part is where the mouse is on top of
    int m_hot_part;
    LayerIndex m_hot_layer;
    FrameNumber m_hot_frame;
    DropTarget m_dropTarget;
    // The 'clk' part is where the mouse's button was pressed (maybe for a drag & drop operation)
    int m_clk_part;
    LayerIndex m_clk_layer;
    FrameNumber m_clk_frame;
    // Absolute mouse positions for scrolling.
    gfx::Point m_oldPos;
    // Configure timeline
    ConfigureTimelinePopup* m_confPopup;
    ScopedConnection m_ctxConn;

    // Marching ants stuff to show the range in the clipboard.
    // TODO merge this with the marching ants of the sprite editor (ui::Editor)
    ui::Timer m_clipboard_timer;
    int m_offset_count;

    bool m_scroll;   // True if the drag-and-drop operation is a scroll operation.
    bool m_copy;     // True if the drag-and-drop operation is a copy.
  };

} // namespace app

#endif
