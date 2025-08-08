// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/sql_query_plan_test_util.h"

#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

namespace {

base::FilePath GetExecPath(base::StringPiece name) {
  base::FilePath path;
  base::PathService::Get(base::DIR_EXE, &path);
  return path.AppendASCII(name);
}

class SqlIndexMatcher {
 public:
  using is_gtest_matcher = void;

  // Specifies the type of index that we should match with. Note this also
  // covers primary keys which are implemented as indexes in sqlite.
  enum class Type {
    kAny,         // USING INDEX, or any of the other options with match
    kCovering,    // USING COVERING INDEX
    kPrimaryKey,  // USING PRIMARY KEY
  };

  // Every sqlite index includes a list of indexed columns. However, some query
  // plans will only use a subset of the columns in the index. This matcher is
  // designed to enforce that a given subset of columns are actually used by the
  // query planner. Note that this list doesn't have to be exhaustive, and
  // plans that use a superset of columns listed in `columns` will still match.
  SqlIndexMatcher(std::string name, std::vector<std::string> columns, Type type)
      : name_(std::move(name)), columns_(std::move(columns)), type_(type) {
    switch (type_) {
      case Type::kAny:
      case Type::kCovering:
        CHECK_NE(name_, "");
        break;
      case Type::kPrimaryKey:
        CHECK_EQ(name_, "");
        break;
    }
  }

  bool MatchAndExplain(const SqlQueryPlan&, std::ostream*) const;

  void DescribeTo(std::ostream* out) const {
    return DescribeTo(out, /*negated=*/false);
  }

  void DescribeNegationTo(std::ostream* out) const {
    return DescribeTo(out, /*negated=*/true);
  }

 private:
  void DescribeTo(std::ostream*, bool negated) const;
  size_t FindIndexStart(base::StringPiece plan) const;

  std::string name_;
  std::vector<std::string> columns_;
  Type type_;
};

bool SqlIndexMatcher::MatchAndExplain(const SqlQueryPlan& plan,
                                      std::ostream*) const {
  base::StringPiece plan_piece(plan.plan);

  size_t start_pos = FindIndexStart(plan_piece);
  if (start_pos == std::string::npos) {
    return false;
  }

  size_t end_pos = plan_piece.find("\n", start_pos);

  base::StringPiece index_text =
      plan_piece.substr(start_pos, end_pos - start_pos);

  return base::ranges::all_of(columns_, [index_text](const std::string& col) {
    return base::Contains(index_text, col);
  });
}

size_t SqlIndexMatcher::FindIndexStart(base::StringPiece plan) const {
  std::string covering_prefix = base::StrCat({"USING COVERING INDEX ", name_});
  std::string noncovering_prefix = base::StrCat({"USING INDEX ", name_});
  std::string primary_prefix = "USING PRIMARY KEY ";
  std::string integer_primary_prefix = "USING INTEGER PRIMARY KEY ";
  switch (type_) {
    case SqlIndexMatcher::Type::kCovering:
      return plan.find(covering_prefix);
    case SqlIndexMatcher::Type::kPrimaryKey: {
      size_t pos = plan.find(primary_prefix);
      if (pos != std::string::npos) {
        return pos;
      }
      return plan.find(integer_primary_prefix);
    }
    case SqlIndexMatcher::Type::kAny:
      for (const base::StringPiece prefix :
           {covering_prefix, noncovering_prefix, primary_prefix,
            integer_primary_prefix}) {
        size_t pos = plan.find(prefix);
        if (pos != std::string::npos) {
          return pos;
        }
      }
      return std::string::npos;
  }
}

void SqlIndexMatcher::DescribeTo(std::ostream* out, bool negated) const {
  if (negated) {
    *out << "does not use ";
  } else {
    *out << "uses ";
  }

  switch (type_) {
    case Type::kAny:
      *out << "index " << name_;
      break;
    case Type::kCovering:
      *out << "covering index " << name_;
      break;
    case Type::kPrimaryKey:
      *out << "primary key";
      break;
  }

  if (columns_.empty()) {
    return;
  }

  *out << " with columns";
  const char* separator = " ";

  for (const auto& column : columns_) {
    *out << separator << column;
    separator = ", ";
  }
}

bool HasFullTableScan(const SqlQueryPlan& plan) {
  return base::Contains(plan.plan, "SCAN");
}

}  // namespace

testing::Matcher<SqlQueryPlan> UsesIndex(std::string name,
                                         std::vector<std::string> columns) {
  return SqlIndexMatcher(std::move(name), std::move(columns),
                         SqlIndexMatcher::Type::kAny);
}

testing::Matcher<SqlQueryPlan> UsesCoveringIndex(
    std::string name,
    std::vector<std::string> columns) {
  return SqlIndexMatcher(std::move(name), std::move(columns),
                         SqlIndexMatcher::Type::kCovering);
}

testing::Matcher<SqlQueryPlan> UsesPrimaryKey() {
  return SqlIndexMatcher("", {}, SqlIndexMatcher::Type::kPrimaryKey);
}

std::ostream& operator<<(std::ostream& out, const SqlQueryPlan& plan) {
  return out << plan.query << "\n" << plan.plan;
}

SqlQueryPlanExplainer::SqlQueryPlanExplainer(base::FilePath db_path)
    : db_path_(std::move(db_path)),
      shell_path_(GetExecPath("sqlite_dev_shell")) {}

SqlQueryPlanExplainer::~SqlQueryPlanExplainer() = default;

base::expected<SqlQueryPlan, SqlQueryPlanExplainer::Error>
SqlQueryPlanExplainer::GetPlan(
    std::string query,
    absl::optional<SqlFullScanReason> full_scan_reason) {
  base::CommandLine command_line(shell_path_);
  command_line.AppendArgPath(db_path_);

  std::string explain_query = base::StrCat({"EXPLAIN QUERY PLAN ", query});
  command_line.AppendArg(explain_query);

  std::string output;
  if (!base::GetAppOutputAndError(command_line, &output) ||
      !base::StartsWith(output, "QUERY PLAN")) {
    return base::unexpected(Error::kInvalidOutput);
  }

  SqlQueryPlan plan{
      .query = std::move(query),
      .plan = std::move(output),
  };

  bool plan_has_full_scan = HasFullTableScan(plan);

  if (plan_has_full_scan && !full_scan_reason.has_value()) {
    return base::unexpected(Error::kMissingFullScanAnnotation);
  }

  if (!plan_has_full_scan && full_scan_reason.has_value()) {
    return base::unexpected(Error::kExtraneousFullScanAnnotation);
  }

  return plan;
}

std::ostream& operator<<(std::ostream& out,
                         SqlQueryPlanExplainer::Error error) {
  switch (error) {
    case SqlQueryPlanExplainer::Error::kInvalidOutput:
      return out << "kInvalidOutput";
    case SqlQueryPlanExplainer::Error::kMissingFullScanAnnotation:
      return out << "kMissingFullScanAnnotation";
    case SqlQueryPlanExplainer::Error::kExtraneousFullScanAnnotation:
      return out << "kExtraneousFullScanAnnotation";
  }
}

}  // namespace content
