// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_VECTOR_ICON_UTILS_H_
#define UI_GFX_VECTOR_ICON_UTILS_H_

#include "ui/gfx/gfx_export.h"

namespace gfx {

struct VectorIcon;

// Calculates the size that will be default for |icon|, in dip. This will be the
// smallest icon size |icon| contains.
GFX_EXPORT int GetDefaultSizeOfVectorIcon(const gfx::VectorIcon& icon);

}  // namespace gfx

#endif  // UI_GFX_VECTOR_ICON_UTILS_H_
