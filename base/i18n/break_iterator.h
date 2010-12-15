// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_BREAK_ITERATOR_H_
#define BASE_I18N_BREAK_ITERATOR_H_
#pragma once

#include "base/basictypes.h"
#include "base/string16.h"

// The BreakIterator class iterates through the words and word breaks
// in a UTF-16 string.
//
// It provides two modes, BREAK_WORD and BREAK_SPACE, which modify how
// trailing non-word characters are aggregated into the returned word.
//
// Under BREAK_WORD mode (more common), the non-word characters are
// not included with a returned word (e.g. in the UTF-16 equivalent of
// the string " foo bar! ", the word breaks are at the periods in
// ". .foo. .bar.!. .").
//
// Under BREAK_SPACE mode (less common), the non-word characters are
// included in the word, breaking only when a space-equivalent character
// is encountered (e.g. in the UTF16-equivalent of the string " foo bar! ",
// the word breaks are at the periods in ". .foo .bar! .").
//
// To extract the words from a string, move a BREAK_WORD BreakIterator
// through the string and test whether IsWord() is true.  E.g.,
//   BreakIterator iter(&str, BreakIterator::BREAK_WORD);
//   if (!iter.Init()) return false;
//   while (iter.Advance()) {
//     if (iter.IsWord()) {
//       // region [iter.prev(),iter.pos()) contains a word.
//       VLOG(1) << "word: " << iter.GetString();
//     }
//   }

namespace base {

class BreakIterator {
 public:
  enum BreakType {
    BREAK_WORD,
    BREAK_SPACE
  };

  // Requires |str| to live as long as the BreakIterator does.
  BreakIterator(const string16* str, BreakType break_type);
  ~BreakIterator();

  // Init() must be called before any of the iterators are valid.
  // Returns false if ICU failed to initialize.
  bool Init();

  // Return the current break position within the string,
  // or BreakIterator::npos when done.
  size_t pos() const { return pos_; }
  // Return the value of pos() returned before Advance() was last called.
  size_t prev() const { return prev_; }

  // Advance to the next break.  Returns false if we've run past the end of
  // the string.  (Note that the very last "word break" is after the final
  // character in the string, and when we advance to that position it's the
  // last time Advance() returns true.)
  bool Advance();

  // Under BREAK_WORD mode, returns true if the break we just hit is the
  // end of a word. (Otherwise, the break iterator just skipped over e.g.
  // whitespace or punctuation.)  Under BREAK_SPACE mode, this distinction
  // doesn't apply and it always retuns false.
  bool IsWord() const;

  // Return the string between prev() and pos().
  // Advance() must have been called successfully at least once
  // for pos() to have advanced to somewhere useful.
  string16 GetString() const;

 private:
  // ICU iterator, avoiding ICU ubrk.h dependence.
  // This is actually an ICU UBreakiterator* type, which turns out to be
  // a typedef for a void* in the ICU headers. Using void* directly prevents
  // callers from needing access to the ICU public headers directory.
  void* iter_;

  // The string we're iterating over.
  const string16* string_;

  // The breaking style (word/line).
  BreakType break_type_;

  // Previous and current iterator positions.
  size_t prev_, pos_;

  DISALLOW_COPY_AND_ASSIGN(BreakIterator);
};

}  // namespace base

#endif  // BASE_I18N_BREAK_ITERATOR_H__
