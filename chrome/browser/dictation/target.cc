// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/target.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "ui/base/ime/ime_text_span.h"

namespace dictation {

Target::Target() = default;

Target::Target(content::RenderFrameHost* rfh, const std::string& selected_text)
    : selected_text_(selected_text) {
  // May be null in tests.
  if (rfh) {
    rfh_id_ = rfh->GetGlobalId();
  }
}

Target::~Target() = default;

const std::string& Target::GetSelectedText() const {
  return selected_text_;
}

content::RenderFrameHost* Target::GetRenderFrameHost() const {
  return content::RenderFrameHost::FromID(rfh_id_);
}

content::RenderWidgetHost* Target::GetRenderWidgetHost() const {
  content::RenderFrameHost* rfh = GetRenderFrameHost();
  return rfh ? rfh->GetRenderWidgetHost() : nullptr;
}

void Target::SetComposition(const std::u16string& text, bool is_final) {
  if (!is_final) {
    return;
  }

  content::RenderWidgetHost* rwh = GetRenderWidgetHost();
  if (!rwh) {
    return;
  }

  // Specify an ImeTextSpan for the entire text to make it look like a user
  // typing without a visual difference for the composition.
  ui::ImeTextSpan text_span;
  text_span.end_offset = text.length();
  text_span.underline_style = ui::ImeTextSpan::UnderlineStyle::kNone;

  rwh->SetExternallySourcedComposition(text, {text_span});
}

void Target::CommitComposition(const std::u16string& text) {
  content::RenderWidgetHost* rwh = GetRenderWidgetHost();
  if (!rwh) {
    return;
  }

  // TODO(crbug.com/525856380): Handle changes to the focused element.
  rwh->CommitExternallySourcedComposition(text);
}

}  // namespace dictation
