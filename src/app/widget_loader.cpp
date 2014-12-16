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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/widget_loader.h"

#include "app/app.h"
#include "app/modules/gui.h"
#include "app/resource_finder.h"
#include "app/ui/button_set.h"
#include "app/ui/color_button.h"
#include "app/ui/skin/skin_theme.h"
#include "app/widget_not_found.h"
#include "app/xml_document.h"
#include "app/xml_exception.h"
#include "base/bind.h"
#include "base/fs.h"
#include "base/memory.h"
#include "ui/ui.h"

#include "tinyxml.h"

#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace app {

using namespace ui;
using namespace app::skin;

static int convert_align_value_to_flags(const char *value);
static bool bool_attr_is_true(const TiXmlElement* elem, const char* attribute_name);
static int int_attr(const TiXmlElement* elem, const char* attribute_name, int default_value);

WidgetLoader::WidgetLoader()
  : m_tooltipManager(NULL)
{
}

WidgetLoader::~WidgetLoader()
{
  for (TypeCreatorsMap::iterator
         it=m_typeCreators.begin(), end=m_typeCreators.end(); it != end; ++it)
    it->second->dispose();
}

void WidgetLoader::addWidgetType(const char* tagName, IWidgetTypeCreator* creator)
{
  m_typeCreators[tagName] = creator;
}

Widget* WidgetLoader::loadWidget(const char* fileName, const char* widgetId, ui::Widget* widget)
{
  std::string buf;

  ResourceFinder rf;
  rf.addPath(fileName);

  buf = "widgets/";
  buf += fileName;
  rf.includeDataDir(buf.c_str());

  if (!rf.findFirst())
    throw WidgetNotFound(widgetId);

  widget = loadWidgetFromXmlFile(rf.filename(), widgetId, widget);
  if (!widget)
    throw WidgetNotFound(widgetId);

  return widget;
}

Widget* WidgetLoader::loadWidgetFromXmlFile(
  const std::string& xmlFilename,
  const std::string& widgetId,
  ui::Widget* widget)
{
  m_tooltipManager = NULL;

  XmlDocumentRef doc(open_xml(xmlFilename));
  TiXmlHandle handle(doc);

  // Search the requested widget.
  TiXmlElement* xmlElement = handle
    .FirstChild("gui")
    .FirstChildElement().ToElement();

  while (xmlElement) {
    const char* nodename = xmlElement->Attribute("id");

    if (nodename && nodename == widgetId) {
      widget = convertXmlElementToWidget(xmlElement, NULL, NULL, widget);
      break;
    }

    xmlElement = xmlElement->NextSiblingElement();
  }

  return widget;
}

