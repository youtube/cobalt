// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/metrics/quick_insert_session_metrics.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/quick_insert/quick_insert_category.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "base/functional/overloaded.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/ime/text_input_client.h"

namespace ash {
namespace {

namespace cros_events = metrics::structured::events::v2::cr_os_events;

constexpr int kCapsLockCountThreshold = 20;

cros_events::PickerInputFieldType GetInputFieldType(
    ui::TextInputClient* client) {
  if (client == nullptr) {
    return cros_events::PickerInputFieldType::NONE;
  }
  switch (client->GetTextInputType()) {
    case ui::TEXT_INPUT_TYPE_NONE:
      return cros_events::PickerInputFieldType::NONE;
    case ui::TEXT_INPUT_TYPE_TEXT:
    case ui::TEXT_INPUT_TYPE_TEXT_AREA:
      if (client->CanInsertImage()) {
        return cros_events::PickerInputFieldType::RICH_TEXT;
      } else {
        return cros_events::PickerInputFieldType::PLAIN_TEXT;
      }
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      return cros_events::PickerInputFieldType::PASSWORD;
    case ui::TEXT_INPUT_TYPE_SEARCH:
      return cros_events::PickerInputFieldType::SEARCH;
    case ui::TEXT_INPUT_TYPE_EMAIL:
      return cros_events::PickerInputFieldType::EMAIL;
    case ui::TEXT_INPUT_TYPE_NUMBER:
      return cros_events::PickerInputFieldType::NUMBER;
    case ui::TEXT_INPUT_TYPE_TELEPHONE:
      return cros_events::PickerInputFieldType::TELEPHONE;
    case ui::TEXT_INPUT_TYPE_URL:
      return cros_events::PickerInputFieldType::URL;
    case ui::TEXT_INPUT_TYPE_DATE:
    case ui::TEXT_INPUT_TYPE_DATE_TIME:
    case ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL:
    case ui::TEXT_INPUT_TYPE_MONTH:
    case ui::TEXT_INPUT_TYPE_TIME:
    case ui::TEXT_INPUT_TYPE_WEEK:
    case ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD:
      return cros_events::PickerInputFieldType::DATE_TIME;
    case ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE:
    case ui::TEXT_INPUT_TYPE_NULL:
      return cros_events::PickerInputFieldType::OTHER;
  }
}

int GetSelectionLength(ui::TextInputClient* client) {
  if (!client) {
    return 0;
  }
  gfx::Range selection_range;
  client->GetEditableSelectionRange(&selection_range);
  if (selection_range.IsValid() && !selection_range.is_empty()) {
    return selection_range.length();
  }
  return 0;
}

cros_events::PickerSessionOutcome ConvertToCrosEventSessionOutcome(
    QuickInsertSessionMetrics::SessionOutcome outcome) {
  switch (outcome) {
    case QuickInsertSessionMetrics::SessionOutcome::kUnknown:
      return cros_events::PickerSessionOutcome::UNKNOWN;
    case QuickInsertSessionMetrics::SessionOutcome::kInsertedOrCopied:
      return cros_events::PickerSessionOutcome::INSERTED_OR_COPIED;
    case QuickInsertSessionMetrics::SessionOutcome::kAbandoned:
      return cros_events::PickerSessionOutcome::ABANDONED;
    case QuickInsertSessionMetrics::SessionOutcome::kRedirected:
      return cros_events::PickerSessionOutcome::REDIRECTED;
    case QuickInsertSessionMetrics::SessionOutcome::kFormat:
      return cros_events::PickerSessionOutcome::FORMAT;
    case QuickInsertSessionMetrics::SessionOutcome::kOpenFile:
      return cros_events::PickerSessionOutcome::OPEN_FILE;
    case QuickInsertSessionMetrics::SessionOutcome::kOpenLink:
      return cros_events::PickerSessionOutcome::OPEN_LINK;
    case QuickInsertSessionMetrics::SessionOutcome::kCreate:
      return cros_events::PickerSessionOutcome::CREATE;
  }
}

cros_events::PickerAction ConvertToCrosEventAction(
    std::optional<QuickInsertCategory> action) {
  if (!action.has_value()) {
    return cros_events::PickerAction::UNKNOWN;
  }
  switch (*action) {
    case QuickInsertCategory::kEditorWrite:
      return cros_events::PickerAction::OPEN_EDITOR_WRITE;
    case QuickInsertCategory::kEditorRewrite:
      return cros_events::PickerAction::OPEN_EDITOR_REWRITE;
    case QuickInsertCategory::kLobsterWithSelectedText:
    case QuickInsertCategory::kLobsterWithNoSelectedText:
      return cros_events::PickerAction::OPEN_LOBSTER;
    case QuickInsertCategory::kLinks:
      return cros_events::PickerAction::OPEN_LINKS;
    case QuickInsertCategory::kEmojisGifs:
    case QuickInsertCategory::kEmojis:
      return cros_events::PickerAction::OPEN_EXPRESSIONS;
    case QuickInsertCategory::kGifs:
      return cros_events::PickerAction::OPEN_GIFS;
    case QuickInsertCategory::kClipboard:
      return cros_events::PickerAction::OPEN_CLIPBOARD;
    case QuickInsertCategory::kDriveFiles:
      return cros_events::PickerAction::OPEN_DRIVE_FILES;
    case QuickInsertCategory::kLocalFiles:
      return cros_events::PickerAction::OPEN_LOCAL_FILES;
    case QuickInsertCategory::kDatesTimes:
      return cros_events::PickerAction::OPEN_DATES_TIMES;
    case QuickInsertCategory::kUnitsMaths:
      return cros_events::PickerAction::OPEN_UNITS_MATHS;
  }
}

cros_events::PickerResultSource GetResultSource(
    std::optional<QuickInsertSearchResult> result) {
  if (!result.has_value()) {
    return cros_events::PickerResultSource::UNKNOWN;
  }
  using ReturnType = cros_events::PickerResultSource;
  return std::visit(
      base::Overloaded{
          [](const QuickInsertTextResult& data) {
            switch (data.source) {
              case QuickInsertTextResult::Source::kUnknown:
                return cros_events::PickerResultSource::UNKNOWN;
              case QuickInsertTextResult::Source::kDate:
                return cros_events::PickerResultSource::DATES_TIMES;
              case QuickInsertTextResult::Source::kMath:
                return cros_events::PickerResultSource::UNITS_MATHS;
              case QuickInsertTextResult::Source::kCaseTransform:
                return cros_events::PickerResultSource::CASE_TRANSFORM;
              case QuickInsertTextResult::Source::kOmnibox:
                return cros_events::PickerResultSource::OMNIBOX;
            }
          },
          [](const QuickInsertEmojiResult& data) {
            return cros_events::PickerResultSource::EMOJI;
          },
          [](const QuickInsertClipboardResult& data) {
            return cros_events::PickerResultSource::CLIPBOARD;
          },
          [](const QuickInsertGifResult& data) {
            return cros_events::PickerResultSource::TENOR;
          },
          [](const QuickInsertBrowsingHistoryResult& data) {
            return cros_events::PickerResultSource::OMNIBOX;
          },
          [](const QuickInsertLocalFileResult& data) {
            return cros_events::PickerResultSource::LOCAL_FILES;
          },
          [](const QuickInsertDriveFileResult& data) {
            return cros_events::PickerResultSource::DRIVE_FILES;
          },
          [](const QuickInsertCategoryResult& data) -> ReturnType {
            NOTREACHED();
          },
          [](const QuickInsertSearchRequestResult& data) -> ReturnType {
            NOTREACHED();
          },
          [](const QuickInsertEditorResult& data) -> ReturnType {
            NOTREACHED();
          },
          [](const QuickInsertLobsterResult& data) -> ReturnType {
            NOTREACHED();
          },
          [](const QuickInsertNewWindowResult& data) -> ReturnType {
            return cros_events::PickerResultSource::UNKNOWN;
          },
          [](const QuickInsertCapsLockResult& data) -> ReturnType {
            return cros_events::PickerResultSource::UNKNOWN;
          },
          [](const QuickInsertCaseTransformResult& data) -> ReturnType {
            return cros_events::PickerResultSource::CASE_TRANSFORM;
          },
      },
      *result);
}

cros_events::PickerResultType GetResultType(
    std::optional<QuickInsertSearchResult> result) {
  if (!result.has_value()) {
    return cros_events::PickerResultType::UNKNOWN;
  }
  using ReturnType = cros_events::PickerResultType;
  return std::visit(
      base::Overloaded{
          [](const QuickInsertTextResult& data) {
            return cros_events::PickerResultType::TEXT;
          },
          [](const QuickInsertEmojiResult& data) {
            switch (data.type) {
              case QuickInsertEmojiResult::Type::kEmoji:
                return cros_events::PickerResultType::EMOJI;
              case QuickInsertEmojiResult::Type::kSymbol:
                return cros_events::PickerResultType::SYMBOL;
              case QuickInsertEmojiResult::Type::kEmoticon:
                return cros_events::PickerResultType::EMOTICON;
            }
          },
          [](const QuickInsertClipboardResult& data) {
            switch (data.display_format) {
              case QuickInsertClipboardResult::DisplayFormat::kFile:
                return cros_events::PickerResultType::CLIPBOARD_FILE;
              case QuickInsertClipboardResult::DisplayFormat::kText:
                return cros_events::PickerResultType::CLIPBOARD_TEXT;
              case QuickInsertClipboardResult::DisplayFormat::kImage:
                return cros_events::PickerResultType::CLIPBOARD_IMAGE;
              case QuickInsertClipboardResult::DisplayFormat::kHtml:
                return cros_events::PickerResultType::CLIPBOARD_HTML;
            }
          },
          [](const QuickInsertGifResult& data) {
            return cros_events::PickerResultType::GIF;
          },
          [](const QuickInsertBrowsingHistoryResult& data) {
            return cros_events::PickerResultType::LINK;
          },
          [](const QuickInsertLocalFileResult& data) {
            return cros_events::PickerResultType::LOCAL_FILE;
          },
          [](const QuickInsertDriveFileResult& data) {
            return cros_events::PickerResultType::DRIVE_FILE;
          },
          [](const QuickInsertCategoryResult& data) -> ReturnType {
            NOTREACHED();
          },
          [](const QuickInsertSearchRequestResult& data) -> ReturnType {
            NOTREACHED();
          },
          [](const QuickInsertEditorResult& data) -> ReturnType {
            NOTREACHED();
          },
          [](const QuickInsertLobsterResult& data) -> ReturnType {
            NOTREACHED();
          },
          [](const QuickInsertNewWindowResult& data) -> ReturnType {
            return cros_events::PickerResultType::UNKNOWN;
          },
          [](const QuickInsertCapsLockResult& data) -> ReturnType {
            return cros_events::PickerResultType::UNKNOWN;
          },
          [](const QuickInsertCaseTransformResult& data) -> ReturnType {
            return cros_events::PickerResultType::TEXT;
          },
      },
      *result);
}

}  // namespace

QuickInsertSessionMetrics::QuickInsertSessionMetrics() = default;

QuickInsertSessionMetrics::QuickInsertSessionMetrics(PrefService* prefs)
    : prefs_(prefs) {}

QuickInsertSessionMetrics::~QuickInsertSessionMetrics() {
  OnFinishSession();
}

void QuickInsertSessionMetrics::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kQuickInsertLockSelectedCountPrefName,
                                0);
  registry->RegisterIntegerPref(
      prefs::kQuickInsertCapsLockDisplayedCountPrefName, 0);
}

