// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_SYSTEM_SHADOW_H_
#define ASH_STYLE_SYSTEM_SHADOW_H_

#include "ash/ash_export.h"
#include "ui/gfx/geometry/rect.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class Layer;
}  // namespace ui

namespace views {
class View;
}  // namespace views

namespace ash {

// SystemShadow is an interface to generate shadow with system shadow style for
// different types of UI surfaces.
class ASH_EXPORT SystemShadow {
 public:
  // Shadow types of system UI components. The shadows with different elevations
  // have different appearance.
  enum class Type {
    kElevation4,
    kElevation8,
    kElevation12,
    kElevation16,
    kElevation24,
  };

  virtual ~SystemShadow();

  // Create a system shadow based on `ui::Shadow` which paints shadow on a nine
  // patch layer. This shadow can be used for any UI surfaces. Usually, when
  // creating the shadow for a window, attach the shadow's layer at the bottom
  // of the window's layer; when creating the shadow for a view, attach the
  // shadow's layer at the bottom of the view's parent layer. The layer's
  // content bounds should be manually updated.
  static std::unique_ptr<SystemShadow> CreateShadowOnNinePatchLayer(
      Type shadow_type);

  // Create a system shadow based on `ash::ViewShadow`. This shadow is used for
  // views. The shadow's layer is added to the `layers_beneath_` of the view and
  // its content bounds are adjusted with the bounds of view's layer. The shadow
  // does not need to manually update the content bounds but cannot be used when
  // the shadow's content bounds do not equal to the view bounds. For example,
  // `AppListFolderView` has a clip rect whose bounds should be the content
  // bounds of the shadow. In this case, please use
  // `CreateShadowOnNinePatchLayer` instead.
  static std::unique_ptr<SystemShadow> CreateShadowOnNinePatchLayerForView(
      views::View* view,
      Type shadow_type);

  // Create a system shadow based on `ui::Shadow`. The shadow's layer is added
  // to the bottom of the window's layer and its contents bounds are adjusted
  // with the window bounds. The shadow does not need to manually update the
  // content bounds but cannot be used when the shadow's contents bounds do not
  // equal to the window bounds. For example, the content bounds of
  // `OverviewItem` for wide and tall windows do not equal to the item bounds.
  // In this case, please use `CreateShadowOnNinePatchLayer` instead.
  static std::unique_ptr<SystemShadow> CreateShadowOnNinePatchLayerForWindow(
      aura::Window* window,
      Type shadow_type);

  // Create a system shadow painted on a texture layer. Painting shadow on a
  // texture layer is expensive so only use it when necessary. See
  // `SystemShadowOnTextureLayer` for more details.
  static std::unique_ptr<SystemShadow> CreateShadowOnTextureLayer(
      Type shadow_type);

  // Get shadow elevation according to the given type.
  static int GetElevationFromType(Type type);

  // Change shadow type and update shadow elevation and appearance. Note that to
  // avoid inconsistency of shadow type and elevation. Always change system
  // shadow elevation with `SetType`.
  virtual void SetType(Type type) = 0;

  virtual void SetContentBounds(const gfx::Rect& bounds) = 0;

  virtual void SetRoundedCornerRadius(int corner_radius) = 0;

  virtual const gfx::Rect& GetContentBounds() = 0;

  // Return the layer of the shadow. This function can be used by any types of
  // shadow. The layer is commonly used for setting layer hierarchy, visibility,
  // and transformation.
  virtual ui::Layer* GetLayer() = 0;

  // Return the nine patch layer of the shadow. This function is only used by
  // ui::Shadow based implementations. The nine patch layer is a child layer of
  // the shadow's layer painted with the shadow image. Normally, set the
  // hierarchy, visibility and transformation on the shadow's layer instead of
  // the nine patch layer.
  virtual ui::Layer* GetNinePatchLayer() = 0;
};

}  // namespace ash

#endif  // ASH_STYLE_SYSTEM_SHADOW_H_
