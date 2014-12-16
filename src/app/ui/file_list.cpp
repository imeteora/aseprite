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

#include "app/ui/file_list.h"

#include "app/modules/gfx.h"
#include "app/thumbnail_generator.h"
#include "app/ui/skin/skin_theme.h"
#include "base/string.h"
#include "she/font.h"
#include "she/surface.h"
#include "ui/ui.h"

#include <algorithm>
#include <cctype>
#include <cstring>

#define ISEARCH_KEYPRESS_INTERVAL_MSECS 500

namespace app {

using namespace app::skin;
using namespace gfx;
using namespace ui;

FileList::FileList()
  : Widget(kGenericWidget)
  , m_generateThumbnailTimer(200, this)
  , m_monitoringTimer(50, this)
{
  setFocusStop(true);
  setDoubleBuffered(true);

  m_currentFolder = FileSystemModule::instance()->getRootFileItem();
  m_req_valid = false;
  m_selected = NULL;
  m_isearchClock = 0;

  m_itemToGenerateThumbnail = NULL;

  m_generateThumbnailTimer.Tick.connect(&FileList::onGenerateThumbnailTick, this);
  m_monitoringTimer.Tick.connect(&FileList::onMonitoringTick, this);
  m_monitoringTimer.start();

  regenerateList();
}

FileList::~FileList()
{
  // Stop timers.
  m_generateThumbnailTimer.stop();
  m_monitoringTimer.stop();

  // Stop workers creating thumbnails.
  ThumbnailGenerator::instance()->stopAllWorkers();
}

void FileList::setExtensions(const char* extensions)
{
  m_exts = extensions;
}

void FileList::setCurrentFolder(IFileItem* folder)
{
  ASSERT(folder != NULL);
  ASSERT(folder->isBrowsable());

  m_currentFolder = folder;
  m_req_valid = false;
  m_selected = NULL;

  regenerateList();

  // select first folder
  if (!m_list.empty() && m_list.front()->isBrowsable())
    selectIndex(0);

  // Emit "CurrentFolderChanged" event.
  onCurrentFolderChanged();

  invalidate();
  View::getView(this)->updateView();

  requestFocus();
}

void FileList::goUp()
{
  IFileItem* folder = m_currentFolder;
  IFileItem* parent = folder->getParent();
  if (parent) {
    setCurrentFolder(parent);
    m_selected = folder;

    // Make the selected item visible.
    makeSelectedFileitemVisible();
  }
}

bool FileList::onProcessMessage(Message* msg)
{
  switch (msg->type()) {

    case kMouseDownMessage:
      captureMouse();

    case kMouseMoveMessage:
      if (hasCapture()) {
        MouseMessage* mouseMsg = static_cast<MouseMessage*>(msg);
        int th = getTextHeight();
        int y = getBounds().y;
        IFileItem* old_selected = m_selected;
        m_selected = NULL;

        // rows
        for (FileItemList::iterator
               it=m_list.begin();
             it!=m_list.end(); ++it) {
          IFileItem* fi = *it;
          gfx::Size itemSize = getFileItemSize(fi);

          if (((mouseMsg->position().y >= y) &&
               (mouseMsg->position().y < y+th+4*guiscale())) ||
              (it == m_list.begin() && mouseMsg->position().y < y) ||
              (it == m_list.end()-1 && mouseMsg->position().y >= y+th+4*guiscale())) {
            m_selected = fi;
            makeSelectedFileitemVisible();
            break;
          }

          y += itemSize.h;
        }

        if (old_selected != m_selected) {
          generatePreviewOfSelectedItem();

          invalidate();

          // Emit "FileSelected" event.
          onFileSelected();
        }
      }
      break;

    case kMouseUpMessage:
      if (hasCapture()) {
        releaseMouse();
      }
      break;

    case kKeyDownMessage:
      if (hasFocus()) {
        KeyMessage* keyMsg = static_cast<KeyMessage*>(msg);
        KeyScancode scancode = keyMsg->scancode();
        int unicodeChar = keyMsg->unicodeChar();
        int select = getSelectedIndex();
        View* view = View::getView(this);
        int bottom = m_list.size();

        switch (scancode) {

          case kKeyUp:
            if (select >= 0)
              select--;
            else
              select = 0;
            break;

          case kKeyDown:
            if (select >= 0)
              select++;
            else
              select = 0;
            break;

          case kKeyHome:
            select = 0;
            break;

          case kKeyEnd:
            select = bottom-1;
            break;

          case kKeyPageUp:
          case kKeyPageDown: {
            int sgn = (scancode == kKeyPageUp) ? -1: 1;
            gfx::Rect vp = view->getViewportBounds();
            if (select < 0)
              select = 0;
            select += sgn * vp.h / (getTextHeight()+4*guiscale());
            break;
          }

          case kKeyLeft:
          case kKeyRight:
            if (select >= 0) {
              gfx::Rect vp = view->getViewportBounds();
              int sgn = (scancode == kKeyLeft) ? -1: 1;
              gfx::Point scroll = view->getViewScroll();
              scroll.x += vp.w/2*sgn;
              view->setViewScroll(scroll);
            }
            break;

          case kKeyEnter:
          case kKeyEnterPad:
            if (m_selected) {
              if (m_selected->isBrowsable()) {
                setCurrentFolder(m_selected);
                return true;
              }
              if (m_selected->isFolder()) {
                // Do nothing (is a folder but not browseable).
                return true;
              }
              else {
                // Emit "FileAccepted" event.
                onFileAccepted();
                return true;
              }
            }
            else
              return Widget::onProcessMessage(msg);

          case kKeyBackspace:
            goUp();
            return true;

          default:
            if (unicodeChar == ' ' ||
                (std::tolower(unicodeChar) >= 'a' &&
                 std::tolower(unicodeChar) <= 'z') ||
                (unicodeChar >= '0' &&
                 unicodeChar <= '9')) {
              if (ui::clock() - m_isearchClock > ISEARCH_KEYPRESS_INTERVAL_MSECS)
                m_isearch.clear();

              m_isearch.push_back(unicodeChar);

              int i, chrs = m_isearch.size();
              FileItemList::iterator
                link = m_list.begin() + ((select >= 0) ? select: 0);

              for (i=MAX(select, 0); i<bottom; ++i, ++link) {
                IFileItem* fi = *link;
                if (base::utf8_icmp(fi->getDisplayName(), m_isearch, chrs) == 0) {
                  select = i;
                  break;
                }
              }
              m_isearchClock = ui::clock();
              // Go to selectIndex...
            }
            else
              return Widget::onProcessMessage(msg);
        }

        if (bottom > 0)
          selectIndex(MID(0, select, bottom-1));

        return true;
      }
      break;

    case kMouseWheelMessage: {
      View* view = View::getView(this);
      if (view) {
        gfx::Point scroll = view->getViewScroll();
        scroll += static_cast<MouseMessage*>(msg)->wheelDelta() * 3*(getTextHeight()+4*guiscale());
        view->setViewScroll(scroll);
      }
      break;
    }

    case kDoubleClickMessage:
      if (m_selected) {
        if (m_selected->isBrowsable()) {
          setCurrentFolder(m_selected);
          return true;
        }
        else {
          onFileAccepted();         // Emit "FileAccepted" event.
          return true;
        }
      }
      break;

  }

  return Widget::onProcessMessage(msg);
}

void FileList::onPaint(ui::PaintEvent& ev)
{
  Graphics* g = ev.getGraphics();
  SkinTheme* theme = static_cast<SkinTheme*>(getTheme());
  View* view = View::getView(this);
  gfx::Rect vp = view->getViewportBounds();
  gfx::Rect bounds = getClientBounds();
  int x, y = bounds.y;
  int evenRow = 0;
  gfx::Color bgcolor;
  gfx::Color fgcolor;
  she::Surface* thumbnail = NULL;
  int thumbnail_y = 0;

  g->fillRect(theme->getColor(ThemeColor::Background), bounds);

  // rows
  for (FileItemList::iterator
         it=m_list.begin(), end=m_list.end(); it!=end; ++it) {
    IFileItem* fi = *it;
    gfx::Size itemSize = getFileItemSize(fi);

    if (fi == m_selected) {
      fgcolor = theme->getColor(ThemeColor::FileListSelectedRowText);
      bgcolor = theme->getColor(ThemeColor::FileListSelectedRowFace);
    }
    else {
      bgcolor = evenRow ? theme->getColor(ThemeColor::FileListEvenRowFace):
                          theme->getColor(ThemeColor::FileListOddRowFace);

      if (fi->isFolder() && !fi->isBrowsable())
        fgcolor = theme->getColor(ThemeColor::FileListDisabledRowText);
      else
        fgcolor = evenRow ? theme->getColor(ThemeColor::FileListEvenRowText):
                            theme->getColor(ThemeColor::FileListOddRowText);
    }

    x = bounds.x+2*guiscale();

    // Item background
    g->fillRect(bgcolor, gfx::Rect(bounds.x, y, bounds.w, itemSize.h));

    if (fi->isFolder()) {
      int icon_w = getFont()->textLength("[+]");

      g->drawUIString("[+]", fgcolor, bgcolor, gfx::Point(x, y+2*guiscale()));
      x += icon_w+2*guiscale();
    }

    // item name
    g->drawString(
      fi->getDisplayName().c_str(),
      fgcolor, bgcolor, gfx::Point(x, y+2*guiscale()));

    // draw progress bars
    double progress;
    ThumbnailGenerator::WorkerStatus workerStatus =
      ThumbnailGenerator::instance()->getWorkerStatus(fi, progress);

    if (workerStatus == ThumbnailGenerator::WorkingOnThumbnail) {
      int barw = 64*guiscale();

      theme->paintProgressBar(g,
        gfx::Rect(
          bounds.x2()-2*guiscale()-barw,
          y+itemSize.h/2-3*guiscale(),
          barw, 6*guiscale()),
        progress);
    }

    // Thumbnail position
    if (fi == m_selected) {
      thumbnail = fi->getThumbnail();
      if (thumbnail)
        thumbnail_y = y + itemSize.h/2;
    }

    y += itemSize.h;
    evenRow ^= 1;
  }

  // Draw the thumbnail
  if (thumbnail) {
    x = vp.x+vp.w - 2*guiscale() - thumbnail->width();
    y = thumbnail_y - thumbnail->height()/2 + getBounds().y;
    y = MID(vp.y+2*guiscale(), y, vp.y+vp.h-3*guiscale()-thumbnail->height());
    x -= getBounds().x;
    y -= getBounds().y;

    g->blit(thumbnail, 0, 0, x, y, thumbnail->width(), thumbnail->height());
    g->drawRect(gfx::rgba(0, 0, 0),
      gfx::Rect(x-1, y-1, thumbnail->width()+1, thumbnail->height()+1));
  }
}

void FileList::onPreferredSize(PreferredSizeEvent& ev)
{
  if (!m_req_valid) {
    gfx::Size reqSize(0, 0);

    // rows
    for (FileItemList::iterator
           it=m_list.begin();
         it!=m_list.end(); ++it) {
      IFileItem* fi = *it;
      gfx::Size itemSize = getFileItemSize(fi);
      reqSize.w = MAX(reqSize.w, itemSize.w);
      reqSize.h += itemSize.h;
    }

    m_req_valid = true;
    m_req_w = reqSize.w;
    m_req_h = reqSize.h;
  }
  ev.setPreferredSize(Size(m_req_w, m_req_h));
}

void FileList::onFileSelected()
{
  FileSelected();
}

void FileList::onFileAccepted()
{
  FileAccepted();
}

void FileList::onCurrentFolderChanged()
{
  CurrentFolderChanged();
}

void FileList::onMonitoringTick()
{
  if (ThumbnailGenerator::instance()->checkWorkers())
    invalidate();
}

void FileList::onGenerateThumbnailTick()
{
  m_generateThumbnailTimer.stop();

  IFileItem* fileitem = m_itemToGenerateThumbnail;
  if (fileitem)
    ThumbnailGenerator::instance()->addWorkerToGenerateThumbnail(fileitem);
}

gfx::Size FileList::getFileItemSize(IFileItem* fi) const
{
  int len = 0;

  if (fi->isFolder())
    len += getFont()->textLength("[+]") + 2*guiscale();

  len += getFont()->textLength(fi->getDisplayName().c_str());

  return gfx::Size(len+4*guiscale(), getTextHeight()+4*guiscale());
}

void FileList::makeSelectedFileitemVisible()
{
  View* view = View::getView(this);
  gfx::Rect vp = view->getViewportBounds();
  gfx::Point scroll = view->getViewScroll();
  int th = getTextHeight();
  int y = getBounds().y;

  // rows
  for (FileItemList::iterator
         it=m_list.begin();
       it!=m_list.end(); ++it) {
    IFileItem* fi = *it;
    gfx::Size itemSize = getFileItemSize(fi);

    if (fi == m_selected) {
      if (y < vp.y)
        scroll.y = y - getBounds().y;
      else if (y > vp.y + vp.h - (th+4*guiscale()))
        scroll.y = y - getBounds().y - vp.h + (th+4*guiscale());

      view->setViewScroll(scroll);
      break;
    }

    y += itemSize.h;
  }
}

void FileList::regenerateList()
{
  // get the children of the current folder
  m_list = m_currentFolder->getChildren();

  // filter the list by the available extensions
  if (!m_exts.empty()) {
    for (FileItemList::iterator
           it=m_list.begin();
         it!=m_list.end(); ) {
      IFileItem* fileitem = *it;
      if (!fileitem->isFolder() &&
          !fileitem->hasExtension(m_exts.c_str())) {
        it = m_list.erase(it);
      }
      else
        ++it;
    }
  }
}

int FileList::getSelectedIndex()
{
  for (FileItemList::iterator
         it = m_list.begin();
       it != m_list.end(); ++it) {
    if (*it == m_selected)
      return it - m_list.begin();
  }

  return -1;
}

void FileList::selectIndex(int index)
{
  IFileItem* old_selected = m_selected;

  m_selected = m_list.at(index);
  if (old_selected != m_selected) {
    makeSelectedFileitemVisible();

    invalidate();

    // Emit "FileSelected" event.
    onFileSelected();
  }

  generatePreviewOfSelectedItem();
}

// Puts the selected file-item as the next item to be processed by the
// round-robin that generate thumbnails
void FileList::generatePreviewOfSelectedItem()
{
  if (m_selected &&
      !m_selected->isFolder() &&
      !m_selected->getThumbnail())
    {
      m_itemToGenerateThumbnail = m_selected;
      m_generateThumbnailTimer.start();
    }
}

} // namespace app
