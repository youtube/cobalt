// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/storage/filter_annotation_table.h"

#include <stddef.h>

#include <optional>
#include <string_view>
#include <vector>

#include "base/check.h"
#include "base/notimplemented.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace multistep_filter {

namespace {

namespace filter_annotations {
constexpr char kTableName[] = "filter_annotations";
constexpr char kId[] = "id";
constexpr char kTaskType[] = "task_type";
constexpr char kSourceHost[] = "source_host";
constexpr char kCreationTimestamp[] = "creation_timestamp";
}  // namespace filter_annotations

namespace filter_annotation_attributes {
constexpr char kTableName[] = "filter_annotation_attributes";
constexpr char kAnnotationId[] = "annotation_id";
constexpr char kKey[] = "key";
constexpr char kValue[] = "value";
}  // namespace filter_annotation_attributes

std::string GetDeleteAttributesSql(std::string_view where_clause) {
  return base::StrCat({"DELETE FROM ", filter_annotation_attributes::kTableName,
                       " WHERE ", filter_annotation_attributes::kAnnotationId,
                       " IN (SELECT ", filter_annotations::kId, " FROM ",
                       filter_annotations::kTableName, " WHERE ", where_clause,
                       ")"});
}

}  // namespace

FilterAnnotationTable::FilterAnnotationTable() = default;
FilterAnnotationTable::~FilterAnnotationTable() = default;

bool FilterAnnotationTable::Init(sql::Database* db) {
  CHECK(db);
  db_ = db;

  // TODO(crbug.com/491051120): Replace this manual schema creation with
  // shared SQL utilities.
  auto create_filter_annotations_table = [&]() -> bool {
    if (db_->DoesTableExist(filter_annotations::kTableName)) {
      return true;
    }
    const std::string kCreateFilterAnnotationsTableSql = base::StrCat(
        {"CREATE TABLE ", filter_annotations::kTableName, "(",
         filter_annotations::kId, " TEXT PRIMARY KEY NOT NULL,",
         filter_annotations::kTaskType, " TEXT NOT NULL,",
         filter_annotations::kSourceHost, " TEXT NOT NULL,",
         filter_annotations::kCreationTimestamp, " INTEGER NOT NULL)"});
    return db_->Execute(kCreateFilterAnnotationsTableSql);
  };

  auto create_filter_annotation_attributes_table = [&]() -> bool {
    if (db_->DoesTableExist(filter_annotation_attributes::kTableName)) {
      return true;
    }
    const std::string kCreateFilterAnnotationAttributesTableSql = base::StrCat(
        {"CREATE TABLE ", filter_annotation_attributes::kTableName, "(",
         filter_annotation_attributes::kAnnotationId, " TEXT NOT NULL,",
         filter_annotation_attributes::kKey, " TEXT NOT NULL,",
         filter_annotation_attributes::kValue, " TEXT NOT NULL)"});
    return db_->Execute(kCreateFilterAnnotationAttributesTableSql);
  };

  // Creates indexes to optimize retrieval and deletion performance.
  auto create_indexes = [&]() -> bool {
    const std::string kCreateFilterAnnotationsIndexSql = base::StrCat(
        {"CREATE INDEX IF NOT EXISTS "
         "filter_annotations_task_type_timestamp_idx "
         "ON ",
         filter_annotations::kTableName, "(", filter_annotations::kTaskType,
         ", ", filter_annotations::kCreationTimestamp, ")"});
    const std::string kCreateAttributesIndexSql = base::StrCat(
        {"CREATE INDEX IF NOT EXISTS filter_annotation_attributes_id_idx ON ",
         filter_annotation_attributes::kTableName, "(",
         filter_annotation_attributes::kAnnotationId, ")"});
    const std::string kCreateAnnotationsCompositeIndexSql = base::StrCat(
        {"CREATE INDEX IF NOT EXISTS filter_annotations_task_host_idx ON ",
         filter_annotations::kTableName, "(", filter_annotations::kTaskType,
         ", ", filter_annotations::kSourceHost, ")"});
    const std::string kCreateAnnotationsHostTimestampIndexSql = base::StrCat(
        {"CREATE INDEX IF NOT EXISTS "
         "filter_annotations_host_timestamp_idx "
         "ON ",
         filter_annotations::kTableName, "(", filter_annotations::kSourceHost,
         ", ", filter_annotations::kCreationTimestamp, ")"});
    return db_->Execute(kCreateFilterAnnotationsIndexSql) &&
           db_->Execute(kCreateAttributesIndexSql) &&
           db_->Execute(kCreateAnnotationsCompositeIndexSql) &&
           db_->Execute(kCreateAnnotationsHostTimestampIndexSql);
  };

  return create_filter_annotations_table() &&
         create_filter_annotation_attributes_table() && create_indexes();
}

