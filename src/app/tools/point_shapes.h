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

namespace app {
namespace tools {

class NonePointShape : public PointShape {
public:
  void transformPoint(ToolLoop* loop, int x, int y)
  {
    // Do nothing
  }
  void getModifiedArea(ToolLoop* loop, int x, int y, Rect& area)
  {
    // Do nothing
  }
};

class PixelPointShape : public PointShape {
public:
  void transformPoint(ToolLoop* loop, int x, int y)
  {
    doInkHline(x, y, x, loop);
  }
  void getModifiedArea(ToolLoop* loop, int x, int y, Rect& area)
  {
    area = Rect(x, y, 1, 1);
  }
};

class BrushPointShape : public PointShape {
public:
  void transformPoint(ToolLoop* loop, int x, int y)
  {
    Brush* brush = loop->getBrush();
    std::vector<BrushScanline>::const_iterator scanline = brush->scanline().begin();
    int v, h = brush->bounds().h;

    x += brush->bounds().x;
    y += brush->bounds().y;

    for (v=0; v<h; ++v) {
      if (scanline->state)
        doInkHline(x+scanline->x1, y+v, x+scanline->x2, loop);
      ++scanline;
    }
  }
  void getModifiedArea(ToolLoop* loop, int x, int y, Rect& area)
  {
    Brush* brush = loop->getBrush();
    area = brush->bounds();
    area.x += x;
    area.y += y;
  }
};

class FloodFillPointShape : public PointShape {
public:
  bool isFloodFill() { return true; }

  void transformPoint(ToolLoop* loop, int x, int y)
  {
    doc::algorithm::floodfill(loop->getSrcImage(), x, y,
      paintBounds(loop, x, y),
      loop->getTolerance(),
      loop->getContiguous(),
      loop, (AlgoHLine)doInkHline);
  }

  void getModifiedArea(ToolLoop* loop, int x, int y, Rect& area)
  {
    area = paintBounds(loop, x, y);
  }

private:
  gfx::Rect paintBounds(ToolLoop* loop, int x, int y) {
    gfx::Point offset = loop->getOffset();
    gfx::Rect bounds(
      offset.x, offset.y,
      loop->sprite()->width(), loop->sprite()->height());

    bounds = bounds.createIntersect(loop->getSrcImage()->bounds());

    // Limit the flood-fill to the current tile if the grid is visible.
    if (loop->getDocumentSettings()->getGridVisible()) {
      gfx::Rect grid = loop->getDocumentSettings()->getGridBounds();
      div_t d, dx, dy;

      dx = div(grid.x+loop->getOffset().x, grid.w);
      dy = div(grid.y+loop->getOffset().y, grid.h);

      d = div(x-dx.rem, grid.w);
      x = dx.rem + d.quot*grid.w;

      d = div(y-dy.rem, grid.h);
      y = dy.rem + d.quot*grid.h;

      bounds = bounds.createIntersect(gfx::Rect(x, y, grid.w, grid.h));
    }

    return bounds;
  }
};

class SprayPointShape : public PointShape {
  BrushPointShape m_subPointShape;

public:

  bool isSpray() { return true; }

  void transformPoint(ToolLoop* loop, int x, int y)
  {
    int spray_width = loop->getSprayWidth();
    int spray_speed = loop->getSpraySpeed();
    int c, u, v, times = (spray_width*spray_width/4) * spray_speed / 100;

    // In Windows, rand() has a RAND_MAX too small
#if RAND_MAX <= 0xffff
    fixmath::fixed angle, radius;

    for (c=0; c<times; c++) {
      angle = fixmath::itofix(rand() * 256 / RAND_MAX);
      radius = fixmath::itofix(rand() * (spray_width*10) / RAND_MAX) / 10;
      u = fixmath::fixtoi(fixmath::fixmul(radius, fixmath::fixcos(angle)));
      v = fixmath::fixtoi(fixmath::fixmul(radius, fixmath::fixsin(angle)));

      m_subPointShape.transformPoint(loop, x+u, y+v);
    }
#else
    fixmath::fixed angle, radius;

    for (c=0; c<times; c++) {
      angle = rand();
      radius = rand() % fixmath::itofix(spray_width);
      u = fixmath::fixtoi(fixmath::fixmul(radius, fixmath::fixcos(angle)));
      v = fixmath::fixtoi(fixmath::fixmul(radius, fixmath::fixsin(angle)));
      m_subPointShape.transformPoint(loop, x+u, y+v);
    }
#endif
  }

  void getModifiedArea(ToolLoop* loop, int x, int y, Rect& area)
  {
    int spray_width = loop->getSprayWidth();
    Point p1(x-spray_width, y-spray_width);
    Point p2(x+spray_width, y+spray_width);

    Rect area1;
    Rect area2;
    m_subPointShape.getModifiedArea(loop, p1.x, p1.y, area1);
    m_subPointShape.getModifiedArea(loop, p2.x, p2.y, area2);

    area = area1.createUnion(area2);
  }
};

} // namespace tools
} // namespace app
