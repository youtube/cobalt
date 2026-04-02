// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/smaps_categorizer.h"

#include <cmath>

#include "base/containers/flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/detailed_metrics_delegate.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/os_metrics.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#include "third_party/abseil-cpp/absl/strings/match.h"
#include "third_party/abseil-cpp/absl/strings/numbers.h"
#include "third_party/abseil-cpp/absl/strings/strip.h"

namespace memory_instrumentation {

namespace {

constexpr base::TimeDelta kYieldThreshold = base::Milliseconds(1);

}  // namespace

SmapsCategorizer::SmapsCategorizer(DetailedMetricsDelegate* delegate)
    : delegate_(delegate) {}

SmapsCategorizer::~SmapsCategorizer() = default;

void SmapsCategorizer::Start(std::vector<char> buffer,
                             mojom::RawOSMemDump* dump,
                             DoneCallback callback) {
  if (done_callback_) {
    std::move(done_callback_).Run(false);
  }
  weak_ptr_factory_.InvalidateWeakPtrs();
  buffer_ = std::move(buffer);
  dump_ = dump;
  done_callback_ = std::move(callback);
  current_pos_ = 0;
  total_pss_kb_ = 0;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&SmapsCategorizer::ParseNextChunk,
                                weak_ptr_factory_.GetWeakPtr()));
}

void SmapsCategorizer::ParseNextChunk() {
  base::ElapsedTimer timer;
  absl::string_view content(buffer_.data(), buffer_.size());

  while (current_pos_ < content.size()) {
    size_t line_end = content.find('\n', current_pos_);
    absl::string_view line = (line_end == absl::string_view::npos)
                                 ? content.substr(current_pos_)
                                 : content.substr(current_pos_, line_end - current_pos_);

    // Check if it's a header line (starts with hex address range).
    if (absl::ascii_isxdigit(static_cast<unsigned char>(line[0]))) {
      if (delegate_) {
        delegate_->OnSmapsHeader(line);
      }
    } else {
      // Check if it's a counter line (e.g. "Pss:  12 kB").
      size_t colon_pos = line.find(':');
      if (colon_pos != absl::string_view::npos) {
        absl::string_view name = line.substr(0, colon_pos);
        absl::string_view value_part = line.substr(colon_pos + 1);
        value_part = absl::StripAsciiWhitespace(value_part);
        value_part = absl::StripSuffix(value_part, " kB");
        uint64_t value_kb = 0;
        if (absl::SimpleAtoi(value_part, &value_kb)) {
          if (name == "Pss") {
            total_pss_kb_ += value_kb;
          }
          if (delegate_) {
            delegate_->OnSmapsCounter(name, value_kb);
          }
        }
      }
    }

    current_pos_ = (line_end == absl::string_view::npos) ? content.size()
                                                        : line_end + 1;

    // Yield control if we've been parsing for too long.
    if (timer.Elapsed() > kYieldThreshold) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&SmapsCategorizer::ParseNextChunk,
                                    weak_ptr_factory_.GetWeakPtr()));
      return;
    }
  }

  // Parse complete. Populated the dump with generic stats from delegate.
  if (delegate_ && dump_) {
    // Validate against smaps_rollup (already in dump_->pss_kb) with 1% epsilon.
    // If discrepancy is too large, the snapshot might be inconsistent.
    bool validation_success = true;
    if (dump_->pss_kb > 0) {
      uint64_t epsilon = dump_->pss_kb / 100;
      if (std::abs(static_cast<int64_t>(total_pss_kb_) -
                   static_cast<int64_t>(dump_->pss_kb)) >
          static_cast<int64_t>(epsilon)) {
        validation_success = false;
        DLOG(WARNING) << "Smaps snapshot validation failed. Snapshot PSS: "
                      << total_pss_kb_ << " KB, Rollup PSS: " << dump_->pss_kb
                      << " KB";
        base::UmaHistogramEnumeration("Memory.DetailedDump.AbortReason",
                                      OSMetrics::DetailedDumpAbortReason::kSmapsValidationFailed);
      }
    }

    if (validation_success) {
      dump_->detailed_stats_kb = delegate_->GetAndResetStats();
      dump_->last_detailed_dump_time = base::TimeTicks::Now();
    } else {
      // Clear delegate stats to avoid using invalid data.
      delegate_->GetAndResetStats();
    }
  }

  // Clear buffer and swap for guaranteed deallocation.
  std::vector<char>().swap(buffer_);

  std::move(done_callback_).Run(true);
}

}  // namespace memory_instrumentation
