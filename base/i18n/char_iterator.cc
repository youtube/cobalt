// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/char_iterator.h"

#include "unicode/utf8.h"
#include "unicode/utf16.h"

namespace base {

UTF8CharIterator::UTF8CharIterator(const std::string* str)
    : str_(reinterpret_cast<const uint8_t*>(str->data())),
      len_(str->size()),
      array_pos_(0),
      next_pos_(0),
      char_pos_(0),
      char_(0) {
  if (len_)
    U8_NEXT(str_, next_pos_, len_, char_);
}

bool UTF8CharIterator::Advance() {
  if (array_pos_ >= len_)
    return false;

  array_pos_ = next_pos_;
  char_pos_++;
  if (next_pos_ < len_)
    U8_NEXT(str_, next_pos_, len_, char_);

  return true;
}

UTF16CharIterator::UTF16CharIterator(const string16* str)
    : str_(reinterpret_cast<const char16*>(str->data())),
      len_(str->size()),
      array_pos_(0),
      next_pos_(0),
      char_pos_(0),
      char_(0) {
  if (len_)
    U16_NEXT(str_, next_pos_, len_, char_);
}

bool UTF16CharIterator::Advance() {
  if (array_pos_ >= len_)
    return false;

  array_pos_ = next_pos_;
  char_pos_++;
  if (next_pos_ < len_)
    U16_NEXT(str_, next_pos_, len_, char_);

  return true;
}

}  // namespace base
