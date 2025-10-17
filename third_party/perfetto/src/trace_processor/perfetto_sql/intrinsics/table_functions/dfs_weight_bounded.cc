/*
 * Copyright (C) 2024 The Android Open Source Project
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

#include "src/trace_processor/perfetto_sql/intrinsics/table_functions/dfs_weight_bounded.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "perfetto/base/logging.h"
#include "perfetto/base/status.h"
#include "perfetto/ext/base/status_or.h"
#include "perfetto/protozero/proto_decoder.h"
#include "perfetto/trace_processor/basic_types.h"
#include "protos/perfetto/trace_processor/metrics_impl.pbzero.h"
#include "src/trace_processor/containers/string_pool.h"
#include "src/trace_processor/db/column.h"
#include "src/trace_processor/db/table.h"
#include "src/trace_processor/perfetto_sql/intrinsics/table_functions/tables_py.h"
#include "src/trace_processor/util/status_macros.h"

namespace perfetto::trace_processor {
namespace tables {
DfsWeightBoundedTable::~DfsWeightBoundedTable() = default;
}  // namespace tables

namespace {
struct Edge {
  uint32_t id;
  uint32_t weight;
};
using Destinations = std::vector<Edge>;

base::StatusOr<std::vector<Destinations>> ParseSourceToDestionationsMap(
    protos::pbzero::RepeatedBuilderResult::Decoder& source,
    protos::pbzero::RepeatedBuilderResult::Decoder& dest,
    protos::pbzero::RepeatedBuilderResult::Decoder& weight) {
  std::vector<Destinations> source_to_destinations_map;
  bool parse_error = false;
  auto source_node_ids = source.int_values(&parse_error);
  auto dest_node_ids = dest.int_values(&parse_error);
  auto edge_weights = weight.int_values(&parse_error);

  for (; source_node_ids && dest_node_ids && edge_weights;
       ++source_node_ids, ++dest_node_ids, ++edge_weights) {
    source_to_destinations_map.resize(
        std::max(source_to_destinations_map.size(),
                 std::max(static_cast<size_t>(*source_node_ids + 1),
                          static_cast<size_t>(*dest_node_ids + 1))));
    source_to_destinations_map[static_cast<uint32_t>(*source_node_ids)]
        .push_back(Edge{static_cast<uint32_t>(*dest_node_ids),
                        static_cast<uint32_t>(*edge_weights)});
  }
  if (parse_error) {
    return base::ErrStatus("Failed while parsing source or dest ids");
  }
  if (static_cast<bool>(source_node_ids) != static_cast<bool>(dest_node_ids)) {
    return base::ErrStatus(
        "dfs_weight_bounded: length of source and destination columns is not "
        "the same");
  }
  return source_to_destinations_map;
}

base::StatusOr<std::vector<Edge>> ParseRootToMaxWeightMap(
    protos::pbzero::RepeatedBuilderResult::Decoder& start,
    protos::pbzero::RepeatedBuilderResult::Decoder& end) {
  std::vector<Edge> roots;
  bool parse_error = false;
  auto root_node_ids = start.int_values(&parse_error);
  auto target_weights = end.int_values(&parse_error);

  for (; root_node_ids && target_weights; ++root_node_ids, ++target_weights) {
    roots.push_back(Edge{static_cast<uint32_t>(*root_node_ids),
                         static_cast<uint32_t>(*target_weights)});
  }

  if (parse_error) {
    return base::ErrStatus(
        "Failed while parsing root_node_ids or root_target_weights");
  }
  if (static_cast<bool>(root_node_ids) != static_cast<bool>(target_weights)) {
    return base::ErrStatus(
        "dfs_weight_bounded: length of root_node_ids and root_target_weights "
        "columns is not the same");
  }
  return roots;
}

void DfsWeightBoundedImpl(
    tables::DfsWeightBoundedTable* table,
    const std::vector<Destinations>& source_to_destinations_map,
    const std::vector<Edge>& roots,
    const bool is_target_weight_floor) {
  struct StackState {
    uint32_t id;
    uint32_t weight;
    std::optional<uint32_t> parent_id;
  };

  std::vector<uint8_t> seen_node_ids(source_to_destinations_map.size());
  std::vector<StackState> stack;

  for (const auto& root : roots) {
    stack.clear();
    stack.push_back({root.id, 0, std::nullopt});
    std::fill(seen_node_ids.begin(), seen_node_ids.end(), 0);

    for (uint32_t total_weight = 0; !stack.empty();) {
      StackState stack_state = stack.back();
      stack.pop_back();

      if (seen_node_ids[stack_state.id]) {
        continue;
      }
      seen_node_ids[stack_state.id] = true;
      total_weight += stack_state.weight;

      if (!is_target_weight_floor && total_weight > root.weight) {
        // If target weight is a ceiling weight then we don't want to include
        // the last node that crosses the threshold.
        break;
      }

      tables::DfsWeightBoundedTable::Row row;
      row.root_node_id = root.id;
      row.node_id = stack_state.id;
      row.parent_node_id = stack_state.parent_id;
      table->Insert(row);

      if (total_weight > root.weight) {
        // If the target weight is a floor weight, we add the last node that
        // crossed the threshold before exiting the search.
        break;
      }

      PERFETTO_DCHECK(stack_state.id < source_to_destinations_map.size());

      const auto& children = source_to_destinations_map[stack_state.id];
      for (auto it = children.rbegin(); it != children.rend(); ++it) {
        stack.emplace_back(StackState{(*it).id, (*it).weight, stack_state.id});
      }
    }
  }
}
}  // namespace

DfsWeightBounded::DfsWeightBounded(StringPool* pool) : pool_(pool) {}
DfsWeightBounded::~DfsWeightBounded() = default;

Table::Schema DfsWeightBounded::CreateSchema() {
  return tables::DfsWeightBoundedTable::ComputeStaticSchema();
}

std::string DfsWeightBounded::TableName() {
  return tables::DfsWeightBoundedTable::Name();
}

uint32_t DfsWeightBounded::EstimateRowCount() {
  // TODO(lalitm): improve this estimate.
  return 1024;
}

base::StatusOr<std::unique_ptr<Table>> DfsWeightBounded::ComputeTable(
    const std::vector<SqlValue>& arguments) {
  PERFETTO_CHECK(arguments.size() == 6);

  const SqlValue& raw_source_ids = arguments[0];
  const SqlValue& raw_dest_ids = arguments[1];
  const SqlValue& raw_edge_weights = arguments[2];
  const SqlValue& raw_root_ids = arguments[3];
  const SqlValue& raw_root_target_weights = arguments[4];
  const SqlValue& raw_is_target_weight_floor = arguments[5];

  if (raw_source_ids.is_null() && raw_dest_ids.is_null() &&
      raw_edge_weights.is_null()) {
    return std::unique_ptr<Table>(
        std::make_unique<tables::DfsWeightBoundedTable>(pool_));
  }

  if (raw_root_ids.is_null() && raw_root_target_weights.is_null()) {
    return std::unique_ptr<Table>(
        std::make_unique<tables::DfsWeightBoundedTable>(pool_));
  }

  if (raw_source_ids.is_null() || raw_dest_ids.is_null() ||
      raw_edge_weights.is_null() || raw_root_ids.is_null() ||
      raw_root_target_weights.is_null()) {
    return base::ErrStatus(
        "dfs_weight_bounded: either all arguments should be null or none "
        "should be");
  }
  if (raw_source_ids.type != SqlValue::kBytes) {
    return base::ErrStatus(
        "dfs_weight_bounded: source_node_ids should be a repeated field");
  }
  if (raw_dest_ids.type != SqlValue::kBytes) {
    return base::ErrStatus(
        "dfs_weight_bounded: dest_node_ids should be a repeated field");
  }
  if (raw_edge_weights.type != SqlValue::kBytes) {
    return base::ErrStatus(
        "dfs_weight_bounded: edge_weights should be a repeated field");
  }
  if (raw_root_ids.type != SqlValue::kBytes) {
    return base::ErrStatus(
        "dfs_weight_bounded: root_ids should be a repeated field");
  }
  if (raw_root_target_weights.type != SqlValue::kBytes) {
    return base::ErrStatus(
        "dfs_weight_bounded: root_target_weights should be a repeated field");
  }

  protos::pbzero::ProtoBuilderResult::Decoder proto_source_ids(
      static_cast<const uint8_t*>(raw_source_ids.AsBytes()),
      raw_source_ids.bytes_count);
  if (!proto_source_ids.is_repeated()) {
    return base::ErrStatus(
        "dfs_weight_bounded: source_node_ids is not generated by RepeatedField "
        "function");
  }
  protos::pbzero::RepeatedBuilderResult::Decoder source_ids(
      proto_source_ids.repeated());

  protos::pbzero::ProtoBuilderResult::Decoder proto_dest_ids(
      static_cast<const uint8_t*>(raw_dest_ids.AsBytes()),
      raw_dest_ids.bytes_count);
  if (!proto_dest_ids.is_repeated()) {
    return base::ErrStatus(
        "dfs_weight_bounded: dest_node_ids is not generated by RepeatedField "
        "function");
  }
  protos::pbzero::RepeatedBuilderResult::Decoder dest_ids(
      proto_dest_ids.repeated());

  protos::pbzero::ProtoBuilderResult::Decoder proto_edge_weights(
      static_cast<const uint8_t*>(raw_edge_weights.AsBytes()),
      raw_edge_weights.bytes_count);
  if (!proto_edge_weights.is_repeated()) {
    return base::ErrStatus(
        "dfs_weight_bounded: edge_weights is not generated by RepeatedField "
        "function");
  }
  protos::pbzero::RepeatedBuilderResult::Decoder edge_weights(
      proto_edge_weights.repeated());

  protos::pbzero::ProtoBuilderResult::Decoder proto_root_ids(
      static_cast<const uint8_t*>(raw_root_ids.AsBytes()),
      raw_root_ids.bytes_count);
  if (!proto_root_ids.is_repeated()) {
    return base::ErrStatus(
        "dfs_weight_bounded: root_ids is not generated by RepeatedField "
        "function");
  }
  protos::pbzero::RepeatedBuilderResult::Decoder root_ids(
      proto_root_ids.repeated());

  protos::pbzero::ProtoBuilderResult::Decoder proto_root_target_weights(
      static_cast<const uint8_t*>(raw_root_target_weights.AsBytes()),
      raw_root_target_weights.bytes_count);
  if (!proto_root_target_weights.is_repeated()) {
    return base::ErrStatus(
        "dfs_weight_bounded: root_target_weights is not generated by "
        "RepeatedField function");
  }
  protos::pbzero::RepeatedBuilderResult::Decoder root_target_weights(
      proto_root_target_weights.repeated());

  bool is_target_weight_floor =
      static_cast<bool>(raw_is_target_weight_floor.AsLong());
  ASSIGN_OR_RETURN(auto map, ParseSourceToDestionationsMap(source_ids, dest_ids,
                                                           edge_weights));

  ASSIGN_OR_RETURN(auto roots,
                   ParseRootToMaxWeightMap(root_ids, root_target_weights));

  auto table = std::make_unique<tables::DfsWeightBoundedTable>(pool_);
  DfsWeightBoundedImpl(table.get(), map, roots, is_target_weight_floor);
  return std::unique_ptr<Table>(std::move(table));
}

}  // namespace perfetto::trace_processor
