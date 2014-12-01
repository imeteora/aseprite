/* Aseprite
 * Copyright (C) 2001-2014 David Capello
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

#ifdef __ASE_CONFIG_H
#error You cannot use config.h two times
#endif

#define __ASE_CONFIG_H

// In MSVC
#ifdef _MSC_VER
  // Avoid warnings about insecure standard C++ functions
  #define _CRT_SECURE_NO_WARNINGS

  // Disable warning C4355 in MSVC: 'this' used in base member initializer list
  #pragma warning(disable:4355)
#endif

// General information
#define PACKAGE                 "Aseprite"
#define VERSION "1.1.0-dev"
#ifdef CUSTOM_WEBSITE_URL
#define WEBSITE                 CUSTOM_WEBSITE_URL // To test web server
#else
#define WEBSITE                 "http://www.aseprite.org/"
#endif
#define WEBSITE_DOWNLOAD        WEBSITE "download/"
#define WEBSITE_CONTRIBUTORS    WEBSITE "contributors/"
#define UPDATE_URL              WEBSITE "update/?xml=1"
#define COPYRIGHT               "Copyright (C) 2001-2014 David Capello"

#define PRINTF                  verbose_printf

// verbose_printf is defined in src/app/log.cpp and used through PRINTF macro
void verbose_printf(const char* format, ...);

#include <math.h>
#undef PI
#define PI 3.14159265358979323846

#include <allegro/base.h>
#include <allegro/debug.h>
#undef ASSERT
#undef TRACE

#include "base/debug.h"

//////////////////////////////////////////////////////////////////////
// Overloaded new/delete operators to detect memory-leaks

#if defined __cplusplus && defined MEMLEAK

#include <new>
#include "base/memory.h"

inline void* operator new(std::size_t size)
{
  void* ptr = base_malloc(size);
  if (!ptr)
    throw std::bad_alloc();
  return ptr;
}

inline void operator delete(void* ptr)
{
  if (!ptr)
    return;
  base_free(ptr);
}

inline void* operator new[](std::size_t size)
{
  void* ptr = base_malloc(size);
  if (!ptr)
    throw std::bad_alloc();
  return ptr;
}

inline void operator delete[](void* ptr)
{
  if (!ptr)
    return;
  base_free(ptr);
}

#endif
