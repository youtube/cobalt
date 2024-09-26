// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_NATIVE_THEME_NATIVE_THEME_FLUENT_H_
#define UI_NATIVE_THEME_NATIVE_THEME_FLUENT_H_

#include "ui/native_theme/native_theme_base.h"

namespace gfx {
class Rect;
class RectF;
}  // namespace gfx

template <typename T>
class sk_sp;
class SkTypeface;

namespace ui {

class NATIVE_THEME_EXPORT NativeThemeFluent : public NativeThemeBase {
 public:
  explicit NativeThemeFluent(bool should_only_use_dark_colors);

  NativeThemeFluent(const NativeThemeFluent&) = delete;
  NativeThemeFluent& operator=(const NativeThemeFluent&) = delete;

  ~NativeThemeFluent() override;

  static NativeThemeFluent* web_instance();

  void PaintArrowButton(cc::PaintCanvas* canvas,
                        const ColorProvider* color_provider,
                        const gfx::Rect& rect,
                        Part direction,
                        State state,
                        ColorScheme color_scheme,
                        const ScrollbarArrowExtraParams& arrow) const override;
  void PaintScrollbarTrack(cc::PaintCanvas* canvas,
                           const ColorProvider* color_provider,
                           Part part,
                           State state,
                           const ScrollbarTrackExtraParams& extra_params,
                           const gfx::Rect& rect,
                           ColorScheme color_scheme) const override;
  void PaintScrollbarThumb(cc::PaintCanvas* canvas,
                           const ColorProvider* color_provider,
                           Part part,
                           State state,
                           const gfx::Rect& rect,
                           ScrollbarOverlayColorTheme theme,
                           ColorScheme color_scheme) const override;
  void PaintScrollbarCorner(cc::PaintCanvas* canvas,
                            const ColorProvider* color_provider,
                            State state,
                            const gfx::Rect& rect,
                            ColorScheme color_scheme) const override;
  gfx::Size GetPartSize(Part part,
                        State state,
                        const ExtraParams& extra) const override;

 private:
  friend class NativeThemeFluentTest;

  void PaintButton(cc::PaintCanvas* canvas,
                   const ColorProvider* color_provider,
                   const gfx::Rect& rect,
                   ColorScheme color_scheme) const;
  void PaintArrow(cc::PaintCanvas* canvas,
                  const ColorProvider* color_provider,
                  const gfx::Rect& rect,
                  Part part,
                  State state,
                  ColorScheme color_scheme) const;

  // Calculates and returns the position and dimensions of the scaled arrow rect
  // within the scrollbar button rect. The goal is to keep the arrow in the
  // center of the button with the applied kFluentScrollbarArrowOffset. See
  // OffsetArrowRect method for more details.
  gfx::RectF GetArrowRect(const gfx::Rect& rect, Part part, State state) const;

  // An arrow rect is a square. Returns the side length based on the state and
  // the font availability.
  int GetArrowSideLength(State state) const;

  // By Fluent design, arrow rect is offset from the center to the side opposite
  // from the track rect border by kFluentScrollbarArrowOffset px.
  void OffsetArrowRect(gfx::RectF& arrow_rect,
                       Part part,
                       int max_arrow_rect_side) const;

  // Returns true if the font with arrow icons is present on the device.
  bool ArrowIconsAvailable() const { return typeface_.get(); }

  const char* GetArrowCodePointForScrollbarPart(Part part) const;

  // The value stores a shared pointer to SkTypeface with the font family, which
  // contains arrow icons.
  sk_sp<SkTypeface> typeface_;
};

}  // namespace ui

#endif  // UI_NATIVE_THEME_NATIVE_THEME_FLUENT_H_