void QuickInsertSessionMetrics::SetOutcome(SessionOutcome outcome) {
  if (outcome_ == SessionOutcome::kUnknown) {
    outcome_ = outcome;
  }
}

void QuickInsertSessionMetrics::SetSelectedCategory(
    QuickInsertCategory category) {
  if (!last_category_.has_value()) {
    last_category_ = category;
  }
}

void QuickInsertSessionMetrics::SetSelectedResult(
    QuickInsertSearchResult selected_result,
    int index) {
  if (!selected_result_.has_value()) {
    selected_result_ = std::move(selected_result);
    result_index_ = index;
  }
}

void QuickInsertSessionMetrics::UpdateSearchQuery(
    std::u16string_view search_query) {
  int new_length = static_cast<int>(search_query.length());
  search_query_total_edits_ += abs(new_length - search_query_length_);
  search_query_length_ = new_length;
}

void QuickInsertSessionMetrics::OnStartSession(ui::TextInputClient* client) {
  metrics::structured::StructuredMetricsClient::Record(
      std::move(cros_events::Picker_StartSession()
                    .SetInputFieldType(GetInputFieldType(client))
                    .SetSelectionLength(
                        static_cast<int64_t>(GetSelectionLength(client)))));
}

void QuickInsertSessionMetrics::OnFinishSession() {
  if (caps_lock_displayed_) {
    UpdateCapLockPrefs(
        selected_result_.has_value() &&
        std::holds_alternative<QuickInsertCapsLockResult>(*selected_result_));
  }
  base::UmaHistogramEnumeration("Ash.Picker.Session.Outcome", outcome_);
  metrics::structured::StructuredMetricsClient::Record(
      cros_events::Picker_FinishSession()
          .SetOutcome(ConvertToCrosEventSessionOutcome(outcome_))
          .SetAction(ConvertToCrosEventAction(last_category_))
          .SetResultSource(GetResultSource(std::move(selected_result_)))
          .SetResultType(GetResultType(std::move(selected_result_)))
          .SetTotalEdits(search_query_total_edits_)
          .SetFinalQuerySize(search_query_length_)
          .SetResultIndex(result_index_));
}

