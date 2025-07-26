// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/quick_insert_insert_media_request.h"

#include <optional>
#include <utility>

#include "ash/quick_insert/quick_insert_insert_media.h"
#include "ash/quick_insert/quick_insert_rich_media.h"
#include "ash/quick_insert/quick_insert_web_paste_target.h"
#include "base/functional/callback.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"

namespace ash {

namespace {

QuickInsertInsertMediaRequest::Result ConvertInsertMediaResult(
    InsertMediaResult result) {
  switch (result) {
    case InsertMediaResult::kSuccess:
      return QuickInsertInsertMediaRequest::Result::kSuccess;
    case InsertMediaResult::kUnsupported:
      return QuickInsertInsertMediaRequest::Result::kUnsupported;
    case InsertMediaResult::kNotFound:
      return QuickInsertInsertMediaRequest::Result::kNotFound;
  }
}

}  // namespace

QuickInsertInsertMediaRequest::QuickInsertInsertMediaRequest(
    ui::InputMethod* input_method,
    const QuickInsertRichMedia& media_to_insert,
    const base::TimeDelta insert_timeout,
    base::OnceCallback<std::optional<QuickInsertWebPasteTarget>()>
        get_web_paste_target,
    OnCompleteCallback on_complete_callback)
    : media_to_insert_(media_to_insert),
      get_web_paste_target_(std::move(get_web_paste_target)),
      on_complete_callback_(on_complete_callback.is_null()
                                ? base::DoNothing()
                                : std::move(on_complete_callback)) {
  observation_.Observe(input_method);
  insert_timeout_timer_.Start(
      FROM_HERE, insert_timeout, this,
      &QuickInsertInsertMediaRequest::CancelPendingInsert);
}

QuickInsertInsertMediaRequest::~QuickInsertInsertMediaRequest() = default;

void QuickInsertInsertMediaRequest::OnTextInputStateChanged(
    const ui::TextInputClient* client) {
  // `OnTextInputStateChanged` can be called multiple times before the insert
  // timeout. For each `client` change, attempt to insert the media. Only notify
  // that the insert has failed when the insert has timed out.
  ui::TextInputClient* mutable_client =
      observation_.GetSource()->GetTextInputClient();

  DCHECK_EQ(mutable_client, client);
  if (mutable_client == nullptr ||
      mutable_client->GetTextInputType() ==
          ui::TextInputType::TEXT_INPUT_TYPE_NONE ||
      !media_to_insert_.has_value()) {
    return;
  }

  if (!InputFieldSupportsInsertingMedia(*media_to_insert_, *mutable_client)) {
    return;
  }

  insert_timeout_timer_.Reset();
  observation_.Reset();

  InsertMediaToInputField(*std::exchange(media_to_insert_, std::nullopt),
                          *mutable_client, std::move(get_web_paste_target_),
                          base::BindOnce(&ConvertInsertMediaResult)
                              .Then(std::move(on_complete_callback_)));
}

void QuickInsertInsertMediaRequest::OnInputMethodDestroyed(
    const ui::InputMethod* input_method) {
  if (observation_.GetSource() == input_method) {
    observation_.Reset();
  }
}

void QuickInsertInsertMediaRequest::CancelPendingInsert() {
  observation_.Reset();

  if (media_to_insert_.has_value()) {
    media_to_insert_ = std::nullopt;
    std::move(on_complete_callback_).Run(Result::kTimeout);
  }
}

}  // namespace ash
