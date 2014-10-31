// Aseprite UI Library
// Copyright (C) 2001-2013  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ui/listbox.h"

#include "ui/listitem.h"
#include "ui/message.h"
#include "ui/preferred_size_event.h"
#include "ui/resize_event.h"
#include "ui/system.h"
#include "ui/theme.h"
#include "ui/view.h"

namespace ui {

using namespace gfx;

ListBox::ListBox()
  : Widget(kListBoxWidget)
{
  setFocusStop(true);
  initTheme();
}

ListItem* ListBox::getSelectedChild()
{
  UI_FOREACH_WIDGET(getChildren(), it) {
    ASSERT(dynamic_cast<ListItem*>(*it) != NULL);

    if (static_cast<ListItem*>(*it)->isSelected())
      return static_cast<ListItem*>(*it);
  }
  return 0;
}

int ListBox::getSelectedIndex()
{
  int i = 0;

  UI_FOREACH_WIDGET(getChildren(), it) {
    if (static_cast<ListItem*>(*it)->isSelected())
      return i;
    i++;
  }

  return -1;
}

void ListBox::selectChild(ListItem* item)
{
  UI_FOREACH_WIDGET(getChildren(), it) {
    ListItem* child = static_cast<ListItem*>(*it);

    if (child->isSelected()) {
      if (item && child == item)
        return;

      child->setSelected(false);
    }
  }

  if (item) {
    View* view = View::getView(this);

    item->setSelected(true);

    if (view) {
      gfx::Rect vp = view->getViewportBounds();
      gfx::Point scroll = view->getViewScroll();

      if (item->getBounds().y < vp.y)
        scroll.y = item->getBounds().y - getBounds().y;
      else if (item->getBounds().y > vp.y + vp.h - item->getBounds().h)
        scroll.y = (item->getBounds().y - getBounds().y
                    - vp.h + item->getBounds().h);

      view->setViewScroll(scroll);
    }
  }

  onChangeSelectedItem();
}

void ListBox::selectIndex(int index)
{
  const WidgetsList& children = getChildren();
  if (index < 0 || index >= (int)children.size())
    return;

  ListItem* child = static_cast<ListItem*>(children[index]);
  ASSERT(child);
  selectChild(child);
}

size_t ListBox::getItemsCount() const
{
  return getChildren().size();
}

// Setup the scroll to center the selected item in the viewport
void ListBox::centerScroll()
{
  View* view = View::getView(this);
  ListItem* item = getSelectedChild();

  if (view && item) {
    gfx::Rect vp = view->getViewportBounds();
    gfx::Point scroll = view->getViewScroll();

    scroll.y = ((item->getBounds().y - getBounds().y)
                - vp.h/2 + item->getBounds().h/2);

    view->setViewScroll(scroll);
  }
}

inline bool sort_by_text(Widget* a, Widget* b) {
  return a->getText() < b->getText();
}

void ListBox::sortItems()
{
  WidgetsList widgets = getChildren();
  std::sort(widgets.begin(), widgets.end(), &sort_by_text);

  // Remove all children and add then again.
  while (!getChildren().empty())
    removeChild(getChildren().back());

  for (Widget* child : widgets)
    addChild(child);
}

bool ListBox::onProcessMessage(Message* msg)
{
  switch (msg->type()) {

    case kOpenMessage:
      centerScroll();
      break;

    case kMouseDownMessage:
      captureMouse();

    case kMouseMoveMessage:
      if (hasCapture()) {
        gfx::Point mousePos = static_cast<MouseMessage*>(msg)->position();
        int select = getSelectedIndex();
        View* view = View::getView(this);
        bool pick_item = true;

        if (view) {
          gfx::Rect vp = view->getViewportBounds();

          if (mousePos.y < vp.y) {
            int num = MAX(1, (vp.y - mousePos.y) / 8);
            selectIndex(select-num);
            pick_item = false;
          }
          else if (mousePos.y >= vp.y + vp.h) {
            int num = MAX(1, (mousePos.y - (vp.y+vp.h-1)) / 8);
            selectIndex(select+num);
            pick_item = false;
          }
        }

        if (pick_item) {
          Widget* picked;

          if (view) {
            picked = view->getViewport()->pick(mousePos);
          }
          else {
            picked = pick(mousePos);
          }

          /* if the picked widget is a child of the list, select it */
          if (picked && hasChild(picked)) {
            if (ListItem* pickedItem = dynamic_cast<ListItem*>(picked))
              selectChild(pickedItem);
          }
        }

        return true;
      }
      break;

    case kMouseUpMessage:
      releaseMouse();
      break;

    case kMouseWheelMessage: {
      View* view = View::getView(this);
      if (view) {
        gfx::Point scroll = view->getViewScroll();
        scroll += static_cast<MouseMessage*>(msg)->wheelDelta() * getTextHeight()*3;
        view->setViewScroll(scroll);
      }
      break;
    }

    case kKeyDownMessage:
      if (hasFocus() && !getChildren().empty()) {
        int select = getSelectedIndex();
        View* view = View::getView(this);
        int bottom = MAX(0, getChildren().size()-1);
        KeyMessage* keymsg = static_cast<KeyMessage*>(msg);

        switch (keymsg->scancode()) {
          case kKeyUp:
            select--;
            break;
          case kKeyDown:
            select++;
            break;
          case kKeyHome:
            select = 0;
            break;
          case kKeyEnd:
            select = bottom;
            break;
          case kKeyPageUp:
            if (view) {
              gfx::Rect vp = view->getViewportBounds();
              select -= vp.h / getTextHeight();
            }
            else
              select = 0;
            break;
          case kKeyPageDown:
            if (view) {
              gfx::Rect vp = view->getViewportBounds();
              select += vp.h / getTextHeight();
            }
            else
              select = bottom;
            break;
          case kKeyLeft:
          case kKeyRight:
            if (view) {
              gfx::Rect vp = view->getViewportBounds();
              gfx::Point scroll = view->getViewScroll();
              int sgn = (keymsg->scancode() == kKeyLeft) ? -1: 1;

              scroll.x += vp.w/2*sgn;

              view->setViewScroll(scroll);
            }
            break;
          default:
            return Widget::onProcessMessage(msg);
        }

        selectIndex(MID(0, select, bottom));
        return true;
      }
      break;

    case kDoubleClickMessage:
      onDoubleClickItem();
      return true;
  }

  return Widget::onProcessMessage(msg);
}

void ListBox::onPaint(PaintEvent& ev)
{
  getTheme()->paintListBox(ev);
}

void ListBox::onResize(ResizeEvent& ev)
{
  setBoundsQuietly(ev.getBounds());

  Rect cpos = getChildrenBounds();

  UI_FOREACH_WIDGET(getChildren(), it) {
    Widget* child = *it;

    cpos.h = child->getPreferredSize().h;
    child->setBounds(cpos);

    cpos.y += child->getBounds().h + this->child_spacing;
  }
}

void ListBox::onPreferredSize(PreferredSizeEvent& ev)
{
  int w = 0, h = 0;

  UI_FOREACH_WIDGET_WITH_END(getChildren(), it, end) {
    Size reqSize = static_cast<ListItem*>(*it)->getPreferredSize();

    w = MAX(w, reqSize.w);
    h += reqSize.h + (it+1 != end ? this->child_spacing: 0);
  }

  w += this->border_width.l + this->border_width.r;
  h += this->border_width.t + this->border_width.b;

  ev.setPreferredSize(Size(w, h));
}

void ListBox::onChangeSelectedItem()
{
  ChangeSelectedItem();
}

void ListBox::onDoubleClickItem()
{
  DoubleClickItem();
}

} // namespace ui