void QuickInsertSessionMetrics::SetCapsLockDisplayed(bool displayed) {
  caps_lock_displayed_ = displayed;
}

void QuickInsertSessionMetrics::UpdateCapLockPrefs(bool caps_lock_selected) {
  if (prefs_ == nullptr) {
    return;
  }
  int caps_lock_displayed_count =
      prefs_->GetInteger(prefs::kQuickInsertCapsLockDisplayedCountPrefName) + 1;
  int caps_lock_selected_count =
      prefs_->GetInteger(prefs::kQuickInsertLockSelectedCountPrefName);
  if (caps_lock_selected) {
    ++caps_lock_selected_count;
  }
  // We will only use caps_lock_selected_count / caps_lock_displayed_count to
  // decide the position of caps lock toggle. We halves both numbers so that
  // they don't grow infinitely and later usages have more weights in decision
  // making. The remainders in division is not significant in our use cases.
  if (caps_lock_displayed_count >= kCapsLockCountThreshold) {
    caps_lock_displayed_count /= 2;
    caps_lock_selected_count /= 2;
  }
  prefs_->SetInteger(prefs::kQuickInsertCapsLockDisplayedCountPrefName,
                     caps_lock_displayed_count);
  prefs_->SetInteger(prefs::kQuickInsertLockSelectedCountPrefName,
                     caps_lock_selected_count);
}

}  // namespace ash
