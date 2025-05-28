// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_QUICK_INSERT_INSERT_MEDIA_H_
#define ASH_QUICK_INSERT_QUICK_INSERT_INSERT_MEDIA_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/quick_insert/quick_insert_rich_media.h"
#include "base/functional/callback_forward.h"

namespace ui {
class TextInputClient;
}  // namespace ui

namespace ash {

struct QuickInsertWebPasteTarget;

// Returns whether the `client` supports inserting `media`.
ASH_EXPORT bool InputFieldSupportsInsertingMedia(
    const QuickInsertRichMedia& media,
    ui::TextInputClient& client);

enum class ASH_EXPORT InsertMediaResult {
  kSuccess,
  kUnsupported,
  kNotFound,
};

using WebPasteTargetCallback =
    base::OnceCallback<std::optional<QuickInsertWebPasteTarget>()>;
using OnInsertMediaCompleteCallback =
    base::OnceCallback<void(InsertMediaResult)>;

// Inserts `media` into `client` asynchronously.
// `callback` is called with whether the insertion was successful.
ASH_EXPORT void InsertMediaToInputField(
    QuickInsertRichMedia media,
    ui::TextInputClient& client,
    WebPasteTargetCallback get_web_paste_target,
    OnInsertMediaCompleteCallback callback);

}  // namespace ash

#endif  // ASH_QUICK_INSERT_QUICK_INSERT_INSERT_MEDIA_H_
