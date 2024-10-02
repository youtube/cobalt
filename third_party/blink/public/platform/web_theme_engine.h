/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_THEME_ENGINE_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_THEME_ENGINE_H_

#include <map>
#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/css/forced_colors.h"
#include "third_party/blink/public/mojom/frame/color_scheme.mojom-shared.h"
#include "third_party/blink/public/platform/web_scrollbar_overlay_color_theme.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/color/color_provider_utils.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
class PaintCanvas;
}

namespace blink {

class WebThemeEngine {
 public:
  // The current state of the associated Part.
  enum State {
    kStateDisabled,
    kStateHover,
    kStateNormal,
    kStatePressed,
    kStateFocused,
    kStateReadonly,
  };

  // The UI part which is being accessed.
  enum Part {
    // ScrollbarTheme parts
    kPartScrollbarDownArrow,
    kPartScrollbarLeftArrow,
    kPartScrollbarRightArrow,
    kPartScrollbarUpArrow,
    kPartScrollbarHorizontalThumb,
    kPartScrollbarVerticalThumb,
    kPartScrollbarHorizontalTrack,
    kPartScrollbarVerticalTrack,
    kPartScrollbarCorner,

    // LayoutTheme parts
    kPartCheckbox,
    kPartRadio,
    kPartButton,
    kPartTextField,
    kPartMenuList,
    kPartSliderTrack,
    kPartSliderThumb,
    kPartInnerSpinButton,
    kPartProgressBar
  };

  enum class SystemThemeColor {
    kNotSupported,
    kButtonFace,
    kButtonText,
    kGrayText,
    kHighlight,
    kHighlightText,
    kHotlight,
    kMenuHighlight,
    kScrollbar,
    kWindow,
    kWindowText,
    kMaxValue = kWindowText,
  };

  // Extra parameters for drawing the PartScrollbarHorizontalTrack and
  // PartScrollbarVerticalTrack.
  struct ScrollbarTrackExtraParams {
    bool is_back;  // Whether this is the 'back' part or the 'forward' part.

    // The bounds of the entire track, as opposed to the part being painted.
    int track_x;
    int track_y;
    int track_width;
    int track_height;
  };

  // Extra parameters for PartCheckbox, PartPushButton and PartRadio.
  struct ButtonExtraParams {
    bool checked;
    bool indeterminate;  // Whether the button state is indeterminate.
    bool has_border;
    SkColor background_color;
    float zoom;
  };

  // Extra parameters for PartTextField
  struct TextFieldExtraParams {
    bool is_text_area;
    bool is_listbox;
    SkColor background_color;
    bool has_border;
    bool auto_complete_active;
    float zoom;
  };

  // Extra parameters for PartMenuList
  struct MenuListExtraParams {
    bool has_border;
    bool has_border_radius;
    int arrow_x;
    int arrow_y;
    int arrow_size;
    SkColor arrow_color;
    SkColor background_color;
    bool fill_content_area;
    float zoom;
  };

  // Extra parameters for PartSliderTrack and PartSliderThumb
  struct SliderExtraParams {
    bool vertical;
    bool in_drag;
    int thumb_x;
    int thumb_y;
    float zoom;
    bool right_to_left;
  };

  // Extra parameters for PartInnerSpinButton
  struct InnerSpinButtonExtraParams {
    bool spin_up;
    bool read_only;
  };

  // Extra parameters for PartProgressBar
  struct ProgressBarExtraParams {
    bool determinate;
    int value_rect_x;
    int value_rect_y;
    int value_rect_width;
    int value_rect_height;
    float zoom;
    bool is_horizontal;
  };

  // Extra parameters for scrollbar thumb. Used only for overlay scrollbars.
  struct ScrollbarThumbExtraParams {
    WebScrollbarOverlayColorTheme scrollbar_theme;
  };

  struct ScrollbarButtonExtraParams {
    float zoom;
    bool right_to_left;
  };

