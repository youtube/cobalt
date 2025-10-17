// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_QUICK_INSERT_CLIPBOARD_HISTORY_PROVIDER_H_
#define ASH_QUICK_INSERT_QUICK_INSERT_CLIPBOARD_HISTORY_PROVIDER_H_

#include <memory>
#include <string>
#include <string_view>

#include "ash/ash_export.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/default_clock.h"

namespace ash {

class ClipboardHistoryItem;

// A provider to fetch clipboard history.
class ASH_EXPORT QuickInsertClipboardHistoryProvider {
 public:
  using OnFetchResultsCallback =
      base::OnceCallback<void(std::vector<QuickInsertSearchResult>)>;

  explicit QuickInsertClipboardHistoryProvider(
      base::Clock* clock = base::DefaultClock::GetInstance());

  QuickInsertClipboardHistoryProvider(
      const QuickInsertClipboardHistoryProvider&) = delete;
  QuickInsertClipboardHistoryProvider& operator=(
      const QuickInsertClipboardHistoryProvider&) = delete;
  ~QuickInsertClipboardHistoryProvider();

  // Fetches clipboard items which were copied within `recency` time duration.
  void FetchResults(OnFetchResultsCallback callback,
                    std::u16string_view query = u"");

 private:
  void OnFetchHistory(OnFetchResultsCallback callback,
                      std::u16string query,
                      std::vector<ClipboardHistoryItem> items);

  raw_ptr<base::Clock> clock_;
  base::WeakPtrFactory<QuickInsertClipboardHistoryProvider> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_QUICK_INSERT_CLIPBOARD_HISTORY_PROVIDER_H_
