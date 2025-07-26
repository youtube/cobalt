/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "src/trace_processor/prelude/table_functions/experimental_slice_layout.h"

#include <algorithm>

#include "src/trace_processor/containers/bit_vector.h"
#include "test/gtest_and_gmock.h"

namespace perfetto {
namespace trace_processor {
namespace {

constexpr uint32_t kColumn =
    tables::ExperimentalSliceLayoutTable::ColumnIndex::filter_track_ids;

std::string ToVis(const Table& table) {
  const Column* layout_depth_column = table.GetColumnByName("layout_depth");
  const Column* ts_column = table.GetColumnByName("ts");
  const Column* dur_column = table.GetColumnByName("dur");
  const Column* filter_track_ids_column =
      table.GetColumnByName("filter_track_ids");

  std::vector<std::string> lines;
  for (uint32_t i = 0; i < table.row_count(); ++i) {
    int64_t layout_depth = layout_depth_column->Get(i).long_value;
    int64_t ts = ts_column->Get(i).long_value;
    int64_t dur = dur_column->Get(i).long_value;
    const char* filter_track_ids = filter_track_ids_column->Get(i).AsString();
    if (std::string("") == filter_track_ids) {
      continue;
    }
    for (int64_t j = 0; j < dur; ++j) {
      size_t y = static_cast<size_t>(layout_depth);
      size_t x = static_cast<size_t>(ts + j);
      while (lines.size() <= y) {
        lines.push_back("");
      }
      if (lines[y].size() <= x) {
        lines[y].resize(x + 1, ' ');
      }
      lines[y][x] = '#';
    }
  }

  std::string output = "";
  output += "\n";
  for (const std::string& line : lines) {
    output += line;
    output += "\n";
  }
  return output;
}

void ExpectOutput(const Table& table, const std::string& expected) {
  const auto& actual = ToVis(table);
  EXPECT_EQ(actual, expected)
      << "Actual:" << actual << "\nExpected:" << expected;
}

tables::SliceTable::Id Insert(tables::SliceTable* table,
                              int64_t ts,
                              int64_t dur,
                              uint32_t track_id,
                              StringId name,
                              std::optional<tables::SliceTable::Id> parent_id) {
  tables::SliceTable::Row row;
  row.ts = ts;
  row.dur = dur;
  row.depth = 0;
  std::optional<tables::SliceTable::Id> id = parent_id;
  while (id) {
    row.depth++;
    id = table->parent_id()[id.value().value];
  }
  row.track_id = tables::TrackTable::Id{track_id};
  row.name = name;
  row.parent_id = parent_id;
  return table->Insert(row).id;
}

TEST(ExperimentalSliceLayoutTest, SingleRow) {
  StringPool pool;
  tables::SliceTable slice_table(&pool);
  StringId name = pool.InternString("SingleRow");

  Insert(&slice_table, 1 /*ts*/, 5 /*dur*/, 1 /*track_id*/, name,
         std::nullopt /*parent*/);

  ExperimentalSliceLayout gen(&pool, &slice_table);

  std::unique_ptr<Table> table;
  auto status = gen.ComputeTable(
      {Constraint{kColumn, FilterOp::kEq, SqlValue::String("1")}}, {},
      BitVector(), table);
  EXPECT_TRUE(status.ok());
  ExpectOutput(*table, R"(
 #####
)");
}

TEST(ExperimentalSliceLayoutTest, DoubleRow) {
  StringPool pool;
  tables::SliceTable slice_table(&pool);
  StringId name = pool.InternString("SingleRow");

  auto id = Insert(&slice_table, 1 /*ts*/, 5 /*dur*/, 1 /*track_id*/, name,
                   std::nullopt);
  Insert(&slice_table, 1 /*ts*/, 5 /*dur*/, 1 /*track_id*/, name, id);

  ExperimentalSliceLayout gen(&pool, &slice_table);

  std::unique_ptr<Table> table;
  auto status = gen.ComputeTable(
      {Constraint{kColumn, FilterOp::kEq, SqlValue::String("1")}}, {},
      BitVector(), table);
  EXPECT_TRUE(status.ok());
  ExpectOutput(*table, R"(
 #####
 #####
)");
}

TEST(ExperimentalSliceLayoutTest, MultipleRows) {
  StringPool pool;
  tables::SliceTable slice_table(&pool);
  StringId name = pool.InternString("MultipleRows");

  auto a = Insert(&slice_table, 1 /*ts*/, 5 /*dur*/, 1 /*track_id*/, name,
                  std::nullopt);
  auto b = Insert(&slice_table, 1 /*ts*/, 4 /*dur*/, 1 /*track_id*/, name, a);
  auto c = Insert(&slice_table, 1 /*ts*/, 3 /*dur*/, 1 /*track_id*/, name, b);
  auto d = Insert(&slice_table, 1 /*ts*/, 2 /*dur*/, 1 /*track_id*/, name, c);
  auto e = Insert(&slice_table, 1 /*ts*/, 1 /*dur*/, 1 /*track_id*/, name, d);
  base::ignore_result(e);

  ExperimentalSliceLayout gen(&pool, &slice_table);

  std::unique_ptr<Table> table;
  auto status = gen.ComputeTable(
      {Constraint{kColumn, FilterOp::kEq, SqlValue::String("1")}}, {},
      BitVector(), table);
  EXPECT_TRUE(status.ok());
  ExpectOutput(*table, R"(
 #####
 ####
 ###
 ##
 #
)");
}

TEST(ExperimentalSliceLayoutTest, MultipleTracks) {
  StringPool pool;
  tables::SliceTable slice_table(&pool);
  StringId name1 = pool.InternString("Slice1");
  StringId name2 = pool.InternString("Slice2");
  StringId name3 = pool.InternString("Slice3");
  StringId name4 = pool.InternString("Track4");

  auto a = Insert(&slice_table, 1 /*ts*/, 4 /*dur*/, 1 /*track_id*/, name1,
                  std::nullopt);
  auto b = Insert(&slice_table, 1 /*ts*/, 2 /*dur*/, 1 /*track_id*/, name2, a);
  auto x = Insert(&slice_table, 4 /*ts*/, 4 /*dur*/, 2 /*track_id*/, name3,
                  std::nullopt);
  auto y = Insert(&slice_table, 4 /*ts*/, 2 /*dur*/, 2 /*track_id*/, name4, x);
  base::ignore_result(b);
  base::ignore_result(y);

  ExperimentalSliceLayout gen(&pool, &slice_table);

  std::unique_ptr<Table> table;
  auto status = gen.ComputeTable(
      {Constraint{kColumn, FilterOp::kEq, SqlValue::String("1,2")}}, {},
      BitVector(), table);
  EXPECT_TRUE(status.ok());
  ExpectOutput(*table, R"(
 ####
 ##
    ####
    ##
)");
}

TEST(ExperimentalSliceLayoutTest, MultipleTracksWithGap) {
  StringPool pool;
  tables::SliceTable slice_table(&pool);
  StringId name1 = pool.InternString("Slice1");
  StringId name2 = pool.InternString("Slice2");
  StringId name3 = pool.InternString("Slice3");
  StringId name4 = pool.InternString("Slice4");
  StringId name5 = pool.InternString("Slice5");
  StringId name6 = pool.InternString("Slice6");

  auto a = Insert(&slice_table, 0 /*ts*/, 4 /*dur*/, 1 /*track_id*/, name1,
                  std::nullopt);
  auto b = Insert(&slice_table, 0 /*ts*/, 2 /*dur*/, 1 /*track_id*/, name2, a);
  auto p = Insert(&slice_table, 3 /*ts*/, 4 /*dur*/, 2 /*track_id*/, name3,
                  std::nullopt);
  auto q = Insert(&slice_table, 3 /*ts*/, 2 /*dur*/, 2 /*track_id*/, name4, p);
  auto x = Insert(&slice_table, 5 /*ts*/, 4 /*dur*/, 1 /*track_id*/, name5,
                  std::nullopt);
  auto y = Insert(&slice_table, 5 /*ts*/, 2 /*dur*/, 1 /*track_id*/, name6, x);
  base::ignore_result(b);
  base::ignore_result(q);
  base::ignore_result(y);

  ExperimentalSliceLayout gen(&pool, &slice_table);

  std::unique_ptr<Table> table;
  auto status = gen.ComputeTable(
      {Constraint{kColumn, FilterOp::kEq, SqlValue::String("1,2")}}, {},
      BitVector(), table);
  EXPECT_TRUE(status.ok());
  ExpectOutput(*table, R"(
#### ####
##   ##
   ####
   ##
)");
}

TEST(ExperimentalSliceLayoutTest, PreviousGroupFullyNested) {
  StringPool pool;
  tables::SliceTable slice_table(&pool);
  StringId name = pool.InternString("Slice");

  // This test ensures that our bounding box logic works when the bounding box
  // of an earlier group is nested inside bounding box of a later group.
  // In that case, we should still layout in a way which avoids overlaps.

  // Group 1 exists just to create push group 2 down one row.
  auto a = Insert(&slice_table, 0 /*ts*/, 1 /*dur*/, 1 /*track_id*/, name,
                  std::nullopt);
  base::ignore_result(a);

  // Group 2 has a depth of 2 so it theoretically "nests" inside a group of
  // depth 4.
  auto c = Insert(&slice_table, 0 /*ts*/, 10 /*dur*/, 2 /*track_id*/, name,
                  std::nullopt);
  auto d = Insert(&slice_table, 0 /*ts*/, 9 /*dur*/, 2 /*track_id*/, name, c);
  base::ignore_result(d);

  // Group 3 has a depth of 4 so it could cause group 2 to "nest" if our
  // layout algorithm did not work correctly.
  auto p = Insert(&slice_table, 3 /*ts*/, 4 /*dur*/, 3 /*track_id*/, name,
                  std::nullopt);
  auto q = Insert(&slice_table, 3 /*ts*/, 3 /*dur*/, 3 /*track_id*/, name, p);
  auto r = Insert(&slice_table, 3 /*ts*/, 2 /*dur*/, 3 /*track_id*/, name, q);
  auto s = Insert(&slice_table, 3 /*ts*/, 1 /*dur*/, 3 /*track_id*/, name, r);
  base::ignore_result(s);

  ExperimentalSliceLayout gen(&pool, &slice_table);

  std::unique_ptr<Table> table;
  auto status = gen.ComputeTable(
      {Constraint{kColumn, FilterOp::kEq, SqlValue::String("1,2,3")}}, {},
      BitVector(), table);
  EXPECT_TRUE(status.ok());
  ExpectOutput(*table, R"(
#
##########
#########
   ####
   ###
   ##
   #
)");
}

TEST(ExperimentalSliceLayoutTest, FilterOutTracks) {
  StringPool pool;
  tables::SliceTable slice_table(&pool);
  StringId name1 = pool.InternString("Slice1");
  StringId name2 = pool.InternString("Slice2");
  StringId name3 = pool.InternString("Slice3");
  StringId name4 = pool.InternString("Slice4");
  StringId name5 = pool.InternString("Slice5");

  auto a = Insert(&slice_table, 0 /*ts*/, 4 /*dur*/, 1 /*track_id*/, name1,
                  std::nullopt);
  auto b = Insert(&slice_table, 0 /*ts*/, 2 /*dur*/, 1 /*track_id*/, name2, a);
  auto p = Insert(&slice_table, 3 /*ts*/, 4 /*dur*/, 2 /*track_id*/, name3,
                  std::nullopt);
  auto q = Insert(&slice_table, 3 /*ts*/, 2 /*dur*/, 2 /*track_id*/, name4, p);
  // This slice should be ignored as it's not in the filter below:
  Insert(&slice_table, 0 /*ts*/, 9 /*dur*/, 3 /*track_id*/, name5,
         std::nullopt);
  base::ignore_result(b);
  base::ignore_result(q);

  ExperimentalSliceLayout gen(&pool, &slice_table);
  std::unique_ptr<Table> table;
  auto status = gen.ComputeTable(
      {Constraint{kColumn, FilterOp::kEq, SqlValue::String("1,2")}}, {},
      BitVector(), table);
  EXPECT_TRUE(status.ok());
  ExpectOutput(*table, R"(
####
##
   ####
   ##
)");
}

}  // namespace
}  // namespace trace_processor
}  // namespace perfetto
