// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/quick_answers/ui/rich_answers_translation_view.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/quick_answers/quick_answers_ui_controller.h"
#include "chrome/browser/ui/quick_answers/ui/quick_answers_util.h"
#include "chromeos/components/quick_answers/quick_answers_model.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"

namespace quick_answers {

// RichAnswersTranslationView
// -----------------------------------------------------------

RichAnswersTranslationView::RichAnswersTranslationView(
    const gfx::Rect& anchor_view_bounds,
    base::WeakPtr<QuickAnswersUiController> controller,
    const quick_answers::QuickAnswer& result)
    : RichAnswersView(anchor_view_bounds, controller, result) {
  InitLayout();

  // TODO (b/274184294): Add custom focus behavior according to
  // approved greenlines.
}

RichAnswersTranslationView::~RichAnswersTranslationView() = default;

void RichAnswersTranslationView::InitLayout() {
  // TODO (b/265258270): Populate translation view contents.
}

BEGIN_METADATA(RichAnswersTranslationView, RichAnswersView)
END_METADATA

}  // namespace quick_answers