Widget* WidgetLoader::convertXmlElementToWidget(const TiXmlElement* elem, Widget* root, Widget* parent, Widget* widget)
{
  const std::string elem_name = elem->Value();

  // TODO error handling: add a message if the widget is bad specified

  // Try to use one of the creators.
  TypeCreatorsMap::iterator it = m_typeCreators.find(elem_name);

  if (it != m_typeCreators.end()) {
    if (!widget)
      widget = it->second->createWidgetFromXml(elem);
  }
  else if (elem_name == "panel") {
    if (!widget)
      widget = new Panel();
  }
  else if (elem_name == "box") {
    bool horizontal  = bool_attr_is_true(elem, "horizontal");
    bool vertical    = bool_attr_is_true(elem, "vertical");
    int align = (horizontal ? JI_HORIZONTAL: vertical ? JI_VERTICAL: 0);

    if (!widget)
      widget = new Box(align);
    else
      widget->setAlign(widget->getAlign() | align);
  }
  else if (elem_name == "vbox") {
    if (!widget)
      widget = new VBox();
  }
  else if (elem_name == "hbox") {
    if (!widget)
      widget = new HBox();
  }
  else if (elem_name == "boxfiller") {
    if (!widget)
      widget = new BoxFiller();
  }
  else if (elem_name == "button") {
    if (!widget)
      widget = new Button("");

    bool left   = bool_attr_is_true(elem, "left");
    bool right  = bool_attr_is_true(elem, "right");
    bool top    = bool_attr_is_true(elem, "top");
    bool bottom = bool_attr_is_true(elem, "bottom");
    bool closewindow = bool_attr_is_true(elem, "closewindow");
    const char *_bevel = elem->Attribute("bevel");

    widget->setAlign((left ? JI_LEFT: (right ? JI_RIGHT: JI_CENTER)) |
      (top ? JI_TOP: (bottom ? JI_BOTTOM: JI_MIDDLE)));

    if (_bevel != NULL) {
      char* bevel = base_strdup(_bevel);
      int c, b[4];
      char *tok;

      for (c=0; c<4; ++c)
        b[c] = 0;

      for (tok=strtok(bevel, " "), c=0;
           tok;
           tok=strtok(NULL, " "), ++c) {
        if (c < 4)
          b[c] = strtol(tok, NULL, 10);
      }
      base_free(bevel);

      setup_bevels(widget, b[0], b[1], b[2], b[3]);
    }

    if (closewindow) {
      static_cast<Button*>(widget)
        ->Click.connect(Bind<void>(&Widget::closeWindow, widget));
    }
  }
  else if (elem_name == "check") {
    const char *looklike = elem->Attribute("looklike");

    if (looklike != NULL && strcmp(looklike, "button") == 0) {
      if (!widget)
        widget = new CheckBox("", kButtonWidget);
    }
    else {
      if (!widget)
        widget = new CheckBox("");
    }

    bool center = bool_attr_is_true(elem, "center");
    bool right  = bool_attr_is_true(elem, "right");
    bool top    = bool_attr_is_true(elem, "top");
    bool bottom = bool_attr_is_true(elem, "bottom");

    widget->setAlign((center ? JI_CENTER:
        (right ? JI_RIGHT: JI_LEFT)) |
      (top    ? JI_TOP:
        (bottom ? JI_BOTTOM: JI_MIDDLE)));
  }
  else if (elem_name == "combobox") {
    if (!widget)
      widget = new ComboBox();

    bool editable = bool_attr_is_true(elem, "editable");
    if (editable)
      ((ComboBox*)widget)->setEditable(true);
  }
  else if (elem_name == "entry") {
    const char* maxsize = elem->Attribute("maxsize");
    const char* suffix = elem->Attribute("suffix");

    if (maxsize != NULL) {
      bool readonly = bool_attr_is_true(elem, "readonly");

      widget = new Entry(strtol(maxsize, NULL, 10), "");

      if (readonly)
        ((Entry*)widget)->setReadOnly(true);

      if (suffix)
        ((Entry*)widget)->setSuffix(suffix);
    }
    else
      throw std::runtime_error("<entry> element found without 'maxsize' attribute");
  }
  else if (elem_name == "grid") {
    const char *columns = elem->Attribute("columns");
    bool same_width_columns = bool_attr_is_true(elem, "same_width_columns");

    if (columns != NULL) {
      widget = new Grid(strtol(columns, NULL, 10),
                        same_width_columns);
    }
  }
  else if (elem_name == "label") {
    if (!widget)
      widget = new Label("");

    bool center = bool_attr_is_true(elem, "center");
    bool right  = bool_attr_is_true(elem, "right");
    bool top    = bool_attr_is_true(elem, "top");
    bool bottom = bool_attr_is_true(elem, "bottom");

    widget->setAlign((center ? JI_CENTER:
        (right ? JI_RIGHT: JI_LEFT)) |
      (top    ? JI_TOP:
        (bottom ? JI_BOTTOM: JI_MIDDLE)));
  }
  else if (elem_name == "link") {
    const char* url = elem->Attribute("url");

    if (!widget)
      widget = new LinkLabel(url ? url: "", "");
    else {
      LinkLabel* link = dynamic_cast<LinkLabel*>(widget);
      ASSERT(link != NULL);
      if (link)
        link->setUrl(url);
    }

    bool center = bool_attr_is_true(elem, "center");
    bool right  = bool_attr_is_true(elem, "right");
    bool top    = bool_attr_is_true(elem, "top");
    bool bottom = bool_attr_is_true(elem, "bottom");

    widget->setAlign(
      (center ? JI_CENTER: (right ? JI_RIGHT: JI_LEFT)) |
      (top    ? JI_TOP: (bottom ? JI_BOTTOM: JI_MIDDLE)));
  }
  else if (elem_name == "listbox") {
    if (!widget)
      widget = new ListBox();
  }
  else if (elem_name == "listitem") {
    ListItem* listitem;
    if (!widget) {
      listitem = new ListItem("");
      widget = listitem;
    }
    else {
      listitem = dynamic_cast<ListItem*>(widget);
      ASSERT(listitem != NULL);
    }

    const char* value = elem->Attribute("value");
    if (value)
      listitem->setValue(value);
  }
  else if (elem_name == "splitter") {
    bool horizontal = bool_attr_is_true(elem, "horizontal");
    bool vertical = bool_attr_is_true(elem, "vertical");
    const char* by = elem->Attribute("by");
    const char* position = elem->Attribute("position");
    Splitter::Type type = (by && strcmp(by, "pixel") == 0 ?
                           Splitter::ByPixel:
                           Splitter::ByPercentage);

    Splitter* splitter = new Splitter(type,
                                      horizontal ? JI_HORIZONTAL:
                                      vertical ? JI_VERTICAL: 0);
    if (position) {
      splitter->setPosition(strtod(position, NULL)
        * (type == Splitter::ByPixel ? guiscale(): 1));
    }
    widget = splitter;
  }
  else if (elem_name == "radio") {
    const char* group = elem->Attribute("group");
    const char* looklike = elem->Attribute("looklike");

    int radio_group = (group ? strtol(group, NULL, 10): 1);

    if (!widget) {
      if (looklike != NULL && strcmp(looklike, "button") == 0) {
        widget = new RadioButton("", radio_group, kButtonWidget);
      }
      else {
        widget = new RadioButton("", radio_group);
      }
    }
    else {
      RadioButton* radio = dynamic_cast<RadioButton*>(widget);
      ASSERT(radio != NULL);
      if (radio)
        radio->setRadioGroup(radio_group);
    }

    bool center = bool_attr_is_true(elem, "center");
    bool right  = bool_attr_is_true(elem, "right");
    bool top    = bool_attr_is_true(elem, "top");
    bool bottom = bool_attr_is_true(elem, "bottom");

    widget->setAlign(
      (center ? JI_CENTER:
        (right ? JI_RIGHT: JI_LEFT)) |
      (top    ? JI_TOP:
        (bottom ? JI_BOTTOM: JI_MIDDLE)));
  }
  else if (elem_name == "separator") {
    bool center      = bool_attr_is_true(elem, "center");
    bool right       = bool_attr_is_true(elem, "right");
    bool middle      = bool_attr_is_true(elem, "middle");
    bool bottom      = bool_attr_is_true(elem, "bottom");
    bool horizontal  = bool_attr_is_true(elem, "horizontal");
    bool vertical    = bool_attr_is_true(elem, "vertical");
    int align =
      (horizontal ? JI_HORIZONTAL: 0) |
      (vertical ? JI_VERTICAL: 0) |
      (center ? JI_CENTER: (right ? JI_RIGHT: JI_LEFT)) |
      (middle ? JI_MIDDLE: (bottom ? JI_BOTTOM: JI_TOP));

    if (!widget) {
      const char* text = elem->Attribute("text");
      widget = new Separator(text ? text: "", align);
    }
    else
      widget->setAlign(widget->getAlign() | align);
  }
  else if (elem_name == "slider") {
    const char *min = elem->Attribute("min");
    const char *max = elem->Attribute("max");
    int min_value = min != NULL ? strtol(min, NULL, 10): 0;
    int max_value = max != NULL ? strtol(max, NULL, 10): 0;

    widget = new Slider(min_value, max_value, min_value);
  }
  else if (elem_name == "textbox") {
    bool wordwrap = bool_attr_is_true(elem, "wordwrap");

    if (!widget)
      widget = new TextBox(elem->GetText(), 0);
    else
      widget->setText(elem->GetText());

    if (wordwrap)
      widget->setAlign(widget->getAlign() | JI_WORDWRAP);
  }
  else if (elem_name == "view") {
    if (!widget)
      widget = new View();
  }
  else if (elem_name == "window") {
    if (!widget) {
      const char* text = elem->Attribute("text");
      bool desktop = bool_attr_is_true(elem, "desktop");

      if (desktop)
        widget = new Window(Window::DesktopWindow);
      else if (text)
        widget = new Window(Window::WithTitleBar, text);
      else
        widget = new Window(Window::WithoutTitleBar);
    }
  }
  else if (elem_name == "colorpicker") {
    if (!widget)
      widget = new ColorButton(Color::fromMask(), app_get_current_pixel_format());
  }
  else if (elem_name == "buttonset") {
    const char* columns = elem->Attribute("columns");

    if (!widget && columns)
      widget = new ButtonSet(strtol(columns, NULL, 10));
  }
  else if (elem_name == "item") {
    if (!parent)
      throw std::runtime_error("<item> without parent");

    if (ButtonSet* buttonset = dynamic_cast<ButtonSet*>(parent)) {
      SkinTheme* theme = static_cast<SkinTheme*>(parent->getTheme());

      const char* icon = elem->Attribute("icon");
      if (icon) {
        int hspan = int_attr(elem, "hspan", 1);
        int vspan = int_attr(elem, "vspan", 1);

        she::Surface* sur = theme->get_part(std::string(icon));
        buttonset->addItem(sur, hspan, vspan);
      }
    }
  }

  // Was the widget created?
  if (widget)
    fillWidgetWithXmlElementAttributes(elem, root, widget);

  return widget;
}

