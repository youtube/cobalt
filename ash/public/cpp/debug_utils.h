// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_DEBUG_UTILS_H_
#define ASH_PUBLIC_CPP_DEBUG_UTILS_H_

#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "ash/ash_export.h"

namespace ash {
namespace debug {

// Prints all windows layer hierarchy to |out|. If mouse is down, only prints
// the layer hierarchy of the window under the mouse. The window under the mouse
// is considered iff the component is HTCLIENT. If no window is under the mouse,
// falls back to default behavior with a warning.
ASH_EXPORT void PrintLayerHierarchy(std::ostringstream* out);

// Prints view hierarchy for the window under the mouse if mouse is down,
// otherwise prints view hierarchy for the active window. The window under the
// mouse is considered iff the component is HTCLIENT. If no window is under the
// mouse, falls back to default behavior with a warning.
ASH_EXPORT void PrintViewHierarchy(std::ostringstream* out);

// Prints all windows hierarchy to |out|. If mouse is down, only prints
// the hierarchy of the window under the mouse. The window under the mouse is
// is considered iff the component is HTCLIENT. If no window is under the mouse,
// falls back to default behavior with a warning. If |scrub_data| is true, we
// may skip some data fields that are not very important for debugging.
// Returns a list of window titles. Window titles will be removed from |out|
// if |scrub_data| is true.
ASH_EXPORT std::vector<std::string> PrintWindowHierarchy(
    std::ostringstream* out,
    bool scrub_data);

}  // namespace debug
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_DEBUG_UTILS_H_
