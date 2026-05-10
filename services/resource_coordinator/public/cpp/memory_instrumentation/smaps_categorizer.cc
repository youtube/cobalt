// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.


// TODO(b/375241103): Add unit tests.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/smaps_categorizer.h"

#include <string_view>

#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"
#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "third_party/abseil-cpp/absl/strings/match.h"
#include "third_party/abseil-cpp/absl/strings/numbers.h"

namespace memory_instrumentation {

namespace {

enum class SmapsReliability {
  kSuccess = 0,
  kRollupMissing = 1,
  kAccessDenied = 2,
  kUnreliable = 3,
  kMaxValue = kUnreliable,
};

// Helper to parse a line like "Pss:                328 kB"
bool ParseSmapsLine(std::string_view line, uint64_t* value_kb) {
  size_t colon_pos = line.find(':');
  if (colon_pos == std::string_view::npos) return false;

  std::string_view value_part = line.substr(colon_pos + 1);
  std::vector<std::string_view> tokens = base::SplitStringPiece(
      value_part, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (tokens.empty()) return false;

  return absl::SimpleAtoi(tokens[0], value_kb);
}

}  // namespace

SmapsCategorizer::SmapsCategorizer(DetailedMetricsDelegate* delegate)
    : delegate_(delegate) {
  // Detach from the creation sequence (which is typically the main UI thread).
  // This ensures that the sequence checker will bind to the background sequenced
  // task runner (e.g., MemoryInfra or ThreadPool polling runner) when RequestDump()
  // is first called, verifying sequence affinity on the background runner.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

SmapsCategorizer::~SmapsCategorizer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void SmapsCategorizer::RequestDump(base::OnceClosure done) {
  // Enforce strict sequence affinity. Calling this from other sequences is
  // not supported and will trigger a DCHECK (YAGNI).
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const bool scanning = isScanning();
  pending_callbacks_.push_back(std::move(done));
  if (scanning) return;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&SmapsCategorizer::ScanSmapsFile,
                     base::FilePath("/proc/self/smaps")),
      base::BindOnce(&SmapsCategorizer::OnScanComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SmapsCategorizer::OnScanComplete(std::optional<ParsedSmapsResults> results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  
  if (results.has_value() && delegate_) {
    for (const auto& entry : results.value()) {
      delegate_->OnSmapsEntry(entry.name, entry.metrics);
    }
  }

  base::UmaHistogramExactLinear("Memory.SmapsCategorizer.CoalescedRequestsCount",
                                 static_cast<int>(pending_callbacks_.size()), 50);
  
  std::vector<base::OnceClosure> callbacks;
  callbacks.swap(pending_callbacks_);
  
  for (auto& cb : callbacks) {
    std::move(cb).Run();
  }
}


// static
std::optional<ParsedSmapsResults> SmapsCategorizer::ScanSmapsFile(
    const base::FilePath& path) {
  base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid()) {
    base::UmaHistogramEnumeration("Memory.SmapsCategorizer.Reliability",
                                  SmapsReliability::kAccessDenied);
    return std::nullopt;
  }
  return ScanSmaps(std::move(file));
}

// static
std::optional<ParsedSmapsResults> SmapsCategorizer::ScanSmaps(
    base::File file) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!file.IsValid())
    return std::nullopt;

  ParsedSmapsResults results;
  results.reserve(128);

  char buffer[8192];
  size_t buffer_offset = 0;

  std::string current_name;
  SmapsMetrics current_metrics;
  bool in_entry = false;

  auto flush_entry = [&]() {
    if (in_entry) {
      results.push_back({current_name, current_metrics});
    }
    in_entry = false;
    current_metrics = SmapsMetrics();
    current_name.clear();
  };

  while (true) {
    int bytes_read = UNSAFE_BUFFERS(file.ReadAtCurrentPos(
        buffer + buffer_offset, sizeof(buffer) - buffer_offset));
    if (bytes_read <= 0)
      break;

    size_t total_buffered = buffer_offset + bytes_read;
    std::string_view chunk(buffer, total_buffered);
    size_t last_newline = chunk.find_last_of('\n');

    if (last_newline == std::string_view::npos) {
      // Line too long for buffer or no newline yet.
      // This shouldn't happen for smaps, but we must handle it.
      buffer_offset = total_buffered;
      if (buffer_offset == sizeof(buffer)) {
        // Force flush if buffer is full and no newline.
        LOG(WARNING) << "smaps line exceeded buffer size (" << sizeof(buffer)
                     << " bytes) without newline, discarding line data.";
        buffer_offset = 0;
      }
      continue;
    }

    std::string_view processable(buffer, last_newline + 1);
    base::StringViewTokenizer line_t(processable, "\n");
    while (line_t.GetNext()) {
      std::string_view line = line_t.token_piece();
      if (line.empty())
        continue;

      if (absl::ascii_isxdigit(line[0]) && line.find('-') != std::string_view::npos) {
        // Header line: "00400000-00452000 r-xp 00000000 08:02 173521
        // /usr/bin/dbus-daemon"
        flush_entry();
        in_entry = true;

        base::StringViewTokenizer field_t(line, " ");
        size_t field_count = 0;
        while (field_t.GetNext()) {
          field_count++;
          if (field_count == 5) {
            const char* token_end_ptr = &*field_t.token_end();
            size_t offset = std::distance(line.data(), token_end_ptr);
            size_t remaining = line.size() - offset;
            std::string_view remainder(token_end_ptr, remaining);
            std::string_view name_sv =
                base::TrimWhitespaceASCII(remainder, base::TRIM_ALL);
            current_name = std::string(name_sv);
            break;
          }
        }
      } else if (in_entry) {
        uint64_t val = 0;
        if (absl::StartsWith(line, "Rss:")) {
          if (ParseSmapsLine(line, &val))
            current_metrics.rss_kb = val;
        } else if (absl::StartsWith(line, "Pss:")) {
          if (ParseSmapsLine(line, &val))
            current_metrics.pss_kb = val;
        } else if (absl::StartsWith(line, "Private_Dirty:")) {
          if (ParseSmapsLine(line, &val))
            current_metrics.private_dirty_kb = val;
        } else if (absl::StartsWith(line, "Private_Clean:")) {
          if (ParseSmapsLine(line, &val))
            current_metrics.private_clean_kb = val;
        } else if (absl::StartsWith(line, "Shared_Dirty:")) {
          if (ParseSmapsLine(line, &val))
            current_metrics.shared_dirty_kb = val;
        } else if (absl::StartsWith(line, "Shared_Clean:")) {
          if (ParseSmapsLine(line, &val))
            current_metrics.shared_clean_kb = val;
        } else if (absl::StartsWith(line, "Swap:")) {
          if (ParseSmapsLine(line, &val))
            current_metrics.swap_kb = val;
        } else if (absl::StartsWith(line, "SwapPss:")) {
          if (ParseSmapsLine(line, &val))
            current_metrics.swap_pss_kb = val;
        }
      }
    }

    // Move remainder to the beginning
    size_t remaining = total_buffered - (last_newline + 1);
    if (remaining > 0) {
      UNSAFE_BUFFERS(memmove(buffer, buffer + last_newline + 1, remaining));
    }
    buffer_offset = remaining;
  }

  flush_entry();

  base::UmaHistogramEnumeration("Memory.SmapsCategorizer.Reliability",
                                SmapsReliability::kSuccess);
  return results;
}

}  // namespace memory_instrumentation
