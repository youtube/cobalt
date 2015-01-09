#ifndef UI_GFX_RECT_H_
#define UI_GFX_RECT_H_

#include "cobalt/math/rect.h"
#include "ui/gfx/rect_f.h"

namespace gfx {

// Introduce cobalt::math::Rect into namespace gfx as gfx::Rect is used by
// media.
using cobalt::math::Rect;

}  // namespace gfx

#endif  // UI_GFX_RECT_H_