bool FilterAnnotationTable::StoreAnnotation(
    const FilterAnnotation& annotation) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin()) {
    return false;
  }

  // Delete all existing annotations for the same task type and source host to
  // ensure we only store the latest one.
  sql::Statement delete_attributes(db_->GetCachedStatement(
      SQL_FROM_HERE, GetDeleteAttributesSql(base::StrCat(
                         {filter_annotations::kTaskType, " = ? AND ",
                          filter_annotations::kSourceHost, " = ?"}))));
  delete_attributes.BindString(0, annotation.task_type);
  delete_attributes.BindString(1, annotation.source_host);
  if (!delete_attributes.Run()) {
    return false;
  }

  sql::Statement delete_annotations(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StrCat({"DELETE FROM ", filter_annotations::kTableName, " WHERE ",
                    filter_annotations::kTaskType, " = ? AND ",
                    filter_annotations::kSourceHost, " = ?"})));
  delete_annotations.BindString(0, annotation.task_type);
  delete_annotations.BindString(1, annotation.source_host);
  if (!delete_annotations.Run()) {
    return false;
  }

  sql::Statement insert_annotation(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StrCat(
          {"INSERT INTO ", filter_annotations::kTableName, "(",
           filter_annotations::kId, ", ", filter_annotations::kTaskType, ", ",
           filter_annotations::kSourceHost, ", ",
           filter_annotations::kCreationTimestamp, ") VALUES(?,?,?,?)"})));
  insert_annotation.BindString(0, annotation.id.AsLowercaseString());
  insert_annotation.BindString(1, annotation.task_type);
  insert_annotation.BindString(2, annotation.source_host);
  insert_annotation.BindTime(3, annotation.creation_timestamp);

  if (!insert_annotation.Run()) {
    return false;
  }

  for (const FilterAttribute& attribute : annotation.attributes) {
    sql::Statement insert_attribute(db_->GetCachedStatement(
        SQL_FROM_HERE,
        base::StrCat({"INSERT INTO ", filter_annotation_attributes::kTableName,
                      "(", filter_annotation_attributes::kAnnotationId, ", ",
                      filter_annotation_attributes::kKey, ", ",
                      filter_annotation_attributes::kValue,
                      ") VALUES(?,?,?)"})));
    insert_attribute.BindString(0, annotation.id.AsLowercaseString());
    insert_attribute.BindString(1, attribute.key);
    insert_attribute.BindString(2, attribute.value);
    if (!insert_attribute.Run()) {
      return false;
    }
  }

  return transaction.Commit();
}

std::vector<FilterAnnotation>
FilterAnnotationTable::GetAnnotationsForTasksSortedByCreationTimestamp(
    base::span<const std::string> task_types,
    size_t max_count,
    base::Time min_creation_time) {
  if (task_types.empty()) {
    return {};
  }

  std::vector<FilterAnnotation> annotations;

  std::string task_types_str =
      base::JoinString(std::vector<std::string>(task_types.size(), "?"), ", ");
  std::string query =
      base::StrCat({"SELECT ", filter_annotations::kId, ", ",
                    filter_annotations::kTaskType, ", ",
                    filter_annotations::kSourceHost, ", ",
                    filter_annotations::kCreationTimestamp, " FROM ",
                    filter_annotations::kTableName, " WHERE ",
                    filter_annotations::kTaskType, " IN (", task_types_str,
                    ") AND ", filter_annotations::kCreationTimestamp,
                    " >= ? ORDER BY ", filter_annotations::kCreationTimestamp,
                    " DESC LIMIT ?"});
  sql::Statement select_annotations(db_->GetUniqueStatement(query));

  int param_index = 0;
  for (const std::string& task_type : task_types) {
    select_annotations.BindString(param_index++, task_type);
  }
  select_annotations.BindTime(param_index++, min_creation_time);
  select_annotations.BindInt64(param_index++, max_count);

  while (select_annotations.Step()) {
    std::string id_str = select_annotations.ColumnString(0);
    base::Uuid id = base::Uuid::ParseLowercase(id_str);
    if (!id.is_valid()) {
      continue;
    }

    std::string retrieved_task_type = select_annotations.ColumnString(1);
    std::string source_host = select_annotations.ColumnString(2);
    base::Time creation_timestamp = select_annotations.ColumnTime(3);

    sql::Statement select_attributes(db_->GetCachedStatement(
        SQL_FROM_HERE,
        base::StrCat({"SELECT ", filter_annotation_attributes::kKey, ", ",
                      filter_annotation_attributes::kValue, " FROM ",
                      filter_annotation_attributes::kTableName, " WHERE ",
                      filter_annotation_attributes::kAnnotationId, " = ?"})));
    select_attributes.BindString(0, id_str);

    std::vector<FilterAttribute> attributes;
    while (select_attributes.Step()) {
      attributes.emplace_back(select_attributes.ColumnString(0),
                              select_attributes.ColumnString(1));
    }

    annotations.emplace_back(id, retrieved_task_type,
                             source_host, creation_timestamp,
                             std::move(attributes));
  }

  if (!select_annotations.Succeeded()) {
    return {};
  }

  return annotations;
}

