// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/actor/tools/utils/logging_util.h"

#import "base/check.h"
#import "base/strings/stringprintf.h"
#import "components/actor/core/aggregated_journal.h"
#import "components/actor/core/journal_details_builder.h"
#import "url/gurl.h"

namespace actor {

void LogJournalEvent(
    AggregatedJournal& journal,
    const GURL& url,
    ActorTaskId task_id,
    std::string_view event_name,
    const std::vector<std::pair<std::string, std::string>>& details) {
  JournalDetailsBuilder builder;
  for (const auto& [key, value] : details) {
    builder.Add(key, value);
  }

  journal.Log(url, task_id, event_name, std::move(builder).Build());
}

void LogToolExecutionResult(AggregatedJournal& journal,
                            const GURL& url,
                            ActorTaskId task_id,
                            std::string_view event_name,
                            const ToolExecutionResult& result,
                            std::string_view success_details_key) {
  std::vector<mojom::JournalDetailsPtr> details;

  if (!result.IsOk()) {
    details = JournalDetailsBuilder()
                  .AddError(GetToolExecutionResultMessage(result))
                  .Build();
  } else {
    details =
        JournalDetailsBuilder()
            .Add(success_details_key, GetToolExecutionResultMessage(result))
            .Build();
  }

  journal.Log(url, task_id, event_name, std::move(details));
}

}  // namespace actor
