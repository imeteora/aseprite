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

#include "app/commands/command.h"
#include "app/context_access.h"
#include "app/document_api.h"
#include "app/find_widget.h"
#include "app/load_widget.h"
#include "app/modules/editors.h"
#include "app/modules/gui.h"
#include "app/settings/settings.h"
#include "app/ui/button_set.h"
#include "app/ui/color_bar.h"
#include "app/ui/editor/editor.h"
#include "app/ui/editor/select_box_state.h"
#include "app/ui/skin/skin_theme.h"
#include "app/undo_transaction.h"
#include "base/bind.h"
#include "base/unique_ptr.h"
#include "doc/image.h"
#include "doc/mask.h"
#include "doc/sprite.h"
#include "ui/ui.h"

#include "generated_canvas_size.h"

namespace app {

using namespace ui;
using namespace app::skin;

// Disable warning about usage of "this" in initializer list.
#ifdef _MSC_VER
#pragma warning(disable:4355)
#endif

// Window used to show canvas parameters.
class CanvasSizeWindow : public app::gen::CanvasSize
                       , public SelectBoxDelegate
{
public:
  enum class Dir { NW, N, NE, W, C, E, SW, S, SE };

  CanvasSizeWindow()
    : m_editor(current_editor)
    , m_rect(0, 0, current_editor->sprite()->width(), current_editor->sprite()->height())
    , m_selectBoxState(new SelectBoxState(this, m_rect,
        SelectBoxState::PaintRulers |
        SelectBoxState::PaintDarkOutside)) {
    setWidth(m_rect.w);
    setHeight(m_rect.h);
    setLeft(0);
    setRight(0);
    setTop(0);
    setBottom(0);

    width() ->EntryChange.connect(Bind<void>(&CanvasSizeWindow::onSizeChange, this));
    height()->EntryChange.connect(Bind<void>(&CanvasSizeWindow::onSizeChange, this));
    dir()   ->ItemChange.connect(Bind<void>(&CanvasSizeWindow::onDirChange, this));;
    left()  ->EntryChange.connect(Bind<void>(&CanvasSizeWindow::onBorderChange, this));
    right() ->EntryChange.connect(Bind<void>(&CanvasSizeWindow::onBorderChange, this));
    top()   ->EntryChange.connect(Bind<void>(&CanvasSizeWindow::onBorderChange, this));
    bottom()->EntryChange.connect(Bind<void>(&CanvasSizeWindow::onBorderChange, this));

    m_editor->setState(m_selectBoxState);

    dir()->setSelectedItem((int)Dir::C);
    updateIcons();
  }

  ~CanvasSizeWindow() {
    m_editor->backToPreviousState();
  }

  bool pressedOk() { return getKiller() == ok(); }

  int getWidth()  { return width()->getTextInt(); }
  int getHeight() { return height()->getTextInt(); }
  int getLeft()   { return left()->getTextInt(); }
  int getRight()  { return right()->getTextInt(); }
  int getTop()    { return top()->getTextInt(); }
  int getBottom() { return bottom()->getTextInt(); }

protected:

  // SelectBoxDelegate impleentation
  virtual void onChangeRectangle(const gfx::Rect& rect) override {
    m_rect = rect;

    updateSizeFromRect();
    updateBorderFromRect();
    updateIcons();
  }

  void onSizeChange() {
    updateBorderFromSize();
    updateRectFromBorder();
    updateEditorBoxFromRect();
  }

  void onDirChange() {
    updateIcons();
    updateBorderFromSize();
    updateRectFromBorder();
    updateEditorBoxFromRect();
  }

  void onBorderChange() {
    updateIcons();
    updateRectFromBorder();
    updateSizeFromRect();
    updateEditorBoxFromRect();
  }

  virtual void onBroadcastMouseMessage(WidgetsList& targets) override {
    Window::onBroadcastMouseMessage(targets);

    // Add the editor as receptor of mouse events too.
    targets.push_back(View::getView(m_editor));
  }

private:

  void updateBorderFromSize() {
    int w = getWidth() - m_editor->sprite()->width();
    int h = getHeight() - m_editor->sprite()->height();
    int l, r, t, b;
    l = r = t = b = 0;

    switch ((Dir)dir()->selectedItem()) {
      case Dir::NW:
      case Dir::W:
      case Dir::SW:
        r = w;
        break;
      case Dir::N:
      case Dir::C:
      case Dir::S:
        l = r = w/2;
        if (w & 1)
          r += (w >= 0 ? 1: -1);
        break;
      case Dir::NE:
      case Dir::E:
      case Dir::SE:
        l = w;
        break;
    }

    switch ((Dir)dir()->selectedItem()) {
      case Dir::NW:
      case Dir::N:
      case Dir::NE:
        b = h;
        break;
      case Dir::W:
      case Dir::C:
      case Dir::E:
        b = t = h/2;
        if (h & 1)
          t += (h >= 0 ? 1: -1);
        break;
      case Dir::SW:
      case Dir::S:
      case Dir::SE:
        t = h;
        break;
    }

    setLeft(l);
    setRight(r);
    setTop(t);
    setBottom(b);
  }

  void updateRectFromBorder() {
    int left = getLeft();
    int top = getTop();

    m_rect = gfx::Rect(-left, -top,
      m_editor->sprite()->width() + left + getRight(),
      m_editor->sprite()->height() + top + getBottom());
  }

  void updateSizeFromRect() {
    setWidth(m_rect.w);
    setHeight(m_rect.h);
  }

  void updateBorderFromRect() {
    setLeft(-m_rect.x);
    setTop(-m_rect.y);
    setRight((m_rect.x + m_rect.w) - current_editor->sprite()->width());
    setBottom((m_rect.y + m_rect.h) - current_editor->sprite()->height());
  }

  void updateEditorBoxFromRect() {
    static_cast<SelectBoxState*>(m_selectBoxState.get())->setBoxBounds(m_rect);

    // Redraw new rulers position
    m_editor->invalidate();
  }

  void updateIcons() {
    SkinTheme* theme = static_cast<SkinTheme*>(getTheme());

    int sel = dir()->selectedItem();

    int c = 0;
    for (int v=0; v<3; ++v) {
      for (int u=0; u<3; ++u) {
        const char* iconId = "canvas_empty";

        if (c == sel) {
          iconId = "canvas_c";
        }
        else if (u+1 < 3 && (u+1)+3*v == sel) {
          iconId = "canvas_w";
        }
        else if (u-1 >= 0 && (u-1)+3*v == sel) {
          iconId = "canvas_e";
        }
        else if (v+1 < 3 && u+3*(v+1) == sel) {
          iconId = "canvas_n";
        }
        else if (v-1 >= 0 && u+3*(v-1) == sel) {
          iconId = "canvas_s";
        }
        else if (u+1 < 3 && v+1 < 3 && (u+1)+3*(v+1) == sel) {
          iconId = "canvas_nw";
        }
        else if (u-1 >= 0 && v+1 < 3 && (u-1)+3*(v+1) == sel) {
          iconId = "canvas_ne";
        }
        else if (u+1 < 3 && v-1 >= 0 && (u+1)+3*(v-1) == sel) {
          iconId = "canvas_sw";
        }
        else if (u-1 >= 0 && v-1 >= 0 && (u-1)+3*(v-1) == sel) {
          iconId = "canvas_se";
        }

        dir()->getItem(c)->setIcon(theme->get_part(iconId));
        ++c;
      }
    }
  }

  void setWidth(int v)  { width()->setTextf("%d", v); }
  void setHeight(int v) { height()->setTextf("%d", v); }
  void setLeft(int v)   { left()->setTextf("%d", v); }
  void setRight(int v)  { right()->setTextf("%d", v); }
  void setTop(int v)    { top()->setTextf("%d", v); }
  void setBottom(int v) { bottom()->setTextf("%d", v); }

  Editor* m_editor;
  gfx::Rect m_rect;
  EditorStatePtr m_selectBoxState;
};

class CanvasSizeCommand : public Command {
  int m_left, m_right, m_top, m_bottom;

public:
  CanvasSizeCommand();
  Command* clone() const override { return new CanvasSizeCommand(*this); }

protected:
  bool onEnabled(Context* context);
  void onExecute(Context* context);
};

CanvasSizeCommand::CanvasSizeCommand()
  : Command("CanvasSize",
            "Canvas Size",
            CmdRecordableFlag)
{
  m_left = m_right = m_top = m_bottom = 0;
}

bool CanvasSizeCommand::onEnabled(Context* context)
{
  return context->checkFlags(ContextFlags::ActiveDocumentIsWritable |
                             ContextFlags::HasActiveSprite);
}

void CanvasSizeCommand::onExecute(Context* context)
{
  const ContextReader reader(context);
  const Sprite* sprite(reader.sprite());

  if (context->isUiAvailable()) {
    // load the window widget
    base::UniquePtr<CanvasSizeWindow> window(new CanvasSizeWindow());

    window->remapWindow();
    window->centerWindow();

    load_window_pos(window, "CanvasSize");
    window->setVisible(true);
    window->openWindowInForeground();
    save_window_pos(window, "CanvasSize");

    if (!window->pressedOk())
      return;

    m_left   = window->getLeft();
    m_right  = window->getRight();
    m_top    = window->getTop();
    m_bottom = window->getBottom();
  }

  // Resize canvas

  int x1 = -m_left;
  int y1 = -m_top;
  int x2 = sprite->width() + m_right;
  int y2 = sprite->height() + m_bottom;

  if (x2 <= x1) x2 = x1+1;
  if (y2 <= y1) y2 = y1+1;

  {
    ContextWriter writer(reader);
    Document* document = writer.document();
    Sprite* sprite = writer.sprite();
    UndoTransaction undoTransaction(writer.context(), "Canvas Size");
    DocumentApi api = document->getApi();

    api.cropSprite(sprite, gfx::Rect(x1, y1, x2-x1, y2-y1));
    undoTransaction.commit();

    document->generateMaskBoundaries();
    update_screen_for_document(document);
  }
}

Command* CommandFactory::createCanvasSizeCommand()
{
  return new CanvasSizeCommand;
}

} // namespace app