void WidgetLoader::fillWidgetWithXmlElementAttributes(const TiXmlElement* elem, Widget* root, Widget* widget)
{
  const char* id        = elem->Attribute("id");
  const char* text      = elem->Attribute("text");
  const char* tooltip   = elem->Attribute("tooltip");
  bool selected         = bool_attr_is_true(elem, "selected");
  bool disabled         = bool_attr_is_true(elem, "disabled");
  bool expansive        = bool_attr_is_true(elem, "expansive");
  bool homogeneous      = bool_attr_is_true(elem, "homogeneous");
  bool magnet           = bool_attr_is_true(elem, "magnet");
  bool noborders        = bool_attr_is_true(elem, "noborders");
  const char* width     = elem->Attribute("width");
  const char* height    = elem->Attribute("height");
  const char* minwidth  = elem->Attribute("minwidth");
  const char* minheight = elem->Attribute("minheight");
  const char* maxwidth  = elem->Attribute("maxwidth");
  const char* maxheight = elem->Attribute("maxheight");
  const char* childspacing = elem->Attribute("childspacing");

  if (width) {
    if (!minwidth) minwidth = width;
    if (!maxwidth) maxwidth = width;
  }

  if (height) {
    if (!minheight) minheight = height;
    if (!maxheight) maxheight = height;
  }

  if (id != NULL)
    widget->setId(id);

  if (text)
    widget->setText(text);

  if (tooltip != NULL && root != NULL) {
    if (!m_tooltipManager) {
      m_tooltipManager = new ui::TooltipManager();
      root->addChild(m_tooltipManager);
    }
    m_tooltipManager->addTooltipFor(widget, tooltip, JI_LEFT);
  }

  if (selected)
    widget->setSelected(selected);

  if (disabled)
    widget->setEnabled(false);

  if (expansive)
    widget->setExpansive(true);

  if (homogeneous)
    widget->setAlign(widget->getAlign() | JI_HOMOGENEOUS);

  if (magnet)
    widget->setFocusMagnet(true);

  if (noborders)
    widget->noBorderNoChildSpacing();

  if (childspacing)
    widget->child_spacing = strtol(childspacing, NULL, 10);

  gfx::Size reqSize = widget->getPreferredSize();

  if (minwidth || minheight) {
    int w = (minwidth ? guiscale()*strtol(minwidth, NULL, 10): reqSize.w);
    int h = (minheight ? guiscale()*strtol(minheight, NULL, 10): reqSize.h);
    widget->setMinSize(gfx::Size(w, h));
  }

  if (maxwidth || maxheight) {
    int w = (maxwidth ? guiscale()*strtol(maxwidth, NULL, 10): INT_MAX);
    int h = (maxheight ? guiscale()*strtol(maxheight, NULL, 10): INT_MAX);
    widget->setMaxSize(gfx::Size(w, h));
  }

  if (!root)
    root = widget;

  // Children
  const TiXmlElement* childElem = elem->FirstChildElement();
  while (childElem) {
    Widget* child = convertXmlElementToWidget(childElem, root, widget, NULL);
    if (child) {
      // Attach the child in the view
      if (widget->type == kViewWidget) {
        static_cast<View*>(widget)->attachToView(child);
        break;
      }
      // Add the child in the grid
      else if (widget->type == kGridWidget) {
        const char* cell_hspan = childElem->Attribute("cell_hspan");
        const char* cell_vspan = childElem->Attribute("cell_vspan");
        const char* cell_align = childElem->Attribute("cell_align");
        int hspan = cell_hspan ? strtol(cell_hspan, NULL, 10): 1;
        int vspan = cell_vspan ? strtol(cell_vspan, NULL, 10): 1;
        int align = cell_align ? convert_align_value_to_flags(cell_align): 0;
        Grid* grid = dynamic_cast<Grid*>(widget);
        ASSERT(grid != NULL);

        grid->addChildInCell(child, hspan, vspan, align);
      }
      // Just add the child in any other kind of widget
      else
        widget->addChild(child);
    }
    childElem = childElem->NextSiblingElement();
  }

  if (widget->type == kViewWidget) {
    bool maxsize = bool_attr_is_true(elem, "maxsize");
    if (maxsize)
      static_cast<View*>(widget)->makeVisibleAllScrollableArea();
  }
}

