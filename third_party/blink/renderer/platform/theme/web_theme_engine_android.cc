// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/theme/web_theme_engine_android.h"

#include "base/notreached.h"
#include "base/system/sys_info.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/renderer/platform/theme/web_theme_engine_conversions.h"
#include "ui/native_theme/native_theme.h"

namespace blink {

static void GetNativeThemeExtraParams(
    WebThemeEngine::Part part,
    WebThemeEngine::State state,
    const WebThemeEngine::ExtraParams* extra_params,
    ui::NativeTheme::ExtraParams* native_theme_extra_params) {
  switch (part) {
    case WebThemeEngine::kPartScrollbarHorizontalTrack:
    case WebThemeEngine::kPartScrollbarVerticalTrack:
      // Android doesn't draw scrollbars.
      NOTREACHED();
      break;
    case WebThemeEngine::kPartCheckbox:
      native_theme_extra_params->button.checked = extra_params->button.checked;
      native_theme_extra_params->button.indeterminate =
          extra_params->button.indeterminate;
      native_theme_extra_params->button.zoom = extra_params->button.zoom;
      break;
    case WebThemeEngine::kPartRadio:
      native_theme_extra_params->button.checked = extra_params->button.checked;
      break;
    case WebThemeEngine::kPartButton:
      native_theme_extra_params->button.has_border =
          extra_params->button.has_border;
      // Native buttons have a different focus style.
      native_theme_extra_params->button.is_focused = false;
      native_theme_extra_params->button.background_color =
          extra_params->button.background_color;
      native_theme_extra_params->button.zoom = extra_params->button.zoom;
      break;
    case WebThemeEngine::kPartTextField:
      native_theme_extra_params->text_field.is_text_area =
          extra_params->text_field.is_text_area;
      native_theme_extra_params->text_field.is_listbox =
          extra_params->text_field.is_listbox;
      native_theme_extra_params->text_field.background_color =
          extra_params->text_field.background_color;
      native_theme_extra_params->text_field.has_border =
          extra_params->text_field.has_border;
      native_theme_extra_params->text_field.auto_complete_active =
          extra_params->text_field.auto_complete_active;
      native_theme_extra_params->text_field.zoom =
          extra_params->text_field.zoom;
      break;
    case WebThemeEngine::kPartMenuList:
      native_theme_extra_params->menu_list.has_border =
          extra_params->menu_list.has_border;
      native_theme_extra_params->menu_list.has_border_radius =
          extra_params->menu_list.has_border_radius;
      native_theme_extra_params->menu_list.arrow_x =
          extra_params->menu_list.arrow_x;
      native_theme_extra_params->menu_list.arrow_y =
          extra_params->menu_list.arrow_y;
      native_theme_extra_params->menu_list.arrow_size =
          extra_params->menu_list.arrow_size;
      native_theme_extra_params->menu_list.arrow_color =
          extra_params->menu_list.arrow_color;
      native_theme_extra_params->menu_list.background_color =
          extra_params->menu_list.background_color;
      native_theme_extra_params->menu_list.zoom = extra_params->menu_list.zoom;
      break;
    case WebThemeEngine::kPartSliderTrack:
      native_theme_extra_params->slider.thumb_x = extra_params->slider.thumb_x;
      native_theme_extra_params->slider.thumb_y = extra_params->slider.thumb_y;
      native_theme_extra_params->slider.zoom = extra_params->slider.zoom;
      native_theme_extra_params->slider.right_to_left =
          extra_params->slider.right_to_left;
      [[fallthrough]];
    case WebThemeEngine::kPartSliderThumb:
      native_theme_extra_params->slider.vertical =
          extra_params->slider.vertical;
      native_theme_extra_params->slider.in_drag = extra_params->slider.in_drag;
      break;
    case WebThemeEngine::kPartInnerSpinButton:
      native_theme_extra_params->inner_spin.spin_up =
          extra_params->inner_spin.spin_up;
      native_theme_extra_params->inner_spin.read_only =
          extra_params->inner_spin.read_only;
      break;
    case WebThemeEngine::kPartProgressBar:
      native_theme_extra_params->progress_bar.determinate =
          extra_params->progress_bar.determinate;
      native_theme_extra_params->progress_bar.value_rect_x =
          extra_params->progress_bar.value_rect_x;
      native_theme_extra_params->progress_bar.value_rect_y =
          extra_params->progress_bar.value_rect_y;
      native_theme_extra_params->progress_bar.value_rect_width =
          extra_params->progress_bar.value_rect_width;
      native_theme_extra_params->progress_bar.value_rect_height =
          extra_params->progress_bar.value_rect_height;
      native_theme_extra_params->progress_bar.zoom =
          extra_params->progress_bar.zoom;
      native_theme_extra_params->progress_bar.is_horizontal =
          extra_params->progress_bar.is_horizontal;
      break;
    default:
      break;  // Parts that have no extra params get here.
  }
}

WebThemeEngineAndroid::~WebThemeEngineAndroid() = default;

gfx::Size WebThemeEngineAndroid::GetSize(WebThemeEngine::Part part) {
  switch (part) {
    case WebThemeEngine::kPartScrollbarHorizontalThumb:
    case WebThemeEngine::kPartScrollbarVerticalThumb: {
      // Minimum length for scrollbar thumb is the scrollbar thickness.
      ScrollbarStyle style;
      GetOverlayScrollbarStyle(&style);
      int scrollbarThickness = style.thumb_thickness + style.scrollbar_margin;
      return gfx::Size(scrollbarThickness, scrollbarThickness);
    }
    default: {
      ui::NativeTheme::ExtraParams extra;
      return ui::NativeTheme::GetInstanceForWeb()->GetPartSize(
          NativeThemePart(part), ui::NativeTheme::kNormal, extra);
    }
  }
}

void WebThemeEngineAndroid::GetOverlayScrollbarStyle(ScrollbarStyle* style) {
  // TODO(bokan): Android scrollbars on non-composited scrollers don't
  // currently fade out so the fadeOutDuration and Delay  Now that this has
  // been added into Blink for other platforms we should plumb that through for
  // Android as well.
  style->fade_out_delay = base::TimeDelta();
  style->fade_out_duration = base::TimeDelta();
  style->thumb_thickness = 4;
  style->scrollbar_margin = 0;
  style->color = SkColorSetARGB(128, 64, 64, 64);
}

void WebThemeEngineAndroid::Paint(
    cc::PaintCanvas* canvas,
    WebThemeEngine::Part part,
    WebThemeEngine::State state,
    const gfx::Rect& rect,
    const WebThemeEngine::ExtraParams* extra_params,
    blink::mojom::ColorScheme color_scheme,
    const absl::optional<SkColor>& accent_color) {
  ui::NativeTheme::ExtraParams native_theme_extra_params;
  GetNativeThemeExtraParams(part, state, extra_params,
                            &native_theme_extra_params);
  // ColorProviders are not supported on android and there are no controls that
  // require ColorProvider colors on the platform.
  const ui::ColorProvider* color_provider = nullptr;
  ui::NativeTheme::GetInstanceForWeb()->Paint(
      canvas, color_provider, NativeThemePart(part), NativeThemeState(state),
      rect, native_theme_extra_params, NativeColorScheme(color_scheme),
      accent_color);
}

ForcedColors WebThemeEngineAndroid::GetForcedColors() const {
  return ui::NativeTheme::GetInstanceForWeb()->InForcedColorsMode()
             ? ForcedColors::kActive
             : ForcedColors::kNone;
}

void WebThemeEngineAndroid::SetForcedColors(const ForcedColors forced_colors) {
  ui::NativeTheme::GetInstanceForWeb()->set_forced_colors(
      forced_colors == ForcedColors::kActive);
}
}  // namespace blink
