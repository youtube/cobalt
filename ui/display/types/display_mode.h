// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_TYPES_DISPLAY_MODE_H_
#define UI_DISPLAY_TYPES_DISPLAY_MODE_H_

#include <memory>
#include <ostream>
#include <string>

#include "ui/display/types/display_types_export.h"
#include "ui/gfx/geometry/size.h"

namespace display {

// This class represents the basic information for a native mode. Platforms may
// extend this class to add platform specific information about the mode.
class DISPLAY_TYPES_EXPORT DisplayMode {
 public:
  DisplayMode(const gfx::Size& size, bool interlaced, float refresh_rate);

  DisplayMode(const DisplayMode&) = delete;
  DisplayMode& operator=(const DisplayMode&) = delete;

  ~DisplayMode();
  std::unique_ptr<DisplayMode> Clone() const;

  const gfx::Size& size() const { return size_; }
  bool is_interlaced() const { return is_interlaced_; }
  float refresh_rate() const { return refresh_rate_; }

  bool operator<(const DisplayMode& other) const;
  bool operator>(const DisplayMode& other) const;

  std::string ToString() const;

 private:
  const gfx::Size size_;
  const float refresh_rate_;
  const bool is_interlaced_;
};

// Used to by gtest to print readable errors.
DISPLAY_TYPES_EXPORT void PrintTo(const DisplayMode& mode, std::ostream* os);

}  // namespace display

#endif  // UI_DISPLAY_TYPES_DISPLAY_MODE_H_
