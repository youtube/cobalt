// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/test/test_helpers.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <memory>
#include <string>
#include <tuple>

#include "base/big_endian.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql::test {

namespace {

size_t CountSQLItemsOfType(sql::Database* db, const char* type) {
  static const char kTypeSQL[] =
      "SELECT COUNT(*) FROM sqlite_schema WHERE type = ?";
  sql::Statement s(db->GetUniqueStatement(kTypeSQL));
  s.BindCString(0, type);
  EXPECT_TRUE(s.Step());
  return s.ColumnInt(0);
}

// Read the number of the root page of a B-tree (index/table).
//
// Returns a 0-indexed page number, not the raw SQLite page number.
absl::optional<int> GetRootPage(sql::Database& db,
                                base::StringPiece tree_name) {
  sql::Statement select(
      db.GetUniqueStatement("SELECT rootpage FROM sqlite_schema WHERE name=?"));
  select.BindString(0, tree_name);
  if (!select.Step())
    return absl::nullopt;

  int sqlite_page_number = select.ColumnInt(0);
  if (!sqlite_page_number)
    return absl::nullopt;

  return sqlite_page_number - 1;
}

[[nodiscard]] bool IsWalDatabase(const base::FilePath& db_path) {
  // See http://www.sqlite.org/fileformat2.html#database_header
  constexpr size_t kHeaderSize = 100;
  constexpr int64_t kHeaderOffset = 0;
  uint8_t header[kHeaderSize];
  base::File file(db_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    return false;
  if (!file.ReadAndCheck(kHeaderOffset, header))
    return false;

  // See https://www.sqlite.org/fileformat2.html#file_format_version_numbers
  constexpr int kWriteVersionHeaderOffset = 18;
  constexpr int kReadVersionHeaderOffset = 19;
  // If the read version is unsupported, we can't rely on our ability to
  // interpret anything else in the header.
  DCHECK_LE(int{header[kReadVersionHeaderOffset]}, 2)
      << "Unsupported SQLite file format";
  return header[kWriteVersionHeaderOffset] == 2;
}

[[nodiscard]] bool CorruptSizeInHeaderMemory(uint8_t* header, int64_t db_size) {
  // See https://www.sqlite.org/fileformat2.html#page_size
  constexpr size_t kPageSizeOffset = 16;
  constexpr uint16_t kMinPageSize = 512;
  uint16_t raw_page_size;
  base::ReadBigEndian(header + kPageSizeOffset, &raw_page_size);
  const int page_size = (raw_page_size == 1) ? 65536 : raw_page_size;
  // Sanity-check that the page size is valid.
  if (page_size < kMinPageSize || (page_size & (page_size - 1)) != 0)
    return false;

  // Set the page count to exceed the file size.
  // See https://www.sqlite.org/fileformat2.html#in_header_database_size
  constexpr size_t kPageCountOffset = 28;
  const int64_t page_count = (db_size + page_size * 2 - 1) / page_size;
  if (page_count > std::numeric_limits<uint32_t>::max())
    return false;
  base::WriteBigEndian(reinterpret_cast<char*>(header + kPageCountOffset),
                       static_cast<uint32_t>(page_count));

  // Update change count so outstanding readers know the info changed.
  // See https://www.sqlite.org/fileformat2.html#file_change_counter
  // and
  // https://www.sqlite.org/fileformat2.html#write_library_version_number_and_version_valid_for_number
  constexpr size_t kFileChangeCountOffset = 24;
  constexpr size_t kVersionValidForOffset = 92;
  uint32_t old_change_count;
  base::ReadBigEndian(header + kFileChangeCountOffset, &old_change_count);
  const uint32_t new_change_count = old_change_count + 1;
  base::WriteBigEndian(reinterpret_cast<char*>(header + kFileChangeCountOffset),
                       new_change_count);
  base::WriteBigEndian(reinterpret_cast<char*>(header + kVersionValidForOffset),
                       new_change_count);
  return true;
}

}  // namespace

absl::optional<int> ReadDatabasePageSize(const base::FilePath& db_path) {
  // See https://www.sqlite.org/fileformat2.html#page_size
  constexpr size_t kPageSizeOffset = 16;
  uint8_t raw_page_size_bytes[2];
  base::File file(db_path, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!file.IsValid())
    return absl::nullopt;
  if (!file.ReadAndCheck(kPageSizeOffset, raw_page_size_bytes))
    return absl::nullopt;

  uint16_t raw_page_size;
  base::ReadBigEndian(raw_page_size_bytes, &raw_page_size);
  // The SQLite database format initially allocated a 16 bits for storing the
  // page size. This worked out until SQLite wanted to support 64kb pages,
  // because 65536 (64kb) doesn't fit in a 16-bit unsigned integer.
  //
  // Currently, the page_size field value of 1 is a special case for 64kb pages.
  // The documentation hints at the path for future expansion -- the page_size
  // field may become a litte-endian number that indicates the database page
  // size divided by 256. This happens to work out because the smallest
  // supported page size is 512.
  const int page_size = (raw_page_size == 1) ? 65536 : raw_page_size;
  // Sanity-check that the page size is valid.
  constexpr uint16_t kMinPageSize = 512;
  if (page_size < kMinPageSize || (page_size & (page_size - 1)) != 0)
    return absl::nullopt;

  return page_size;
}

bool CorruptSizeInHeader(const base::FilePath& db_path) {
  if (IsWalDatabase(db_path)) {
    // Checkpoint the WAL file in Truncate mode before corrupting to ensure that
    // any future transaction always touches the DB file and not just the WAL
    // file.
    base::ScopedAllowBlockingForTesting allow_blocking;
    // TODO: This function doesn't reliably work if connections to the DB are
    // still open. Change any uses to ensure that we close all database
    // connections before calling this function.
    sql::Database db({.exclusive_locking = false, .wal_mode = true});
    if (!db.Open(db_path))
      return false;
    int wal_log_size = 0;
    int checkpointed_frame_count = 0;
    int truncate_result = sqlite3_wal_checkpoint_v2(
        db.db(InternalApiToken()), /*zDb=*/nullptr, SQLITE_CHECKPOINT_TRUNCATE,
        &wal_log_size, &checkpointed_frame_count);
    // A successful checkpoint in truncate mode sets these to zero.
    DCHECK(wal_log_size == 0);
    DCHECK(checkpointed_frame_count == 0);
    if (truncate_result != SQLITE_OK)
      return false;
    db.Close();
  }

  base::File file(db_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                               base::File::FLAG_WRITE);
  if (!file.IsValid())
    return false;

  int64_t db_size = file.GetLength();
  if (db_size < 0)
    return false;

  // Read the entire database header, corrupt it, and write it back.
  // See http://www.sqlite.org/fileformat2.html#database_header
  constexpr size_t kHeaderSize = 100;
  constexpr int64_t kHeaderOffset = 0;
  uint8_t header[kHeaderSize];
  if (!file.ReadAndCheck(kHeaderOffset, header))
    return false;
  if (!CorruptSizeInHeaderMemory(header, db_size))
    return false;
  return file.WriteAndCheck(kHeaderOffset, header);
}

bool CorruptSizeInHeaderWithLock(const base::FilePath& db_path) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  sql::Database db;
  if (!db.Open(db_path))
    return false;

