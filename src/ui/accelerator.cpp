// Aseprite UI Library
// Copyright (C) 2001-2013  David Capello
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ui/accelerator.h"

#include "base/unique_ptr.h"
#include "base/split_string.h"
#include "base/string.h"

#include <cctype>
#include <string>
#include <vector>

// #define REPORT_KEYS
#define PREPROCESS_KEYS

namespace ui {

Accelerator::Accelerator()
  : m_modifiers(kKeyNoneModifier)
  , m_scancode(kKeyNil)
  , m_unicodeChar(0)
{
}

Accelerator::Accelerator(KeyModifiers modifiers, KeyScancode scancode, int unicodeChar)
  : m_modifiers(modifiers)
  , m_scancode(scancode)
  , m_unicodeChar(unicodeChar)
{
}

Accelerator::Accelerator(const std::string& str)
  : m_modifiers(kKeyNoneModifier)
  , m_scancode(kKeyNil)
  , m_unicodeChar(0)
{
  // Special case: plus sign
  if (str == "+") {
    m_unicodeChar = '+';
    return;
  }

  std::vector<std::string> tokens;
  base::split_string(str, tokens, "+");
  for (std::string tok : tokens) {
    tok = base::string_to_lower(tok);

    if (m_scancode == kKeySpace) {
      m_modifiers = (KeyModifiers)((int)m_modifiers | (int)kKeySpaceModifier);
      m_scancode = kKeyNil;
    }

    // Modifiers
    if (tok == "shift") {
      m_modifiers = (KeyModifiers)((int)m_modifiers | (int)kKeyShiftModifier);
    }
    else if (tok == "alt") {
      m_modifiers = (KeyModifiers)((int)m_modifiers | (int)kKeyAltModifier);
    }
    else if (tok == "ctrl") {
      m_modifiers = (KeyModifiers)((int)m_modifiers | (int)kKeyCtrlModifier);
    }
    else if (tok == "cmd") {
      m_modifiers = (KeyModifiers)((int)m_modifiers | (int)kKeyCmdModifier);
    }

    // Scancode

    // Word with one character
    else if (tok.size() == 1) {
      if ((tok[0] >= 'a') && (tok[0] <= 'z')) {
        m_unicodeChar = tok[0];
      }
      else {
        m_unicodeChar = tok[0];
      }

      if ((tok[0] >= 'a') && (tok[0] <= 'z'))
        m_scancode = (KeyScancode)((int)kKeyA + std::tolower(tok[0]) - 'a');
      else if ((tok[0] >= '0') && (tok[0] <= '9'))
        m_scancode = (KeyScancode)((int)kKey0 + tok[0] - '0');
      else {
        switch (tok[0]) {
          case '~': m_scancode = kKeyTilde; break;
          case '-': m_scancode = kKeyMinus; break;
          case '=': m_scancode = kKeyEquals; break;
          case '[': m_scancode = kKeyOpenbrace; break;
          case ']': m_scancode = kKeyClosebrace; break;
          case ';': m_scancode = kKeyColon; break;
          case '\'': m_scancode = kKeyQuote; break;
          case '\\': m_scancode = kKeyBackslash; break;
          case ',': m_scancode = kKeyComma; break;
          case '.': m_scancode = kKeyStop; break;
          case '/': m_scancode = kKeySlash; break;
          case '*': m_scancode = kKeyAsterisk; break;
        }
      }
    }
    // Other ones
    else {
      // F1, F2, ..., F11, F12
      if (tok[0] == 'f' && (tok.size() <= 3)) {
        int num = strtol(tok.c_str()+1, NULL, 10);
        if ((num >= 1) && (num <= 12))
          m_scancode = (KeyScancode)((int)kKeyF1 + num - 1);
      }
      else if ((tok == "escape") || (tok == "esc"))
        m_scancode = kKeyEsc;
      else if (tok == "backspace")
        m_scancode = kKeyBackspace;
      else if (tok == "tab")
        m_scancode = kKeyTab;
      else if (tok == "enter")
        m_scancode = kKeyEnter;
      else if (tok == "space")
        m_scancode = kKeySpace;
      else if ((tok == "insert") || (tok == "ins"))
        m_scancode = kKeyInsert;
      else if ((tok == "delete") || (tok == "del"))
        m_scancode = kKeyDel;
      else if (tok == "home")
        m_scancode = kKeyHome;
      else if (tok == "end")
        m_scancode = kKeyEnd;
      else if ((tok == "page up") || (tok == "pgup"))
        m_scancode = kKeyPageUp;
      else if ((tok == "page down") || (tok == "pgdn"))
        m_scancode = kKeyPageDown;
      else if (tok == "left")
        m_scancode = kKeyLeft;
      else if (tok == "right")
        m_scancode = kKeyRight;
      else if (tok == "up")
        m_scancode = kKeyUp;
      else if (tok == "down")
        m_scancode = kKeyDown;
      else if (tok == "0 pad")
        m_scancode = kKey0Pad;
      else if (tok == "1 pad")
        m_scancode = kKey1Pad;
      else if (tok == "2 pad")
        m_scancode = kKey2Pad;
      else if (tok == "3 pad")
        m_scancode = kKey3Pad;
      else if (tok == "4 pad")
        m_scancode = kKey4Pad;
      else if (tok == "5 pad")
        m_scancode = kKey5Pad;
      else if (tok == "6 pad")
        m_scancode = kKey6Pad;
      else if (tok == "7 pad")
        m_scancode = kKey7Pad;
      else if (tok == "8 pad")
        m_scancode = kKey8Pad;
      else if (tok == "9 pad")
        m_scancode = kKey9Pad;
      else if (tok == "slash pad")
        m_scancode = kKeySlashPad;
      else if (tok == "asterisk")
        m_scancode = kKeyAsterisk;
      else if (tok == "minus pad")
        m_scancode = kKeyMinusPad;
      else if (tok == "plus pad")
        m_scancode = kKeyPlusPad;
      else if (tok == "del pad")
        m_scancode = kKeyDelPad;
      else if (tok == "enter pad")
        m_scancode = kKeyEnterPad;
    }
  }
}

bool Accelerator::operator==(const Accelerator& other) const
{
  if (m_modifiers != other.m_modifiers)
    return false;

  if (m_scancode == other.m_scancode) {
    if (m_scancode != kKeyNil)
      return true;
    else if (m_unicodeChar != 0)
      return (std::tolower(m_unicodeChar) == std::tolower(other.m_unicodeChar));
  }

  return false;
}

bool Accelerator::isEmpty() const
{
  return
    (m_modifiers == kKeyNoneModifier &&
     m_scancode == kKeyNil &&
     m_unicodeChar == 0);
}

std::string Accelerator::toString() const
{
  // Same order that Allegro scancodes
  static const char* table[] = {
    NULL,
    "A",
    "B",
    "C",
    "D",
    "E",
    "F",
    "G",
    "H",
    "I",
    "J",
    "K",
    "L",
    "M",
    "N",
    "O",
    "P",
    "Q",
    "R",
    "S",
    "T",
    "U",
    "V",
    "W",
    "X",
    "Y",
    "Z",
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    "0 Pad",
    "1 Pad",
    "2 Pad",
    "3 Pad",
    "4 Pad",
    "5 Pad",
    "6 Pad",
    "7 Pad",
    "8 Pad",
    "9 Pad",
    "F1",
    "F2",
    "F3",
    "F4",
    "F5",
    "F6",
    "F7",
    "F8",
    "F9",
    "F10",
    "F11",
    "F12",
    "Esc",
    "~",
    "-",
    "=",
    "Backspace",
    "Tab",
    "[",
    "]",
    "Enter",
    ";",
    "\'",
    "\\",
    "KEY_BACKSLASH2",
    ",",
    ".",
    "/",
    "Space",
    "Ins",
    "Del",
    "Home",
    "End",
    "PgUp",
    "PgDn",
    "Left",
    "Right",
    "Up",
    "Down",
    "/ Pad",
    "* Pad",
    "- Pad",
    "+ Pad",
    "Delete Pad",
    "Enter Pad",
    "PrtScr",
    "Pause",
    "KEY_ABNT_C1",
    "Yen",
    "Kana",
    "KEY_CONVERT",
    "KEY_NOCONVERT",
    "KEY_AT",
    "KEY_CIRCUMFLEX",
    "KEY_COLON2",
    "Kanji",
  };
  static size_t table_size = sizeof(table) / sizeof(table[0]);

  std::string buf;

  // Shifts
  if (m_modifiers & kKeyCtrlModifier) buf += "Ctrl+";
  if (m_modifiers & kKeyCmdModifier) buf += "Cmd+";
  if (m_modifiers & kKeyAltModifier) buf += "Alt+";
  if (m_modifiers & kKeyShiftModifier) buf += "Shift+";
  if (m_modifiers & kKeySpaceModifier) buf += "Space+";

  // Key
  if (m_unicodeChar)
    buf += (wchar_t)toupper(m_unicodeChar);
  else if (m_scancode && m_scancode > 0 && m_scancode < (int)table_size)
    buf += table[m_scancode];
  else if (!buf.empty() && buf[buf.size()-1] == '+')
    buf.erase(buf.size()-1);

  return buf;
}

bool Accelerator::check(KeyModifiers modifiers, KeyScancode scancode, int unicodeChar) const
{
#ifdef REPORT_KEYS
  char buf[256];
  std::string buf2;
#endif

  // Preprocess the character to be compared with the accelerator
#ifdef PREPROCESS_KEYS
  // Directly scancode
  if ((scancode >= kKeyF1 && scancode <= kKeyF12) ||
      (scancode == kKeyEsc) ||
      (scancode == kKeyBackspace) ||
      (scancode == kKeyTab) ||
      (scancode == kKeyEnter) ||
      (scancode == kKeyBackslash) ||
      (scancode == kKeyBackslash2) ||
      (scancode >= kKeySpace && scancode <= kKeyDown) ||
      (scancode >= kKeyEnterPad && scancode <= kKeyNoconvert) ||
      (scancode == kKeyKanji)) {
    unicodeChar = 0;
  }
  // For Ctrl+number
  /*           scancode    unicodeChar
     Ctrl+0    27          0
     Ctrl+1    28          2
     Ctrl+2    29          0
     Ctrl+3    30          27
     Ctrl+4    31          28
     Ctrl+5    32          29
     Ctrl+6    33          30
     Ctrl+7    34          31
     Ctrl+8    35          127
     Ctrl+9    36          2
   */
  else if ((scancode >= kKey0 && scancode <= kKey9) &&
           (unicodeChar < 32 || unicodeChar == 127)) {
    unicodeChar = '0' + scancode - kKey0;
    scancode = kKeyNil;
  }
  // For Ctrl+letter
  else if (unicodeChar >= 1 && unicodeChar <= 'z'-'a'+1) {
    unicodeChar = 'a'+unicodeChar-1;
    scancode = kKeyNil;
  }
  // For any other legal Unicode code
  else if (unicodeChar >= ' ') {
    unicodeChar = std::tolower(unicodeChar);

    /* without shift (because characters like '*' can be trigger with
       "Shift+8", so we don't want "Shift+*") */
    if (!(unicodeChar >= 'a' && unicodeChar <= 'z'))
      modifiers = (KeyModifiers)((int)modifiers & ((int)~kKeyShiftModifier));

    scancode = kKeyNil;
  }
#endif

#ifdef REPORT_KEYS
  printf("%3d==%3d %3d==%3d %s==%s ",
    m_scancode, scancode,
    m_unicodeChar, unicodeChar,
    toString().c_str(),
    Accelerator(modifiers, scancode, unicodeChar).toString().c_str());
#endif

  if ((m_modifiers == modifiers) &&
      ((m_scancode != kKeyNil && m_scancode == scancode) ||
       (m_unicodeChar && m_unicodeChar == unicodeChar))) {
#ifdef REPORT_KEYS
    printf("true\n");
#endif
    return true;
  }

#ifdef REPORT_KEYS
  printf("false\n");
#endif
  return false;
}

bool Accelerator::checkFromAllegroKeyArray() const
{
  KeyModifiers modifiers = kKeyNoneModifier;

  if (she::is_key_pressed(kKeyLShift)  ) modifiers = (KeyModifiers)((int)modifiers | (int)kKeyShiftModifier);
  if (she::is_key_pressed(kKeyRShift)  ) modifiers = (KeyModifiers)((int)modifiers | (int)kKeyShiftModifier);
  if (she::is_key_pressed(kKeyLControl)) modifiers = (KeyModifiers)((int)modifiers | (int)kKeyCtrlModifier);
  if (she::is_key_pressed(kKeyRControl)) modifiers = (KeyModifiers)((int)modifiers | (int)kKeyCtrlModifier);
  if (she::is_key_pressed(kKeyAlt)     ) modifiers = (KeyModifiers)((int)modifiers | (int)kKeyAltModifier);
  if (she::is_key_pressed(kKeyCommand) ) modifiers = (KeyModifiers)((int)modifiers | (int)kKeyCmdModifier);

  return ((m_scancode == 0 || she::is_key_pressed(m_scancode)) &&
          (m_modifiers == modifiers));
}

} // namespace ui
