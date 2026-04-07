// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/smaps_categorizer.h"

#include <vector>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace memory_instrumentation {

namespace {

uint32_t ParseMetricValue(base::StringPiece line,
                          const std::string& metric_name) {
  if (base::StartsWith(line, metric_name, base::CompareCase::SENSITIVE)) {
    base::StringPiece value_part = line.substr(metric_name.size());
    value_part = base::TrimWhitespaceASCII(value_part, base::TRIM_ALL);

    // Smaps values are like "123 kB". We want "123".
    size_t space_pos = value_part.find(' ');
    if (space_pos != base::StringPiece::npos) {
      value_part = value_part.substr(0, space_pos);
    }

    uint32_t val_kb = 0;
    if (base::StringToUint(value_part, &val_kb)) {
      return val_kb;
    }
  }
  return 0;
}

void UpdateStats(mojom::DetailedMemoryStats& stats,
                 mojom::CobaltMemoryCategory category,
                 base::StringPiece line) {
  uint32_t pss_kb = ParseMetricValue(line, "Pss:");
  if (pss_kb > 0) {
    stats.categories_kb[category] += pss_kb;
  }
}

bool IsHeaderLine(base::StringPiece line, std::string* mapped_file) {
  // A header line typically starts with an address range (hex-hex)
  // 7fb8322000-7fb8323000 r--p 00000000 00:00 0 [stack:1234]
  if (line.empty() || !base::IsHexDigit(line[0])) {
    return false;
  }

  // Fast check for address range: hex followed by '-'
  size_t first_space = line.find(' ');
  if (first_space == base::StringPiece::npos) {
    return false;
  }
  base::StringPiece address_range = line.substr(0, first_space);
  if (address_range.find('-') == base::StringPiece::npos) {
    return false;
  }

  // Find the 6th column (the mapped file path)
  // Columns: address perms offset dev inode [pathname]
  size_t current_pos = first_space;
  for (int i = 0; i < 4; ++i) {
    current_pos = line.find_first_not_of(' ', current_pos);
    if (current_pos == base::StringPiece::npos) {
      return false;
    }
    current_pos = line.find_first_of(' ', current_pos);
    if (current_pos == base::StringPiece::npos) {
      // If we are at the 5th column and there's no space, it means no pathname.
      if (i == 3) {
        mapped_file->clear();
        return true;
      }
      return false;
    }
  }

  base::StringPiece path = line.substr(current_pos);
  path = base::TrimWhitespaceASCII(path, base::TRIM_ALL);
  *mapped_file = std::string(path);
  return true;
}

}  // namespace

mojom::DetailedMemoryStats SmapsCategorizer::ParseSmaps(
    const base::FilePath& smaps_path,
    const Delegate& delegate) {
  mojom::DetailedMemoryStats stats;
  base::ScopedFILE f(base::OpenFile(smaps_path, "r"));
  if (!f) {
    DPLOG(ERROR) << "Failed to open smaps file (check permissions): "
                 << smaps_path.value();
    return stats;
  }

  const uint32_t kMaxLineSize = 4096;
  char line[kMaxLineSize];
  mojom::CobaltMemoryCategory current_category =
      mojom::CobaltMemoryCategory::kUnknown;
  while (fgets(line, kMaxLineSize, f.get())) {
    base::StringPiece line_piece(line);
    line_piece = base::TrimWhitespaceASCII(line_piece, base::TRIM_TRAILING);

    std::string mapped_file;
    if (IsHeaderLine(line_piece, &mapped_file)) {
      current_category = delegate.GetCategory(mapped_file);
    } else {
      UpdateStats(stats, current_category, line_piece);
    }
  }

  return stats;
}

mojom::DetailedMemoryStats SmapsCategorizer::ParseSmapsContent(
    const std::string& content,
    const Delegate& delegate) {
  mojom::DetailedMemoryStats stats;
  mojom::CobaltMemoryCategory current_category =
      mojom::CobaltMemoryCategory::kUnknown;

  std::vector<base::StringPiece> lines = base::SplitStringPiece(
      content, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);

  for (base::StringPiece line : lines) {
    line = base::TrimWhitespaceASCII(line, base::TRIM_TRAILING);
    std::string mapped_file;
    if (IsHeaderLine(line, &mapped_file)) {
      current_category = delegate.GetCategory(mapped_file);
    } else {
      UpdateStats(stats, current_category, line);
    }
  }

  return stats;
}

uint32_t SmapsCategorizer::ParseSmapsMetric(const base::FilePath& smaps_path,
                                            const std::string& metric_name) {
  base::ScopedFILE f(base::OpenFile(smaps_path, "r"));
  if (!f) {
    DPLOG(ERROR) << "Failed to open smaps file for metric: "
                 << smaps_path.value();
    return 0;
  }

  const uint32_t kMaxLineSize = 4096;
  char line[kMaxLineSize];
  uint32_t total_kb = 0;
  while (fgets(line, kMaxLineSize, f.get())) {
    base::StringPiece line_piece(line);
    line_piece = base::TrimWhitespaceASCII(line_piece, base::TRIM_TRAILING);
    total_kb += ParseMetricValue(line_piece, metric_name);
  }
  return total_kb;
}

uint32_t SmapsCategorizer::ParseSmapsMetricContent(
    const std::string& content,
    const std::string& metric_name) {
  uint32_t total_kb = 0;
  std::vector<base::StringPiece> lines = base::SplitStringPiece(
      content, "\n", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  for (base::StringPiece line : lines) {
    line = base::TrimWhitespaceASCII(line, base::TRIM_TRAILING);
    total_kb += ParseMetricValue(line, metric_name);
  }
  return total_kb;
}

}  // namespace memory_instrumentation