static int convert_align_value_to_flags(const char *value)
{
  char *tok, *ptr = base_strdup(value);
  int flags = 0;

  for (tok=strtok(ptr, " ");
       tok != NULL;
       tok=strtok(NULL, " ")) {
    if (strcmp(tok, "horizontal") == 0) {
      flags |= JI_HORIZONTAL;
    }
    else if (strcmp(tok, "vertical") == 0) {
      flags |= JI_VERTICAL;
    }
    else if (strcmp(tok, "left") == 0) {
      flags |= JI_LEFT;
    }
    else if (strcmp(tok, "center") == 0) {
      flags |= JI_CENTER;
    }
    else if (strcmp(tok, "right") == 0) {
      flags |= JI_RIGHT;
    }
    else if (strcmp(tok, "top") == 0) {
      flags |= JI_TOP;
    }
    else if (strcmp(tok, "middle") == 0) {
      flags |= JI_MIDDLE;
    }
    else if (strcmp(tok, "bottom") == 0) {
      flags |= JI_BOTTOM;
    }
    else if (strcmp(tok, "homogeneous") == 0) {
      flags |= JI_HOMOGENEOUS;
    }
  }

  base_free(ptr);
  return flags;
}

static bool bool_attr_is_true(const TiXmlElement* elem, const char* attribute_name)
{
  const char* value = elem->Attribute(attribute_name);

  return (value != NULL) && (strcmp(value, "true") == 0);
}

static int int_attr(const TiXmlElement* elem, const char* attribute_name, int default_value)
{
  const char* value = elem->Attribute(attribute_name);

  return (value ? strtol(value, NULL, 10): default_value);
}

} // namespace app