  // Prevent anyone else from using the database.  The transaction is
  // rolled back when |db| is destroyed.
  if (!db.Execute("BEGIN EXCLUSIVE"))
    return false;

  return CorruptSizeInHeader(db_path);
}

bool CorruptIndexRootPage(const base::FilePath& db_path,
                          base::StringPiece index_name) {
  absl::optional<int> page_size = ReadDatabasePageSize(db_path);
  if (!page_size.has_value())
    return false;

  sql::Database db;
  if (!db.Open(db_path))
    return false;

  absl::optional<int> page_number = GetRootPage(db, index_name);
  db.Close();
  if (!page_number.has_value())
    return false;

  std::vector<uint8_t> page_buffer(page_size.value());
  const int64_t page_offset = int64_t{page_number.value()} * page_size.value();

  base::File file(db_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                               base::File::FLAG_WRITE);
  if (!file.IsValid())
    return false;
  return file.WriteAndCheck(page_offset, page_buffer);
}

size_t CountSQLTables(sql::Database* db) {
  return CountSQLItemsOfType(db, "table");
}

size_t CountSQLIndices(sql::Database* db) {
  return CountSQLItemsOfType(db, "index");
}

size_t CountTableColumns(sql::Database* db, const char* table) {
  // TODO(shess): sql::Database::QuoteForSQL() would make sense.
  std::string quoted_table;
  {
    static const char kQuoteSQL[] = "SELECT quote(?)";
    sql::Statement s(db->GetUniqueStatement(kQuoteSQL));
    s.BindCString(0, table);
    EXPECT_TRUE(s.Step());
    quoted_table = s.ColumnString(0);
  }

  std::string sql = "PRAGMA table_info(" + quoted_table + ")";
  sql::Statement s(db->GetUniqueStatement(sql.c_str()));
  size_t rows = 0;
  while (s.Step()) {
    ++rows;
  }
  EXPECT_TRUE(s.Succeeded());
  return rows;
}

