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

#include "app/file/file.h"

#include "app/console.h"
#include "app/context.h"
#include "app/document.h"
#include "app/file/file_format.h"
#include "app/file/file_formats_manager.h"
#include "app/file/format_options.h"
#include "app/file/split_filename.h"
#include "app/modules/gui.h"
#include "app/modules/palettes.h"
#include "app/ui/status_bar.h"
#include "base/fs.h"
#include "base/mutex.h"
#include "base/path.h"
#include "base/scoped_lock.h"
#include "base/shared_ptr.h"
#include "base/string.h"
#include "doc/quantization.h"
#include "doc/doc.h"
#include "ui/alert.h"

#include <cstring>

namespace app {

using namespace base;

static FileOp* fop_new(FileOpType type, Context* context);
static void fop_prepare_for_sequence(FileOp* fop);

void get_readable_extensions(char* buf, int size)
{
  FileFormatsList::iterator it = FileFormatsManager::instance()->begin();
  FileFormatsList::iterator end = FileFormatsManager::instance()->end();

  // Clear the string
  strncpy(buf, "", size);

  // Insert file format
  for (; it != end; ++it) {
    if ((*it)->support(FILE_SUPPORT_LOAD)) {
      if (*buf) strncat(buf, ",", size);
      strncat(buf, (*it)->extensions(), size);
    }
  }
}

void get_writable_extensions(char* buf, int size)
{
  FileFormatsList::iterator it = FileFormatsManager::instance()->begin();
  FileFormatsList::iterator end = FileFormatsManager::instance()->end();

  // Clear the string
  strncpy(buf, "", size);

  // Insert file format
  for (; it != end; ++it) {
    if ((*it)->support(FILE_SUPPORT_SAVE)) {
      if (*buf) strncat(buf, ",", size);
      strncat(buf, (*it)->extensions(), size);
    }
  }
}

Document* load_document(Context* context, const char* filename)
{
  Document* document;

  /* TODO add a option to configure what to do with the sequence */
  FileOp *fop = fop_to_load_document(context, filename, FILE_LOAD_SEQUENCE_NONE);
  if (!fop)
    return NULL;

  /* operate in this same thread */
  fop_operate(fop, NULL);
  fop_done(fop);

  fop_post_load(fop);

  if (fop->has_error()) {
    Console console;
    console.printf(fop->error.c_str());
  }

  document = fop->document;
  fop_free(fop);

  if (document && context)
    document->setContext(context);

  return document;
}

int save_document(Context* context, doc::Document* document)
{
  ASSERT(dynamic_cast<app::Document*>(document));

  int ret;
  FileOp* fop = fop_to_save_document(context, static_cast<app::Document*>(document));
  if (!fop)
    return -1;

  /* operate in this same thread */
  fop_operate(fop, NULL);
  fop_done(fop);

  if (fop->has_error()) {
    Console console;
    console.printf(fop->error.c_str());
  }

  ret = (!fop->has_error() ? 0: -1);
  fop_free(fop);

  return ret;
}

FileOp* fop_to_load_document(Context* context, const char* filename, int flags)
{
  FileOp *fop;

  fop = fop_new(FileOpLoad, context);
  if (!fop)
    return NULL;

  // Get the extension of the filename (in lower case)
  std::string extension = base::string_to_lower(base::get_file_extension(filename));

  PRINTF("Loading file \"%s\" (%s)\n", filename, extension.c_str());

  // Does file exist?
  if (!base::is_file(filename)) {
    fop_error(fop, "File not found: \"%s\"\n", filename);
    goto done;
  }

  // Get the format through the extension of the filename
  fop->format = FileFormatsManager::instance()
    ->getFileFormatByExtension(extension.c_str());

  if (!fop->format ||
      !fop->format->support(FILE_SUPPORT_LOAD)) {
    fop_error(fop, "ASEPRITE can't load \"%s\" files\n", extension.c_str());
    goto done;
  }

  /* use the "sequence" interface */
  if (fop->format->support(FILE_SUPPORT_SEQUENCES)) {
    /* prepare to load a sequence */
    fop_prepare_for_sequence(fop);

    /* per now, we want load just one file */
    fop->seq.filename_list.push_back(filename);

    /* don't load the sequence (just the one file/one frame) */
    if (!(flags & FILE_LOAD_SEQUENCE_NONE)) {
      std::string left, right;
      int c, width, start_from;
      char buf[512];

      /* first of all, we must generate the list of files to load in the
         sequence... */

      // Check is this could be a sequence
      start_from = split_filename(filename, left, right, width);
      if (start_from >= 0) {
        // Try to get more file names
        for (c=start_from+1; ; c++) {
          // Get the next file name
          sprintf(buf, "%s%0*d%s", left.c_str(), width, c, right.c_str());

          // If the file doesn't exist, we doesn't need more files to load
          if (!base::is_file(buf))
            break;

          /* add this file name to the list */
          fop->seq.filename_list.push_back(buf);
        }
      }

      /* TODO add a better dialog to edit file-names */
      if ((flags & FILE_LOAD_SEQUENCE_ASK) && context && context->isUiAvailable()) {
        /* really want load all files? */
        if ((fop->seq.filename_list.size() > 1) &&
            (ui::Alert::show("Notice"
              "<<Possible animation with:"
              "<<%s, %s..."
              "<<Do you want to load the sequence of bitmaps?"
              "||&Agree||&Skip",
              base::get_file_name(fop->seq.filename_list[0]).c_str(),
              base::get_file_name(fop->seq.filename_list[1]).c_str()) != 1)) {

          // If the user replies "Skip", we need just one file name
          // (the first one).
          if (fop->seq.filename_list.size() > 1) {
            fop->seq.filename_list.erase(fop->seq.filename_list.begin()+1,
                                         fop->seq.filename_list.end());
          }
        }
      }
    }
  }
  else
    fop->filename = filename;

  /* load just one frame */
  if (flags & FILE_LOAD_ONE_FRAME)
    fop->oneframe = true;

done:;
  return fop;
}

FileOp* fop_to_save_document(Context* context, Document* document)
{
  FileOp *fop;
  bool fatal;

  fop = fop_new(FileOpSave, context);
  if (!fop)
    return NULL;

  // Document to save
  fop->document = document;

  // Get the extension of the filename (in lower case)
  std::string extension = base::string_to_lower(base::get_file_extension(fop->document->filename()));

  PRINTF("Saving document \"%s\" (%s)\n", fop->document->filename().c_str(), extension.c_str());

  // Get the format through the extension of the filename
  fop->format = FileFormatsManager::instance()
    ->getFileFormatByExtension(extension.c_str());

  if (!fop->format ||
      !fop->format->support(FILE_SUPPORT_SAVE)) {
    fop_error(fop, "ASEPRITE can't save \"%s\" files\n", extension.c_str());
    return fop;
  }

  // Warnings
  std::string warnings;
  fatal = false;

  /* check image type support */
  switch (fop->document->sprite()->pixelFormat()) {

    case IMAGE_RGB:
      if (!(fop->format->support(FILE_SUPPORT_RGB))) {
        warnings += "<<- RGB format";
        fatal = true;
      }

      if (!(fop->format->support(FILE_SUPPORT_RGBA)) &&
          fop->document->sprite()->needAlpha()) {

        warnings += "<<- Alpha channel";
      }
      break;

    case IMAGE_GRAYSCALE:
      if (!(fop->format->support(FILE_SUPPORT_GRAY))) {
        warnings += "<<- Grayscale format";
        fatal = true;
      }
      if (!(fop->format->support(FILE_SUPPORT_GRAYA)) &&
          fop->document->sprite()->needAlpha()) {

        warnings += "<<- Alpha channel";
      }
      break;

    case IMAGE_INDEXED:
      if (!(fop->format->support(FILE_SUPPORT_INDEXED))) {
        warnings += "<<- Indexed format";
        fatal = true;
      }
      break;
  }

  // check frames support
  if (fop->document->sprite()->totalFrames() > 1) {
    if (!fop->format->support(FILE_SUPPORT_FRAMES) &&
        !fop->format->support(FILE_SUPPORT_SEQUENCES)) {
      warnings += "<<- Frames";
    }
  }

  // layers support
  if (fop->document->sprite()->folder()->getLayersCount() > 1) {
    if (!(fop->format->support(FILE_SUPPORT_LAYERS))) {
      warnings += "<<- Layers";
    }
  }

  // Palettes support.
  if (fop->document->sprite()->getPalettes().size() > 1) {
    if (!fop->format->support(FILE_SUPPORT_PALETTES) &&
        !fop->format->support(FILE_SUPPORT_SEQUENCES)) {
      warnings += "<<- Palette changes between frames";
    }
  }

  // Show the confirmation alert
  if (!warnings.empty()) {
    // Interative
    if (context && context->isUiAvailable()) {
      int ret;

      if (fatal)
        ret = ui::Alert::show("Error<<File format \"%s\" doesn't support:%s"
                              "||&Close",
                              fop->format->name(), warnings.c_str());
      else
        ret = ui::Alert::show("Warning<<File format \"%s\" doesn't support:%s"
                              "<<Do you want continue?"
                              "||&Yes||&No",
                              fop->format->name(), warnings.c_str());

      /* operation can't be done (by fatal error) or the user cancel
         the operation */
      if ((fatal) || (ret != 1)) {
        fop_free(fop);
        return NULL;
      }
    }
    // No interactive & fatal error?
    else if (fatal) {
      fop_error(fop, warnings.c_str());
      return fop;
    }
  }

  // Use the "sequence" interface.
  if (fop->format->support(FILE_SUPPORT_SEQUENCES)) {
    fop_prepare_for_sequence(fop);

    // To save one frame
    if (fop->document->sprite()->totalFrames() == 1) {
      fop->seq.filename_list.push_back(fop->document->filename());
    }
    // To save more frames
    else {
      std::string left, right;
      int width, start_from;

      start_from = split_filename(fop->document->filename().c_str(), left, right, width);
      if (start_from < 0) {
        start_from = 1;
        width = 1;
      }

      for (FrameNumber frame(0); frame<fop->document->sprite()->totalFrames(); ++frame) {
        // Get the name for this frame
        char buf[4096];
        sprintf(buf, "%s%0*d%s", left.c_str(), width, start_from+frame, right.c_str());
        fop->seq.filename_list.push_back(buf);
      }
    }
  }
  else
    fop->filename = fop->document->filename();

  // Configure output format?
  if (fop->format->support(FILE_SUPPORT_GET_FORMAT_OPTIONS)) {
    SharedPtr<FormatOptions> format_options = fop->format->getFormatOptions(fop);

    // Does the user cancelled the operation?
    if (format_options == NULL) {
      fop_free(fop);
      return NULL;
    }

    fop->seq.format_options = format_options;
    fop->document->setFormatOptions(format_options);
  }

  return fop;
}

// Executes the file operation: loads or saves the sprite.
//
// It can be called from a different thread of the one used
// by fop_to_load_sprite() or fop_to_save_sprite().
//
// After this function you must to mark the "fop" as "done" calling
// fop_done() function.
void fop_operate(FileOp *fop, IFileOpProgress* progress)
{
  ASSERT(fop != NULL);
  ASSERT(!fop_is_done(fop));

  fop->progressInterface = progress;

  // Load //////////////////////////////////////////////////////////////////////
  if (fop->type == FileOpLoad &&
      fop->format != NULL &&
      fop->format->support(FILE_SUPPORT_LOAD)) {
    // Load a sequence
    if (fop->is_sequence()) {
      int image_index = 0;
      Image* old_image;
      bool loadres;

      // Default palette
      fop->seq.palette->makeBlack();

      // TODO set_palette for each frame???
#define SEQUENCE_IMAGE()                                                \
      do {                                                              \
        image_index = fop->document                                     \
          ->sprite()                                                    \
          ->stock()->addImage(fop->seq.image);                       \
                                                                        \
        fop->seq.last_cel->setImage(image_index);                       \
        fop->seq.layer->addCel(fop->seq.last_cel);                      \
                                                                        \
        if (fop->document->sprite()->getPalette(frame)                  \
              ->countDiff(fop->seq.palette, NULL, NULL) > 0) {          \
          fop->seq.palette->setFrame(frame);                            \
          fop->document->sprite()->setPalette(fop->seq.palette, true);  \
        }                                                               \
                                                                        \
        old_image = fop->seq.image;                                     \
        fop->seq.image = NULL;                                          \
        fop->seq.last_cel = NULL;                                       \
      } while (0)

      /* load the sequence */
      FrameNumber frames(fop->seq.filename_list.size());
      FrameNumber frame(0);
      old_image = NULL;

      fop->seq.has_alpha = false;
      fop->seq.progress_offset = 0.0f;
      fop->seq.progress_fraction = 1.0f / (double)frames;

      std::vector<std::string>::iterator it = fop->seq.filename_list.begin();
      std::vector<std::string>::iterator end = fop->seq.filename_list.end();
      for (; it != end; ++it) {
        fop->filename = it->c_str();

        // Call the "load" procedure to read the first bitmap.
        loadres = fop->format->load(fop);
        if (!loadres) {
          fop_error(fop, "Error loading frame %d from file \"%s\"\n",
                    frame+1, fop->filename.c_str());
        }

        // For the first frame...
        if (!old_image) {
          // Error reading the first frame
          if (!loadres || !fop->document || !fop->seq.last_cel) {
            delete fop->seq.image;
            delete fop->seq.last_cel;
            delete fop->document;
            fop->document = NULL;
            break;
          }
          // Read ok
          else {
            // Add the keyframe
            SEQUENCE_IMAGE();
          }
        }
        // For other frames
        else {
          // All done (or maybe not enough memory)
          if (!loadres || !fop->seq.last_cel) {
            delete fop->seq.image;
            delete fop->seq.last_cel;
            break;
          }

          // Compare the old frame with the new one
#if USE_LINK // TODO this should be configurable through a check-box
          if (count_diff_between_images(old_image, fop->seq.image)) {
            SEQUENCE_IMAGE();
          }
          // We don't need this image
          else {
            delete fop->seq.image;

            // But add a link frame
            fop->seq.last_cel->image = image_index;
            layer_add_frame(fop->seq.layer, fop->seq.last_cel);

            fop->seq.last_image = NULL;
            fop->seq.last_cel = NULL;
          }
#else
          SEQUENCE_IMAGE();
#endif
        }

        ++frame;
        fop->seq.progress_offset += fop->seq.progress_fraction;
      }
      fop->filename = *fop->seq.filename_list.begin();

      // Final setup
      if (fop->document != NULL) {
        // Configure the layer as the 'Background'
        if (!fop->seq.has_alpha)
          fop->seq.layer->configureAsBackground();

        // Set the frames range
        fop->document->sprite()->setTotalFrames(frame);

        // Sets special options from the specific format (e.g. BMP
        // file can contain the number of bits per pixel).
        fop->document->setFormatOptions(fop->seq.format_options);
      }
    }
    // Direct load from one file.
    else {
      // Call the "load" procedure.
      if (!fop->format->load(fop))
        fop_error(fop, "Error loading sprite from file \"%s\"\n",
                  fop->filename.c_str());
    }
  }
  // Save //////////////////////////////////////////////////////////////////////
  else if (fop->type == FileOpSave &&
           fop->format != NULL &&
           fop->format->support(FILE_SUPPORT_SAVE)) {
#ifdef ENABLE_SAVE
    // Save a sequence
    if (fop->is_sequence()) {
      ASSERT(fop->format->support(FILE_SUPPORT_SEQUENCES));

      Sprite* sprite = fop->document->sprite();

      // Create a temporary bitmap
      fop->seq.image = Image::create(sprite->pixelFormat(),
                                     sprite->width(),
                                     sprite->height());
      if (fop->seq.image != NULL) {
        fop->seq.progress_offset = 0.0f;
        fop->seq.progress_fraction = 1.0f / (double)sprite->totalFrames();

        // For each frame in the sprite.
        for (FrameNumber frame(0); frame < sprite->totalFrames(); ++frame) {
          // Draw the "frame" in "fop->seq.image"
          sprite->render(fop->seq.image, 0, 0, frame);

          // Setup the palette.
          sprite->getPalette(frame)->copyColorsTo(fop->seq.palette);

          // Setup the filename to be used.
          fop->filename = fop->seq.filename_list[frame];

          // Call the "save" procedure... did it fail?
          if (!fop->format->save(fop)) {
            fop_error(fop, "Error saving frame %d in the file \"%s\"\n",
                      frame+1, fop->filename.c_str());
            break;
          }

          fop->seq.progress_offset += fop->seq.progress_fraction;
        }
        fop->filename = *fop->seq.filename_list.begin();

        // Destroy the image
        delete fop->seq.image;
      }
      else {
        fop_error(fop, "Not enough memory for the temporary bitmap.\n");
      }
    }
    // Direct save to a file.
    else {
      // Call the "save" procedure.
      if (!fop->format->save(fop))
        fop_error(fop, "Error saving the sprite in the file \"%s\"\n",
                  fop->filename.c_str());
    }
#else
    fop_error(fop,
      "Save operation is not supported in trial version.\n"
      "Go to " WEBSITE_DOWNLOAD " and get the full-version.");
#endif
  }

  // Progress = 100%
  fop_progress(fop, 1.0f);
}

// After mark the 'fop' as 'done' you must to free it calling fop_free().
void fop_done(FileOp *fop)
{
  // Finally done.
  scoped_lock lock(*fop->mutex);
  fop->done = true;
}

void fop_stop(FileOp *fop)
{
  scoped_lock lock(*fop->mutex);
  if (!fop->done)
    fop->stop = true;
}

FileOp::~FileOp()
{
  if (this->format)
    this->format->destroyData(this);

  delete this->seq.palette;
  delete this->mutex;
}

void FileOp::createDocument(Sprite* spr)
{
  // spr can be NULL if the sprite is set in onPostLoad() then

  ASSERT(this->document == NULL);
  this->document = new Document(spr);
}

void fop_free(FileOp *fop)
{
  delete fop;
}

void fop_post_load(FileOp* fop)
{
  if (fop->document == NULL)
    return;

  // Set the filename.
  if (fop->is_sequence())
    fop->document->setFilename(fop->seq.filename_list.begin()->c_str());
  else
    fop->document->setFilename(fop->filename.c_str());

  bool result = fop->format->postLoad(fop);
  if (!result) {
    // Destroy the document
    delete fop->document;
    fop->document = NULL;

    return;
  }

  if (fop->document->sprite() != NULL) {
    // Creates a suitable palette for RGB images
    if (fop->document->sprite()->pixelFormat() == IMAGE_RGB &&
        fop->document->sprite()->getPalettes().size() <= 1 &&
        fop->document->sprite()->getPalette(FrameNumber(0))->isBlack()) {
      SharedPtr<Palette> palette
        (quantization::create_palette_from_rgb(
          fop->document->sprite(),
          FrameNumber(0), NULL));

      fop->document->sprite()->resetPalettes();
      fop->document->sprite()->setPalette(palette, false);
    }
  }

  fop->document->markAsSaved();
}

void fop_sequence_set_format_options(FileOp* fop, const SharedPtr<FormatOptions>& format_options)
{
  ASSERT(fop->seq.format_options == NULL);
  fop->seq.format_options = format_options;
}

void fop_sequence_set_color(FileOp *fop, int index, int r, int g, int b)
{
  fop->seq.palette->setEntry(index, rgba(r, g, b, 255));
}

void fop_sequence_get_color(FileOp *fop, int index, int *r, int *g, int *b)
{
  uint32_t c = fop->seq.palette->getEntry(index);

  *r = rgba_getr(c);
  *g = rgba_getg(c);
  *b = rgba_getb(c);
}

Image* fop_sequence_image(FileOp* fop, PixelFormat pixelFormat, int w, int h)
{
  Sprite* sprite;

  // Create the image
  if (!fop->document) {
    sprite = new Sprite(pixelFormat, w, h, 256);
    try {
      LayerImage* layer = new LayerImage(sprite);

      // Add the layer
      sprite->folder()->addLayer(layer);

      // Done
      fop->createDocument(sprite);
      fop->seq.layer = layer;
    }
    catch (...) {
      delete sprite;
      throw;
    }
  }
  else {
    sprite = fop->document->sprite();

    if (sprite->pixelFormat() != pixelFormat)
      return NULL;
  }

  if (fop->seq.last_cel) {
    fop_error(fop, "Error: called two times \"fop_sequence_image()\".\n");
    return NULL;
  }

  // Create a bitmap
  Image* image = Image::create(pixelFormat, w, h);

  fop->seq.image = image;
  fop->seq.last_cel = new Cel(fop->seq.frame++, 0);

  return image;
}

void fop_error(FileOp *fop, const char *format, ...)
{
  char buf_error[4096];
  va_list ap;
  va_start(ap, format);
  vsnprintf(buf_error, sizeof(buf_error), format, ap);
  va_end(ap);

  // Concatenate the new error
  {
    scoped_lock lock(*fop->mutex);
    fop->error += buf_error;
  }
}

void fop_progress(FileOp *fop, double progress)
{
  scoped_lock lock(*fop->mutex);

  if (fop->is_sequence()) {
    fop->progress =
      fop->seq.progress_offset +
      fop->seq.progress_fraction*progress;
  }
  else {
    fop->progress = progress;
  }

  if (fop->progressInterface)
    fop->progressInterface->ackFileOpProgress(progress);
}

double fop_get_progress(FileOp *fop)
{
  double progress;
  {
    scoped_lock lock(*fop->mutex);
    progress = fop->progress;
  }
  return progress;
}

// Returns true when the file operation finished, this means, when the
// fop_operate() routine ends.
bool fop_is_done(FileOp *fop)
{
  bool done;
  {
    scoped_lock lock(*fop->mutex);
    done = fop->done;
  }
  return done;
}

bool fop_is_stop(FileOp *fop)
{
  bool stop;
  {
    scoped_lock lock(*fop->mutex);
    stop = fop->stop;
  }
  return stop;
}

static FileOp* fop_new(FileOpType type, Context* context)
{
  FileOp* fop = new FileOp;

  fop->type = type;
  fop->format = NULL;
  fop->format_data = NULL;
  fop->context = context;
  fop->document = NULL;

  fop->mutex = new base::mutex();
  fop->progress = 0.0f;
  fop->progressInterface = NULL;
  fop->done = false;
  fop->stop = false;
  fop->oneframe = false;

  fop->seq.palette = NULL;
  fop->seq.image = NULL;
  fop->seq.progress_offset = 0.0f;
  fop->seq.progress_fraction = 0.0f;
  fop->seq.frame = FrameNumber(0);
  fop->seq.layer = NULL;
  fop->seq.last_cel = NULL;

  return fop;
}

static void fop_prepare_for_sequence(FileOp* fop)
{
  fop->seq.palette = new Palette(FrameNumber(0), 256);
  fop->seq.format_options.reset();
}

} // namespace app
