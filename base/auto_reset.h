// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_AUTO_RESET_H_
#define BASE_AUTO_RESET_H_

#include "base/basictypes.h"

// AutoReset is useful for setting a variable to some value only during a
// particular scope.  If you have code that has to add "var = false;" or
// "var = old_var;" at all the exit points of a block, for example, you would
// benefit from using this instead.
//
// NOTE: Right now this is hardcoded to work on bools, since that covers all the
// cases where we've used it.  It would be reasonable to turn it into a template
// class in the future.

class AutoReset {
 public:
  explicit AutoReset(bool* scoped_variable, bool new_value)
      : scoped_variable_(scoped_variable),
        original_value_(*scoped_variable) {
    *scoped_variable_ = new_value;
  }
  ~AutoReset() { *scoped_variable_ = original_value_; }

 private:
  bool* scoped_variable_;
  bool original_value_;

  DISALLOW_COPY_AND_ASSIGN(AutoReset);
};

#endif  // BASE_AUTO_RESET_H_
