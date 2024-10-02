// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_UTIL_H_
#define UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_UTIL_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/platform_window/platform_window_init_properties.h"

class SkBitmap;
class SkPath;

namespace ui {
class WaylandShmBuffer;
class WaylandWindow;
}  // namespace ui

namespace gfx {
enum class BufferFormat : uint8_t;
class Size;
}  // namespace gfx

namespace wl {

using RequestSizeCallback = base::OnceCallback<void(const gfx::Size&)>;

using OnRequestBufferCallback =
    base::OnceCallback<void(wl::Object<struct wl_buffer>)>;

using BufferFormatsWithModifiersMap =
    base::flat_map<gfx::BufferFormat, std::vector<uint64_t>>;

// Constants used to determine how pointer/touch events are processed and
// dispatched.
enum class EventDispatchPolicy {
  kImmediate,
  kOnFrame,
};

// Identifies the direction of the "hittest" for Wayland. |connection|
// is used to identify whether values from shell v5 or v6 must be used.
uint32_t IdentifyDirection(int hittest);

// Draws |bitmap| into |out_buffer|. Returns if no errors occur, and false
// otherwise. It assumes the bitmap fits into the buffer and buffer is
// currently mmap'ed in memory address space.
bool DrawBitmap(const SkBitmap& bitmap, ui::WaylandShmBuffer* out_buffer);

// Helper function to read data from a file.
void ReadDataFromFD(base::ScopedFD fd, std::vector<uint8_t>* contents);

// Translates bounds relative to top level window to specified parent.
gfx::Rect TranslateBoundsToParentCoordinates(const gfx::Rect& child_bounds,
                                             const gfx::Rect& parent_bounds);
gfx::RectF TranslateBoundsToParentCoordinatesF(const gfx::RectF& child_bounds,
                                               const gfx::RectF& parent_bounds);
// Translates bounds relative to parent window to top level window.
gfx::Rect TranslateBoundsToTopLevelCoordinates(const gfx::Rect& child_bounds,
                                               const gfx::Rect& parent_bounds);

// Returns wl_output_transform corresponding |transform|. |transform| is an
// enumeration of a fixed selection of transformations.
wl_output_transform ToWaylandTransform(gfx::OverlayTransform transform);

// |bounds| contains |rect|. ApplyWaylandTransform() returns the resulted
// |rect| after transformation is applied to |bounds| containing |rect| as a
// whole.
gfx::Rect ApplyWaylandTransform(const gfx::Rect& rect,
                                const gfx::Size& bounds,
                                wl_output_transform transform);

// |bounds| contains |rect|. ApplyWaylandTransform() returns the resulted
// |rect| after transformation is applied to |bounds| containing |rect| as a
// whole.
gfx::RectF ApplyWaylandTransform(const gfx::RectF& rect,
                                 const gfx::SizeF& bounds,
                                 wl_output_transform transform);

// Applies transformation to |size|.
gfx::SizeF ApplyWaylandTransform(const gfx::SizeF& size,
                                 wl_output_transform transform);

// Returns the root WaylandWindow for the given wl_surface.
ui::WaylandWindow* RootWindowFromWlSurface(wl_surface* surface);

// Returns bounds of the given window, adjusted to its subsurface. We need to
// adjust bounds because WaylandWindow::GetBounds() returns absolute bounds in
// pixels, but wl_subsurface works with bounds relative to the parent surface
// and in DIP.
gfx::Rect TranslateWindowBoundsToParentDIP(ui::WaylandWindow* window,
                                           ui::WaylandWindow* parent_window);

// Returns rectangles dictated by SkPath.
std::vector<gfx::Rect> CreateRectsFromSkPath(const SkPath& path);

// Returns converted SkPath in DIPs from the one in pixels.
SkPath ConvertPathToDIP(const SkPath& path_in_pixels, float scale);

// Converts SkColor into wl_array.
void SkColorToWlArray(const SkColor& color, wl_array& array);

// Converts SkColor4f into wl_array.
void SkColorToWlArray(const SkColor4f& color, wl_array& array);

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_COMMON_WAYLAND_UTIL_H_
