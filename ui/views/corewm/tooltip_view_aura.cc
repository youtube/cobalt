// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/tooltip_view_aura.h"

#include "base/strings/string_util.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/painter.h"

namespace views::corewm {

namespace {

constexpr int kTooltipBorderThickness = 1;
constexpr gfx::Insets kBorderInset = gfx::Insets::TLBR(4, 8, 5, 8);

}  // namespace

TooltipViewAura::TooltipViewAura()
    : render_text_(gfx::RenderText::CreateRenderText()) {
  render_text_->SetWordWrapBehavior(gfx::WRAP_LONG_WORDS);
  render_text_->SetMultiline(true);

  SetBackground(
      views::CreateThemedSolidBackground(ui::kColorTooltipBackground));
  SetBorder(views::CreatePaddedBorder(
      views::CreateThemedSolidBorder(kTooltipBorderThickness,
                                     ui::kColorTooltipForeground),
      kBorderInset - gfx::Insets(kTooltipBorderThickness)));

  ResetDisplayRect();
}

TooltipViewAura::~TooltipViewAura() = default;

void TooltipViewAura::SetText(const std::u16string& text) {
  render_text_->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);

  // Replace tabs with whitespace to avoid placeholder character rendering
  // where previously it did not. crbug.com/993100
  std::u16string newText(text);
  base::ReplaceChars(newText, u"\t", u"        ", &newText);
  render_text_->SetText(newText);
  SchedulePaint();
}

void TooltipViewAura::SetFontList(const gfx::FontList& font_list) {
  render_text_->SetFontList(font_list);
}

void TooltipViewAura::SetMinLineHeight(int line_height) {
  render_text_->SetMinLineHeight(line_height);
}

void TooltipViewAura::SetMaxWidth(int width) {
  max_width_ = width;
  ResetDisplayRect();
}

void TooltipViewAura::OnPaint(gfx::Canvas* canvas) {
  OnPaintBackground(canvas);
  gfx::Size text_size = size();
  gfx::Insets insets = GetInsets();
  text_size.Enlarge(-insets.width(), -insets.height());
  render_text_->SetDisplayRect(gfx::Rect(text_size));
  canvas->Save();
  canvas->Translate(gfx::Vector2d(insets.left(), insets.top()));
  render_text_->Draw(canvas);
  canvas->Restore();
  OnPaintBorder(canvas);
}

gfx::Size TooltipViewAura::CalculatePreferredSize() const {
  gfx::Size view_size = render_text_->GetStringSize();
  gfx::Insets insets = GetInsets();
  view_size.Enlarge(insets.width(), insets.height());
  return view_size;
}

void TooltipViewAura::OnThemeChanged() {
  views::View::OnThemeChanged();
  // Force the text color to be readable when |background_color| is not
  // opaque.
  render_text_->set_subpixel_rendering_suppressed(
      SkColorGetA(background()->get_color()) != SK_AlphaOPAQUE);
  render_text_->SetColor(
      GetColorProvider()->GetColor(ui::kColorTooltipForeground));
}

void TooltipViewAura::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kTooltip;
  node_data->SetNameChecked(render_text_->GetDisplayText());
}

void TooltipViewAura::ResetDisplayRect() {
  render_text_->SetDisplayRect(gfx::Rect(0, 0, max_width_, 100000));
}

BEGIN_METADATA(TooltipViewAura, views::View)
END_METADATA

}  // namespace views::corewm
