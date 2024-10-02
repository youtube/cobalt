// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_PARSER_METRICS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_PARSER_METRICS_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"

namespace ukm {
class UkmRecorder;
}

namespace blink {

// Store and report metrics data for the HTMLDocumentParser.
class CORE_EXPORT HTMLParserMetrics {
 public:
  HTMLParserMetrics(int64_t source_id, ukm::UkmRecorder*);
  HTMLParserMetrics(const HTMLParserMetrics&) = delete;
  HTMLParserMetrics& operator=(const HTMLParserMetrics&) = delete;
  ~HTMLParserMetrics() = default;

  void AddChunk(base::TimeDelta elapsed_time, unsigned tokens_parsed);

  void AddYieldInterval(base::TimeDelta elapsed_time);

  void AddInput(unsigned length);

  void ReportMetricsAtParseEnd();

  unsigned chunk_count() const { return chunk_count_; }

 private:
  void ReportUMAs();

  // UKM System data.
  const int64_t source_id_;
  ukm::UkmRecorder* const recorder_;

  // Metrics data.
  unsigned chunk_count_ = 0;                  // For computing averages.
  base::TimeDelta accumulated_parsing_time_;  // Constructed with 0 value
  base::TimeDelta min_parsing_time_ = base::TimeDelta::Max();
  base::TimeDelta max_parsing_time_;  // Constructed with 0 value
  unsigned total_tokens_parsed_ = 0;
  unsigned min_tokens_parsed_ = UINT_MAX;
  unsigned max_tokens_parsed_ = 0;

  // Yield count may not equal chunk count - 1. That is, there is not
  // always one yield between every pair of chunks.
  unsigned yield_count_ = 0;
  base::TimeDelta accumulated_yield_intervals_;  // Constructed with 0 value
  base::TimeDelta min_yield_interval_ = base::TimeDelta::Max();
  base::TimeDelta max_yield_interval_;  // Constructed with 0 value

  // Track total number of characters parsed in one instantiation of the
  // parser.
  unsigned input_character_count_ = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_PARSER_METRICS_H_
