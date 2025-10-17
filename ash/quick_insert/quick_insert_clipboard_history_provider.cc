// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/quick_insert_clipboard_history_provider.h"

#include <string>
#include <string_view>

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "base/i18n/case_conversion.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"

namespace ash {
namespace {

constexpr base::TimeDelta kRecencyThreshold = base::Seconds(60);
constexpr int kMaxTextLength = 10000;

std::optional<QuickInsertClipboardResult::DisplayFormat> GetDisplayFormat(
    crosapi::mojom::ClipboardHistoryDisplayFormat format) {
  switch (format) {
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kFile:
      return QuickInsertClipboardResult::DisplayFormat::kFile;
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kText:
      return QuickInsertClipboardResult::DisplayFormat::kText;
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kPng:
      return QuickInsertClipboardResult::DisplayFormat::kImage;
    case crosapi::mojom::ClipboardHistoryDisplayFormat::kHtml:
      // TODO: b/348102522 - Show HTML content once it's possible to render them
      // inside Quick Insert.
    default:
      return std::nullopt;
  }
}

bool MatchQuery(const ClipboardHistoryItem& item, std::u16string_view query) {
  if (query.empty()) {
    return true;
  }
  if (item.display_text().length() > kMaxTextLength) {
    return false;
  }
  if (item.display_format() !=
          crosapi::mojom::ClipboardHistoryDisplayFormat::kText &&
      item.display_format() !=
          crosapi::mojom::ClipboardHistoryDisplayFormat::kFile) {
    return false;
  }
  return base::i18n::ToLower(item.display_text())
             .find(base::i18n::ToLower(query)) != std::u16string::npos;
}
}  // namespace

QuickInsertClipboardHistoryProvider::QuickInsertClipboardHistoryProvider(
    base::Clock* clock)
    : clock_(clock) {}

QuickInsertClipboardHistoryProvider::~QuickInsertClipboardHistoryProvider() =
    default;

void QuickInsertClipboardHistoryProvider::FetchResults(
    OnFetchResultsCallback callback,
    std::u16string_view query) {
  ClipboardHistoryController* clipboard_history_controller =
      ClipboardHistoryController::Get();
  if (clipboard_history_controller) {
    clipboard_history_controller->GetHistoryValues(
        base::BindOnce(&QuickInsertClipboardHistoryProvider::OnFetchHistory,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::u16string(query)));
  }
}

void QuickInsertClipboardHistoryProvider::OnFetchHistory(
    OnFetchResultsCallback callback,
    std::u16string query,
    std::vector<ClipboardHistoryItem> items) {
  std::vector<QuickInsertSearchResult> results;
  for (const auto& item : items) {
    if (!MatchQuery(item, query)) {
      continue;
    }
    if (std::optional<QuickInsertClipboardResult::DisplayFormat>
            display_format = GetDisplayFormat(item.display_format());
        display_format.has_value()) {
      results.push_back(QuickInsertClipboardResult(
          item.id(), *display_format, item.file_count(), item.display_text(),
          item.display_image(),
          (clock_->Now() - item.time_copied()) < kRecencyThreshold));
    }
  }
  std::move(callback).Run(std::move(results));
}
}  // namespace ash