  // Represents ui::NativeTheme System Info
  struct SystemColorInfoState {
    bool is_dark_mode;
    bool forced_colors;
    std::map<SystemThemeColor, uint32_t> colors;
  };

#if BUILDFLAG(IS_MAC)
  enum ScrollbarOrientation {
    // Vertical scrollbar on the right side of content.
    kVerticalOnRight,
    // Vertical scrollbar on the left side of content.
    kVerticalOnLeft,
    // Horizontal scrollbar (on the bottom of content).
    kHorizontal,
  };

  struct ScrollbarExtraParams {
    bool is_hovering;
    bool is_overlay;
    mojom::ColorScheme scrollbar_theme;
    ScrollbarOrientation orientation;
    float scale_from_dip;
  };
#endif

  union ExtraParams {
    ScrollbarTrackExtraParams scrollbar_track;
    ButtonExtraParams button;
    TextFieldExtraParams text_field;
    MenuListExtraParams menu_list;
    SliderExtraParams slider;
    InnerSpinButtonExtraParams inner_spin;
    ProgressBarExtraParams progress_bar;
    ScrollbarThumbExtraParams scrollbar_thumb;
    ScrollbarButtonExtraParams scrollbar_button;
#if BUILDFLAG(IS_MAC)
    ScrollbarExtraParams scrollbar_extra;
#endif
  };

  virtual ~WebThemeEngine() {}

  // Gets the size of the given theme part. For variable sized items
  // like vertical scrollbar thumbs, the width will be the required width of
  // the track while the height will be the minimum height.
  virtual gfx::Size GetSize(Part) { return gfx::Size(); }

  virtual bool SupportsNinePatch(Part) const { return false; }
  virtual gfx::Size NinePatchCanvasSize(Part) const { return gfx::Size(); }
  virtual gfx::Rect NinePatchAperture(Part) const { return gfx::Rect(); }

  struct ScrollbarStyle {
    int thumb_thickness;
    int scrollbar_margin;
    int thumb_thickness_thin;
    int scrollbar_margin_thin;
    SkColor color;
    base::TimeDelta fade_out_delay;
    base::TimeDelta fade_out_duration;
  };

  // Gets the overlay scrollbar style. Not used on Mac.
  virtual void GetOverlayScrollbarStyle(ScrollbarStyle* style) {
    // Disable overlay scrollbar fade out (for non-composited scrollers) unless
    // explicitly enabled by the implementing child class. NOTE: these values
    // aren't used to control Mac fade out - that happens in ScrollAnimatorMac.
    style->fade_out_delay = base::TimeDelta();
    style->fade_out_duration = base::TimeDelta();
    // The other fields in this struct are used only on Android to draw solid
    // color scrollbars. On other platforms the scrollbars are painted in
    // NativeTheme so these fields are unused in non-Android WebThemeEngines.
  }

  // Paint the given the given theme part.
  virtual void Paint(
      cc::PaintCanvas*,
      Part,
      State,
      const gfx::Rect&,
      const ExtraParams*,
      blink::mojom::ColorScheme,
      const absl::optional<SkColor>& accent_color = absl::nullopt) {}

  virtual absl::optional<SkColor> GetSystemColor(
      SystemThemeColor system_theme) const {
    return absl::nullopt;
  }

  virtual ForcedColors GetForcedColors() const { return ForcedColors::kNone; }
  virtual void OverrideForcedColorsTheme(bool is_dark_theme) {}
  virtual void SetForcedColors(const blink::ForcedColors forced_colors) {}
  virtual void ResetToSystemColors(
      SystemColorInfoState system_color_info_state) {}
  virtual SystemColorInfoState GetSystemColorInfo() {
    SystemColorInfoState state;
    return state;
  }

  // Updates the WebThemeEngine's global light and dark ColorProvider instances
  // using the RendererColorMaps provided. Returns true if new ColorProviders
  // were created, returns false otherwise.
  virtual bool UpdateColorProviders(const ui::RendererColorMap& light_colors,
                                    const ui::RendererColorMap& dark_colors) {
    return false;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_THEME_ENGINE_H_