bool CountTableRows(sql::Database* db, const char* table, size_t* count) {
  // TODO(shess): Table should probably be quoted with [] or "".  See
  // http://www.sqlite.org/lang_keywords.html .  Meanwhile, odd names
  // will throw an error.
  std::string sql = "SELECT COUNT(*) FROM ";
  sql += table;
  sql::Statement s(db->GetUniqueStatement(sql.c_str()));
  if (!s.Step())
    return false;

  *count = static_cast<size_t>(s.ColumnInt64(0));
  return true;
}

bool CreateDatabaseFromSQL(const base::FilePath& db_path,
                           const base::FilePath& sql_path) {
  if (base::PathExists(db_path) || !base::PathExists(sql_path))
    return false;

  std::string sql;
  if (!base::ReadFileToString(sql_path, &sql))
    return false;

  sql::Database db;
  if (!db.Open(db_path))
    return false;

  // TODO(shess): Android defaults to auto_vacuum mode.
  // Unfortunately, this makes certain kinds of tests which manipulate
  // the raw database hard/impossible to write.
  // http://crbug.com/307303 is for exploring this test issue.
  std::ignore = db.Execute("PRAGMA auto_vacuum = 0");

  return db.Execute(sql.c_str());
}

std::string IntegrityCheck(sql::Database& db) {
  std::vector<std::string> messages;
  EXPECT_TRUE(db.FullIntegrityCheck(&messages));

  return base::JoinString(messages, "\n");
}

std::string ExecuteWithResult(sql::Database* db, const char* sql) {
  sql::Statement s(db->GetUniqueStatement(sql));
  return s.Step() ? s.ColumnString(0) : std::string();
}

std::string ExecuteWithResults(sql::Database* db,
                               const char* sql,
                               const char* column_sep,
                               const char* row_sep) {
  sql::Statement s(db->GetUniqueStatement(sql));
  std::string ret;
  while (s.Step()) {
    if (!ret.empty())
      ret += row_sep;
    for (int i = 0; i < s.ColumnCount(); ++i) {
      if (i > 0)
        ret += column_sep;
      ret += s.ColumnString(i);
    }
  }
  return ret;
}

int GetPageCount(sql::Database* db) {
  sql::Statement statement(db->GetUniqueStatement("PRAGMA page_count"));
  CHECK(statement.Step());
  return statement.ColumnInt(0);
}

// static
ColumnInfo ColumnInfo::Create(sql::Database* db,
                              const std::string& db_name,
                              const std::string& table_name,
                              const std::string& column_name) {
  sqlite3* const sqlite3_db = db->db(InternalApiToken());

  const char* data_type;
  const char* collation_sequence;
  int not_null;
  int primary_key;
  int auto_increment;
  int status = sqlite3_table_column_metadata(
      sqlite3_db, db_name.c_str(), table_name.c_str(), column_name.c_str(),
      &data_type, &collation_sequence, &not_null, &primary_key,
      &auto_increment);
  CHECK_EQ(status, SQLITE_OK) << "SQLite error: " << sqlite3_errmsg(sqlite3_db);

  // This happens when virtual tables report no type information.
  if (data_type == nullptr)
    data_type = "(nullptr)";

  return {std::string(data_type), std::string(collation_sequence),
          not_null != 0, primary_key != 0, auto_increment != 0};
}

}  // namespace sql::test