std::optional<int64_t> FilterAnnotationTable::DeleteAnnotationsForTask(
    std::string_view task_type) {
  sql::Transaction transaction(db_);
  if (!transaction.Begin()) {
    return std::nullopt;
  }

  // Delete from attributes table first to avoid orphaning
  sql::Statement delete_attributes(db_->GetCachedStatement(
      SQL_FROM_HERE, GetDeleteAttributesSql(base::StrCat(
                         {filter_annotations::kTaskType, " = ?"}))));
  delete_attributes.BindString(0, task_type);
  if (!delete_attributes.Run()) {
    return std::nullopt;
  }

  // Delete from annotations table
  sql::Statement delete_annotations(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StrCat({"DELETE FROM ", filter_annotations::kTableName, " WHERE ",
                    filter_annotations::kTaskType, " = ?"})));
  delete_annotations.BindString(0, task_type);
  if (!delete_annotations.Run()) {
    return std::nullopt;
  }

  int64_t deleted_count = db_->GetLastChangeCount();
  if (!transaction.Commit()) {
    return std::nullopt;
  }

  return deleted_count;
}

std::optional<int64_t> FilterAnnotationTable::DeleteAnnotationsForHosts(
    const std::vector<std::string>& hosts,
    base::Time delete_begin,
    base::Time delete_end) {
  if (hosts.empty()) {
    return DeleteAnnotationsForTimeRange(delete_begin, delete_end);
  }

  sql::Transaction transaction(db_);
  if (!transaction.Begin()) {
    return std::nullopt;
  }

  int64_t total_deleted = 0;
  for (const std::string& host : hosts) {
    std::optional<int64_t> deleted =
        DeleteAnnotationsForHost(host, delete_begin, delete_end);
    if (!deleted.has_value()) {
      return std::nullopt;
    }
    total_deleted += deleted.value();
  }

  if (!transaction.Commit()) {
    return std::nullopt;
  }

  return total_deleted;
}

std::optional<int64_t> FilterAnnotationTable::DeleteAnnotationsForTimeRange(
    base::Time begin,
    base::Time end) {
  sql::Statement delete_attributes(db_->GetCachedStatement(
      SQL_FROM_HERE, GetDeleteAttributesSql(base::StrCat(
                         {filter_annotations::kCreationTimestamp, " >= ? AND ",
                          filter_annotations::kCreationTimestamp, " < ?"}))));
  delete_attributes.BindTime(0, begin);
  delete_attributes.BindTime(1, end);
  if (!delete_attributes.Run()) {
    return std::nullopt;
  }

  sql::Statement delete_annotations(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StrCat({"DELETE FROM ", filter_annotations::kTableName, " WHERE ",
                    filter_annotations::kCreationTimestamp, " >= ? AND ",
                    filter_annotations::kCreationTimestamp, " < ?"})));
  delete_annotations.BindTime(0, begin);
  delete_annotations.BindTime(1, end);
  if (!delete_annotations.Run()) {
    return std::nullopt;
  }
  return db_->GetLastChangeCount();
}

std::optional<int64_t> FilterAnnotationTable::DeleteAnnotationsForHost(
    std::string_view host,
    base::Time begin,
    base::Time end) {
  sql::Statement delete_attributes(db_->GetCachedStatement(
      SQL_FROM_HERE, GetDeleteAttributesSql(base::StrCat(
                         {filter_annotations::kSourceHost, " = ? AND ",
                          filter_annotations::kCreationTimestamp, " >= ? AND ",
                          filter_annotations::kCreationTimestamp, " < ?"}))));
  delete_attributes.BindString(0, host);
  delete_attributes.BindTime(1, begin);
  delete_attributes.BindTime(2, end);
  if (!delete_attributes.Run()) {
    return std::nullopt;
  }

  sql::Statement delete_annotations(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StrCat({"DELETE FROM ", filter_annotations::kTableName, " WHERE ",
                    filter_annotations::kSourceHost, " = ? AND ",
                    filter_annotations::kCreationTimestamp, " >= ? AND ",
                    filter_annotations::kCreationTimestamp, " < ?"})));
  delete_annotations.BindString(0, host);
  delete_annotations.BindTime(1, begin);
  delete_annotations.BindTime(2, end);
  if (!delete_annotations.Run()) {
    return std::nullopt;
  }
  return db_->GetLastChangeCount();
}

void FilterAnnotationTable::Shutdown() {
  db_ = nullptr;
}

}  // namespace multistep_filter
