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

#include "app/ui/editor/moving_pixels_state.h"

#include "app/app.h"
#include "app/color_utils.h"
#include "app/commands/cmd_flip.h"
#include "app/commands/cmd_move_mask.h"
#include "app/commands/command.h"
#include "app/commands/commands.h"
#include "app/settings/settings.h"
#include "app/tools/ink.h"
#include "app/tools/tool.h"
#include "app/ui/context_bar.h"
#include "app/ui/editor/editor.h"
#include "app/ui/editor/editor_customization_delegate.h"
#include "app/ui/editor/pixels_movement.h"
#include "app/ui/editor/standby_state.h"
#include "app/ui/editor/transform_handles.h"
#include "app/ui/keyboard_shortcuts.h"
#include "app/ui/main_window.h"
#include "app/ui/status_bar.h"
#include "app/ui_context.h"
#include "app/util/clipboard.h"
#include "base/unique_ptr.h"
#include "gfx/rect.h"
#include "doc/algorithm/flip_image.h"
#include "doc/mask.h"
#include "doc/sprite.h"
#include "ui/manager.h"
#include "ui/message.h"
#include "ui/system.h"
#include "ui/view.h"

namespace app {

using namespace ui;

MovingPixelsState::MovingPixelsState(Editor* editor, MouseMessage* msg, PixelsMovementPtr pixelsMovement, HandleType handle)
  : m_editor(editor)
  , m_discarded(false)
{
  // MovingPixelsState needs a selection tool to avoid problems
  // sharing the extra cel between the drawing cursor preview and the
  // pixels movement/transformation preview.
  //ASSERT(!editor->getCurrentEditorInk()->isSelection());

  UIContext* context = UIContext::instance();

  m_pixelsMovement = pixelsMovement;

  if (handle != NoHandle) {
    int u, v;
    editor->screenToEditor(msg->position().x, msg->position().y, &u, &v);
    m_pixelsMovement->catchImage(u, v, handle);

    editor->captureMouse();
  }

  // Setup mask color
  setTransparentColor(context->settings()->selection()->getMoveTransparentColor());

  // Hook BeforeCommandExecution signal so we know if the user wants
  // to execute other command, so we can drop pixels.
  m_ctxConn =
    context->BeforeCommandExecution.connect(&MovingPixelsState::onBeforeCommandExecution, this);

  // Observe SelectionSettings to be informed of changes to
  // Transparent Color from Context Bar.
  context->settings()->selection()->addObserver(this);

  // Add the current editor as filter for key message of the manager
  // so we can catch the Enter key, and avoid to execute the
  // PlayAnimation command.
  m_editor->getManager()->addMessageFilter(kKeyDownMessage, m_editor);
  m_editor->getManager()->addMessageFilter(kKeyUpMessage, m_editor);
  m_editor->addObserver(this);

  ContextBar* contextBar = App::instance()->getMainWindow()->getContextBar();
  contextBar->updateForMovingPixels();
  contextBar->addObserver(this);
}

MovingPixelsState::~MovingPixelsState()
{
  ContextBar* contextBar = App::instance()->getMainWindow()->getContextBar();
  contextBar->removeObserver(this);
  contextBar->updateFromTool(UIContext::instance()->settings()->getCurrentTool());

  m_ctxConn.disconnect();
  UIContext::instance()->settings()->selection()->removeObserver(this);

  m_pixelsMovement.reset(NULL);

  m_editor->removeObserver(this);
  m_editor->getManager()->removeMessageFilter(kKeyDownMessage, m_editor);
  m_editor->getManager()->removeMessageFilter(kKeyUpMessage, m_editor);

  m_editor->document()->generateMaskBoundaries();
}

void MovingPixelsState::translate(int dx, int dy)
{
  if (m_pixelsMovement->isDragging())
    m_pixelsMovement->dropImageTemporarily();

  m_pixelsMovement->catchImageAgain(0, 0, MoveHandle);
  m_pixelsMovement->moveImage(dx, dy, PixelsMovement::NormalMovement);
  m_pixelsMovement->dropImageTemporarily();
}

EditorState::BeforeChangeAction MovingPixelsState::onBeforeChangeState(Editor* editor, EditorState* newState)
{
  ASSERT(m_pixelsMovement != NULL);

  // If we are changing to another state, we've to drop the image.
  if (m_pixelsMovement->isDragging())
    m_pixelsMovement->dropImageTemporarily();

  // Drop pixels if we are changing to a non-temporary state (a
  // temporary state is something like ScrollingState).
  if (!newState || !newState->isTemporalState()) {
    if (!m_discarded)
      m_pixelsMovement->dropImage();

    editor->document()->resetTransformation();

    m_pixelsMovement.reset(NULL);

    editor->releaseMouse();

    return DiscardState;
  }
  else {
    editor->releaseMouse();
    return KeepState;
  }
}

void MovingPixelsState::onCurrentToolChange(Editor* editor)
{
  ASSERT(m_pixelsMovement != NULL);

  tools::Tool* current_tool = editor->getCurrentEditorTool();

  // If the user changed the tool when he/she is moving pixels,
  // we have to drop the pixels only if the new tool is not selection...
  if (m_pixelsMovement &&
      (!current_tool->getInk(0)->isSelection() ||
       !current_tool->getInk(1)->isSelection())) {
    // We have to drop pixels
    dropPixels(editor);
  }
}

bool MovingPixelsState::onMouseDown(Editor* editor, MouseMessage* msg)
{
  ASSERT(m_pixelsMovement != NULL);

  // Set this editor as the active one and setup the ContextBar for
  // moving pixels. This is needed in case that the user is working
  // with a couple of Editors, in one is moving pixels and the other
  // one not.
  UIContext* ctx = UIContext::instance();
  ctx->setActiveView(editor->getDocumentView());

  ContextBar* contextBar = App::instance()->getMainWindow()->getContextBar();
  contextBar->updateForMovingPixels();

  // Start scroll loop
  if (checkForScroll(editor, msg))
    return true;

  Decorator* decorator = static_cast<Decorator*>(editor->decorator());
  Document* document = editor->document();

  // Transform selected pixels
  if (document->isMaskVisible() &&
      decorator->getTransformHandles(editor)) {
    TransformHandles* transfHandles = decorator->getTransformHandles(editor);

    // Get the handle covered by the mouse.
    HandleType handle = transfHandles->getHandleAtPoint(editor,
                                                        msg->position(),
                                                        getTransformation(editor));

    if (handle != NoHandle) {
      // Re-catch the image
      int x, y;
      editor->screenToEditor(msg->position().x, msg->position().y, &x, &y);
      m_pixelsMovement->catchImageAgain(x, y, handle);

      editor->captureMouse();
      return true;
    }
  }

  // Start "moving pixels" loop
  if (editor->isInsideSelection() && (msg->left() ||
                                      msg->right())) {
    // In case that the user is pressing the copy-selection keyboard shortcut.
    EditorCustomizationDelegate* customization = editor->getCustomizationDelegate();
    if (customization && customization->isCopySelectionKeyPressed()) {
      // Stamp the pixels to create the copy.
      m_pixelsMovement->stampImage();
    }

    // Re-catch the image
    int x, y;
    editor->screenToEditor(msg->position().x, msg->position().y, &x, &y);
    m_pixelsMovement->catchImageAgain(x, y, MoveHandle);

    editor->captureMouse();
    return true;
  }
  // End "moving pixels" loop
  else {
    // Drop pixels (e.g. to start drawing)
    dropPixels(editor);
  }

  // Use StandbyState implementation
  return StandbyState::onMouseDown(editor, msg);
}

bool MovingPixelsState::onMouseUp(Editor* editor, MouseMessage* msg)
{
  ASSERT(m_pixelsMovement != NULL);

  // Drop the image temporarily in this location (where the user releases the mouse)
  m_pixelsMovement->dropImageTemporarily();

  // Redraw the new pivot location.
  editor->invalidate();

  editor->releaseMouse();
  return true;
}

bool MovingPixelsState::onMouseMove(Editor* editor, MouseMessage* msg)
{
  ASSERT(m_pixelsMovement != NULL);

  // If there is a button pressed
  if (m_pixelsMovement->isDragging()) {
    // Auto-scroll
    gfx::Point mousePos = editor->autoScroll(msg, AutoScroll::MouseDir, false);

    // Get the position of the mouse in the sprite
    int x, y;
    editor->screenToEditor(mousePos.x, mousePos.y, &x, &y);

    // Get the customization for the pixels movement (snap to grid, angle snap, etc.).
    PixelsMovement::MoveModifier moveModifier = PixelsMovement::NormalMovement;

    if (editor->getCustomizationDelegate()->isSnapToGridKeyPressed())
      moveModifier |= PixelsMovement::SnapToGridMovement;

    if (editor->getCustomizationDelegate()->isAngleSnapKeyPressed())
      moveModifier |= PixelsMovement::AngleSnapMovement;

    if (editor->getCustomizationDelegate()->isMaintainAspectRatioKeyPressed())
      moveModifier |= PixelsMovement::MaintainAspectRatioMovement;

    if (editor->getCustomizationDelegate()->isLockAxisKeyPressed())
      moveModifier |= PixelsMovement::LockAxisMovement;

    // Invalidate handles
    Decorator* decorator = static_cast<Decorator*>(editor->decorator());
    TransformHandles* transfHandles = decorator->getTransformHandles(editor);
    transfHandles->invalidateHandles(editor, m_pixelsMovement->getTransformation());

    // Drag the image to that position
    m_pixelsMovement->moveImage(x, y, moveModifier);

    editor->updateStatusBar();
    return true;
  }

  // Use StandbyState implementation
  return StandbyState::onMouseMove(editor, msg);
}

bool MovingPixelsState::onSetCursor(Editor* editor)
{
  ASSERT(m_pixelsMovement != NULL);

  // Move selection
  if (m_pixelsMovement->isDragging()) {
    editor->hideDrawingCursor();
    jmouse_set_cursor(kMoveCursor);
    return true;
  }

  // Use StandbyState implementation
  return StandbyState::onSetCursor(editor);
}

bool MovingPixelsState::onKeyDown(Editor* editor, KeyMessage* msg)
{
  ASSERT(m_pixelsMovement != NULL);

  if (msg->scancode() == kKeyEnter || // TODO make this key customizable
      msg->scancode() == kKeyEnterPad ||
      msg->scancode() == kKeyEsc) {
    dropPixels(editor);

    // The escape key drop pixels and deselect the mask.
    if (msg->scancode() == kKeyEsc) { // TODO make this key customizable
      Command* cmd = CommandsModule::instance()->getCommandByName(CommandId::DeselectMask);
      UIContext::instance()->executeCommand(cmd);
    }

    return true;
  }
  else {
    Command* command = NULL;
    Params* params = NULL;
    if (KeyboardShortcuts::instance()
          ->getCommandFromKeyMessage(msg, &command, &params)) {
      // We accept zoom commands.
      if (strcmp(command->short_name(), CommandId::Zoom) == 0) {
        UIContext::instance()->executeCommand(command, params);
        return true;
      }
      // Intercept the "Cut" or "Copy" command to handle them locally
      // with the current m_pixelsMovement data.
      else if (strcmp(command->short_name(), CommandId::Cut) == 0 ||
               strcmp(command->short_name(), CommandId::Copy) == 0) {
        // Copy the floating image to the clipboard.
        {
          Document* document = editor->document();
          gfx::Point origin;
          base::UniquePtr<Image> floatingImage(m_pixelsMovement->getDraggedImageCopy(origin));
          clipboard::copy_image(floatingImage.get(),
                                document->sprite()->getPalette(editor->frame()),
                                origin);
        }

        // In case of "Cut" command.
        if (strcmp(command->short_name(), CommandId::Cut) == 0) {
          // Discard the dragged image.
          m_pixelsMovement->discardImage();
          m_discarded = true;

          // Quit from MovingPixelsState, back to standby.
          editor->backToPreviousState();
        }

        // Return true because we've used the keyboard shortcut.
        return true;
      }
      // Flip Horizontally/Vertically commands are handled manually to
      // avoid dropping the floating region of pixels.
      else if (strcmp(command->short_name(), CommandId::Flip) == 0) {
        if (FlipCommand* flipCommand = dynamic_cast<FlipCommand*>(command)) {
          flipCommand->loadParams(params);
          m_pixelsMovement->flipImage(flipCommand->getFlipType());
          return true;
        }
      }
    }
  }

  // Use StandbyState implementation
  return StandbyState::onKeyDown(editor, msg);
}

bool MovingPixelsState::onKeyUp(Editor* editor, KeyMessage* msg)
{
  ASSERT(m_pixelsMovement != NULL);

  // Use StandbyState implementation
  return StandbyState::onKeyUp(editor, msg);
}

bool MovingPixelsState::onUpdateStatusBar(Editor* editor)
{
  ASSERT(m_pixelsMovement != NULL);

  const gfx::Transformation& transform(getTransformation(editor));
  gfx::Size imageSize = m_pixelsMovement->getInitialImageSize();

  StatusBar::instance()->setStatusText
    (100, "Moving Pixels - Pos %d %d, Size %d %d, Orig: %3d %3d (%.02f%% %.02f%%), Angle %.1f",
     transform.bounds().x, transform.bounds().y,
     transform.bounds().w, transform.bounds().h,
     imageSize.w, imageSize.h,
     (double)transform.bounds().w*100.0/imageSize.w,
     (double)transform.bounds().h*100.0/imageSize.h,
     180.0 * transform.angle() / PI);

  return true;
}

bool MovingPixelsState::acceptQuickTool(tools::Tool* tool)
{
  return
    (!m_pixelsMovement ||
     tool->getInk(0)->isSelection() ||
     tool->getInk(0)->isEyedropper() ||
     tool->getInk(0)->isScrollMovement() ||
     tool->getInk(0)->isZoom());
}

// Before executing any command, we drop the pixels (go back to standby).
void MovingPixelsState::onBeforeCommandExecution(Command* command)
{
  // If the command is for other editor, we don't drop pixels.
  if (!isActiveEditor())
    return;

  // We don't need to drop the pixels if a MoveMaskCommand of Content is executed.
  if (MoveMaskCommand* moveMaskCmd = dynamic_cast<MoveMaskCommand*>(command)) {
    if (moveMaskCmd->getTarget() == MoveMaskCommand::Content)
      return;
  }
  else if ((strcmp(command->short_name(), CommandId::Zoom) == 0) ||
           (strcmp(command->short_name(), CommandId::Scroll) == 0)) {
    return;
  }

  if (m_pixelsMovement)
    dropPixels(m_editor);
}

void MovingPixelsState::onBeforeFrameChanged(Editor* editor)
{
  if (!isActiveDocument())
    return;

  if (m_pixelsMovement)
    dropPixels(m_editor);
}

void MovingPixelsState::onBeforeLayerChanged(Editor* editor)
{
  if (!isActiveDocument())
    return;

  if (m_pixelsMovement)
    dropPixels(m_editor);
}

void MovingPixelsState::onSetMoveTransparentColor(app::Color newColor)
{
  app::Color color = UIContext::instance()->settings()->selection()->getMoveTransparentColor();
  setTransparentColor(color);
}

void MovingPixelsState::onDropPixels(ContextBarObserver::DropAction action)
{
  if (!isActiveEditor())
    return;

  switch (action) {

    case ContextBarObserver::DropPixels:
      dropPixels(m_editor);
      break;

    case ContextBarObserver::CancelDrag:
      m_pixelsMovement->discardImage(false);
      m_discarded = true;

      // Quit from MovingPixelsState, back to standby.
      m_editor->backToPreviousState();
      break;
  }
}

void MovingPixelsState::setTransparentColor(const app::Color& color)
{
  ASSERT(m_pixelsMovement != NULL);

  Layer* layer = m_editor->layer();
  ASSERT(layer != NULL);

  m_pixelsMovement->setMaskColor(color_utils::color_for_layer(color, layer));
}

void MovingPixelsState::dropPixels(Editor* editor)
{
  // Just change to default state (StandbyState generally). We'll
  // receive an onBeforeChangeState() event after this call.
  editor->backToPreviousState();
}

gfx::Transformation MovingPixelsState::getTransformation(Editor* editor)
{
  return m_pixelsMovement->getTransformation();
}

bool MovingPixelsState::isActiveDocument() const
{
  Document* doc = UIContext::instance()->activeDocument();
  return (m_editor->document() == doc);
}

bool MovingPixelsState::isActiveEditor() const
{
  Editor* targetEditor = UIContext::instance()->activeEditor();
  return (targetEditor == m_editor);
}

} // namespace app
