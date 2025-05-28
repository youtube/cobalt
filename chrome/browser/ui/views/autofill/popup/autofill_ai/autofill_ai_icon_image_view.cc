// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/autofill_ai/autofill_ai_icon_image_view.h"

#include "build/branding_buildflags.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/views/autofill/popup/autofill_ai/autofill_ai_icon_background.h"
#include "ui/gfx/geometry/size.h"

namespace autofill_ai {
namespace {
std::unique_ptr<views::ImageView> CreateAutofillAiIconImageView(
    gfx::Size preferred_size,
    gfx::Size icon_size,
    views::Emphasis corner_radius) {
  return views::Builder<views::ImageView>()
      .SetImage(ui::ImageModel::FromVectorIcon(kTextAnalysisIcon))
      .SetPreferredSize(preferred_size)
      .SetImageSize(icon_size)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      .SetBackground(std::make_unique<AutofillAiIconBackground>(corner_radius))
#endif
      .Build();
}

}  // namespace

std::unique_ptr<views::ImageView> CreateSmallAutofillAiIconImageView() {
  constexpr int kSmallViewSide = 20;
  constexpr int kSmallIconSide = 16;
  return CreateAutofillAiIconImageView(
      gfx::Size(kSmallViewSide, kSmallViewSide),
      gfx::Size(kSmallIconSide, kSmallIconSide), views::Emphasis::kLow);
}

std::unique_ptr<views::ImageView> CreateLargeAutofillAiIconImageView() {
  constexpr int kLargeViewSide = 36;
  constexpr int kLargeIconSide = 20;
  return CreateAutofillAiIconImageView(
      gfx::Size(kLargeViewSide, kLargeViewSide),
      gfx::Size(kLargeIconSide, kLargeIconSide), views::Emphasis::kHigh);
}

}  // namespace autofill_ai
