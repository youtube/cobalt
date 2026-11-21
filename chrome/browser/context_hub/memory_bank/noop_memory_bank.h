// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_NOOP_MEMORY_BANK_H_
#define CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_NOOP_MEMORY_BANK_H_

#include "chrome/browser/context_hub/memory_bank/memory_bank.h"

namespace context_hub {

// A no-op implementation of the MemoryBank interface.
// Used when the MemoryBanks feature flag is disabled.
// All operations are no-ops that immediately run callbacks.
class NoOpMemoryBank : public MemoryBank {
 public:
  NoOpMemoryBank();
  ~NoOpMemoryBank() override;

  NoOpMemoryBank(const NoOpMemoryBank&) = delete;
  NoOpMemoryBank& operator=(const NoOpMemoryBank&) = delete;

  // MemoryBank:
  void SaveTab(const GURL& url,
               const std::string& tab_title,
               OperationCompleteCallback callback) override;
  void SaveTextSelection(const GURL& url,
                         const std::string& tab_title,
                         const std::string& selected_text,
                         OperationCompleteCallback callback) override;
  void GetAllEntries(GetAllEntriesCallback callback) const override;
  void DeleteEntry(int64_t id, OperationCompleteCallback callback) override;
};

}  // namespace context_hub

#endif  // CHROME_BROWSER_CONTEXT_HUB_MEMORY_BANK_NOOP_MEMORY_BANK_H_
