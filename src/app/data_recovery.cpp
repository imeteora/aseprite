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

#include "app/data_recovery.h"

#include "app/backup.h"
#include "app/document.h"
#include "app/ini_file.h"
#include "app/ui_context.h"
#include "base/fs.h"
#include "base/path.h"
#include "base/temp_dir.h"

namespace app {

DataRecovery::DataRecovery(doc::Context* context)
  : m_tempDir(NULL)
  , m_backup(NULL)
  , m_context(context)
{
  // Check if there is already data to recover
  const std::string existent_data_path = get_config_string("DataRecovery", "Path", "");
  if (!existent_data_path.empty() &&
      base::is_directory(existent_data_path)) {
    // Load the backup data.
    m_tempDir = new base::TempDir();
    m_tempDir->attach(existent_data_path);
    m_backup = new Backup(existent_data_path);
  }
  else {
    // Create a new directory to save the backup information.
    m_tempDir = new base::TempDir(PACKAGE);
    m_backup = new Backup(m_tempDir->path());

    set_config_string("DataRecovery", "Path", m_tempDir->path().c_str());
    flush_config_file();
  }

  m_context->addObserver(this);
  m_context->documents().addObserver(this);
}

DataRecovery::~DataRecovery()
{
  m_context->documents().removeObserver(this);
  m_context->removeObserver(this);

  delete m_backup;

  if (m_tempDir) {
    delete m_tempDir;
    set_config_string("DataRecovery", "Path", "");
  }
}

void DataRecovery::onAddDocument(doc::Document* document)
{
  document->addObserver(this);
}

void DataRecovery::onRemoveDocument(doc::Document* document)
{
  document->removeObserver(this);
}

} // namespace app
