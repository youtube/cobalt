// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/popup_personal_context_notice_view.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_content_view.h"
#include "chrome/browser/ui/views/autofill/popup/popup_row_view.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view.h"

namespace autofill {

PopupPersonalContextNoticeView::PopupPersonalContextNoticeView(
    PopupRowView::AccessibilitySelectionDelegate& a11y_selection_delegate,
    PopupRowView::SelectionDelegate& selection_delegate,
    base::WeakPtr<AutofillPopupController> controller,
    int line_number,
    std::unique_ptr<PopupRowContentView> content_view)
    : PopupRowView(a11y_selection_delegate,
                   selection_delegate,
                   controller,
                   line_number,
                   std::move(content_view)),
      controller_(std::move(controller)),
      line_number_(line_number) {
  // Text container (Title + Description)
  // TODO(crbug.com/517520354): Add styling and the title.
  views::View& text_container = GetContentView();
  auto* layout =
      static_cast<views::BoxLayout*>(text_container.GetLayoutManager());
  layout->SetOrientation(views::BoxLayout::Orientation::kVertical);

  // Description (with link)
  // TODO(crbug.com/517520354): Add styling and strings.
  size_t link_offset;
  std::u16string link_text = u"settings";
  std::u16string description_text = u"Manage in settings";
  link_offset = description_text.find(link_text);

  description_ =
      text_container.AddChildView(std::make_unique<views::StyledLabel>());
  description_->SetText(description_text);
  // TODO(crbug.com/515651588): Update accessibility names.
  text_container.GetViewAccessibility().SetName(description_text);
  GetViewAccessibility().SetName(description_text);

  // Make the link substring in the description clickable.
  description_->AddStyleRange(
      gfx::Range(link_offset, link_offset + link_text.length()),
      views::StyledLabel::RangeStyleInfo::CreateForLink(base::BindRepeating(
          &PopupPersonalContextNoticeView::OnSettingsButtonClicked,
          base::Unretained(this))));

  // "Got it" button
  // TODO(crbug.com/517520354): Add styling and strings.
  got_it_button_ = AddChildView(std::make_unique<views::MdTextButton>(
      base::BindRepeating(&PopupPersonalContextNoticeView::OnGotItButtonClicked,
                          base::Unretained(this)),
      u"OK"));
}

void PopupPersonalContextNoticeView::OnGotItButtonClicked() {
  if (controller_) {
    // TODO(crbug.com/520201413): Add metrics to track the cases when
    // `RemoveSuggestion` returns false.
    controller_->RemoveSuggestion(
        line_number_,
        AutofillMetrics::SingleEntryRemovalMethod::kDeleteButtonClicked);
  }
}

void PopupPersonalContextNoticeView::OnSettingsButtonClicked() {
  // TODO(crbug.com/520188717): Route to new Chrome settings once available.
}

PopupPersonalContextNoticeView::~PopupPersonalContextNoticeView() = default;

BEGIN_METADATA(PopupPersonalContextNoticeView)
END_METADATA

}  // namespace autofill
