// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/search_engines/keyword_table.h"
#include "components/signin/public/webdata/token_service_table.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::AutofillProfile;
using autofill::AutofillTable;
using autofill::CreditCard;
using base::ASCIIToUTF16;
using base::Time;

namespace {

// To make the comparison with golden files less whitespace sensitive:
// - Remove SQLite quotes: http://www.sqlite.org/lang_keywords.html.
// - Collapse multiple spaces into one.
// - Ensure that there is no space before or after ',', '(' or ')'.
std::string NormalizeSchemaForComparison(const std::string& schema) {
  std::string normalized;
  normalized.reserve(schema.size());
  bool skip_following_spaces = false;
  for (char c : schema) {
    if (base::Contains("\"[]`", c))  // Quotes
      continue;
    if (c == ' ' && skip_following_spaces)
      continue;
    bool is_separator = base::Contains(",()", c);
    if (is_separator && !normalized.empty() && normalized.back() == ' ')
      normalized.pop_back();
    normalized.push_back(c);
    skip_following_spaces = c == ' ' || is_separator;
  }
  return normalized;
}

}  // anonymous namespace

// The WebDatabaseMigrationTest encapsulates testing of database migrations.
// Specifically, these tests are intended to exercise any schema changes in
// the WebDatabase and data migrations that occur in
// |WebDatabase::MigrateOldVersionsAsNeeded()|.
class WebDatabaseMigrationTest : public testing::Test {
 public:
  WebDatabaseMigrationTest() = default;

  WebDatabaseMigrationTest(const WebDatabaseMigrationTest&) = delete;
  WebDatabaseMigrationTest& operator=(const WebDatabaseMigrationTest&) = delete;

  ~WebDatabaseMigrationTest() override = default;

  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  // Load the database via the WebDatabase class and migrate the database to
  // the current version.
  void DoMigration() {
    AutofillTable autofill_table;
    KeywordTable keyword_table;
    TokenServiceTable token_service_table;

    WebDatabase db;
    db.AddTable(&autofill_table);
    db.AddTable(&keyword_table);
    db.AddTable(&token_service_table);

    // This causes the migration to occur.
    ASSERT_EQ(sql::INIT_OK, db.Init(GetDatabasePath()));
  }

 protected:
  // Current tested version number.  When adding a migration in
  // |WebDatabase::MigrateOldVersionsAsNeeded()| and changing the version number
  // |kCurrentVersionNumber| this value should change to reflect the new version
  // number and a new migration test added below.
  static const int kCurrentTestedVersionNumber;

  base::FilePath GetDatabasePath() {
    const base::FilePath::CharType kWebDatabaseFilename[] =
        FILE_PATH_LITERAL("TestWebDatabase.sqlite3");
    return temp_dir_.GetPath().Append(base::FilePath(kWebDatabaseFilename));
  }

  // The textual contents of |file| are read from
  // "components/test/data/web_database" and returned in the string |contents|.
  // Returns true if the file exists and is read successfully, false otherwise.
  bool GetWebDatabaseData(const base::FilePath& file, std::string* contents) {
    base::FilePath source_path;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &source_path);
    source_path = source_path.AppendASCII("components");
    source_path = source_path.AppendASCII("test");
    source_path = source_path.AppendASCII("data");
    source_path = source_path.AppendASCII("web_database");
    source_path = source_path.Append(file);
    return base::PathExists(source_path) &&
           base::ReadFileToString(source_path, contents);
  }

  static int VersionFromConnection(sql::Database* connection) {
    // Get version.
    sql::Statement s(connection->GetUniqueStatement(
        "SELECT value FROM meta WHERE key='version'"));
    if (!s.Step())
      return 0;
    return s.ColumnInt(0);
  }

  // The sql files located in "components/test/data/web_database" were generated
  // by launching the Chromium application prior to schema change, then using
  // the sqlite3 command-line application to dump the contents of the "Web Data"
  // database.
  // Like this:
  //   > .output version_nn.sql
  //   > .dump
  void LoadDatabase(const base::FilePath::StringType& file);

 private:
  base::ScopedTempDir temp_dir_;
};

const int WebDatabaseMigrationTest::kCurrentTestedVersionNumber = 120;

void WebDatabaseMigrationTest::LoadDatabase(
    const base::FilePath::StringType& file) {
  std::string contents;
  ASSERT_TRUE(GetWebDatabaseData(base::FilePath(file), &contents));

  sql::Database connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  ASSERT_TRUE(connection.Execute(contents.data()));
}

// Tests that migrating from the golden files version_XX.sql results in the same
// schema as migrating from an empty database.
TEST_F(WebDatabaseMigrationTest, VersionXxSqlFilesAreGolden) {
  DoMigration();
  sql::Database connection;
  ASSERT_TRUE(connection.Open(GetDatabasePath()));
  const std::string& expected_schema = connection.GetSchema();
  for (int i = WebDatabase::kDeprecatedVersionNumber + 1;
       i < kCurrentTestedVersionNumber; ++i) {
    connection.Raze();
    const base::FilePath& file_name = base::FilePath::FromUTF8Unsafe(
        "version_" + base::NumberToString(i) + ".sql");
    ASSERT_NO_FATAL_FAILURE(LoadDatabase(file_name.value()))
        << "Failed to load " << file_name.MaybeAsASCII();
    DoMigration();

    EXPECT_EQ(NormalizeSchemaForComparison(expected_schema),
              NormalizeSchemaForComparison(connection.GetSchema()))
        << "For version " << i;
  }
}

// Tests that the all migrations from an empty database succeed.
TEST_F(WebDatabaseMigrationTest, MigrateEmptyToCurrent) {
  DoMigration();

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // Check that expected tables are present.
    EXPECT_TRUE(connection.DoesTableExist("autofill"));
    EXPECT_TRUE(connection.DoesTableExist("local_addresses"));
    EXPECT_TRUE(connection.DoesTableExist("credit_cards"));
    EXPECT_TRUE(connection.DoesTableExist("local_ibans"));
    EXPECT_TRUE(connection.DoesTableExist("keywords"));
    EXPECT_TRUE(connection.DoesTableExist("meta"));
    EXPECT_TRUE(connection.DoesTableExist("token_service"));
    // The web_apps and web_apps_icons tables are obsolete as of version 58.
    EXPECT_FALSE(connection.DoesTableExist("web_apps"));
    EXPECT_FALSE(connection.DoesTableExist("web_app_icons"));
    // The web_intents and web_intents_defaults tables are obsolete as of
    // version 58.
    EXPECT_FALSE(connection.DoesTableExist("web_intents"));
    EXPECT_FALSE(connection.DoesTableExist("web_intents_defaults"));
  }
}

// Versions below 83 are deprecated. This verifies that old databases are razed.
TEST_F(WebDatabaseMigrationTest, RazeDeprecatedVersionAndReinit) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_82.sql")));

  // Verify pre-conditions. These are expectations for version 82 of the
  // database.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 82, 79));

    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_cards", "type"));
  }

  DoMigration();

  // Check post-conditions of version 104. This ensures that the migration has
  // happened.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The product_description column and should exist.
    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_cards",
                                           "product_description"));
  }
}

// Tests addition of nickname column in masked_credit_cards table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion83ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_83.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 83, 79));

    EXPECT_FALSE(connection.DoesColumnExist("masked_credit_cards", "nickname"));
    ASSERT_TRUE(connection.ExecuteScriptForTesting(R"(
      INSERT INTO masked_credit_cards (id, status, name_on_card, network,
      last_four, exp_month, exp_year, bank_name)
      VALUES ('card_1', 'status', 'bob', 'VISA', '1234', 12, 2050, 'Chase');
    )"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The nickname column should exist.
    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_cards", "nickname"));

    // Make sure that the default nickname value is empty.
    sql::Statement s_masked_cards(connection.GetUniqueStatement(
        "SELECT nickname FROM masked_credit_cards"));
    ASSERT_TRUE(s_masked_cards.Step());
    EXPECT_EQ("", s_masked_cards.ColumnString(0));
  }
}

// Tests addition of card_issuer column in masked_credit_cards table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion84ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_84.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 84, 79));

    EXPECT_FALSE(
        connection.DoesColumnExist("masked_credit_cards", "card_issuer"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The card_issuer column should exist.
    EXPECT_TRUE(
        connection.DoesColumnExist("masked_credit_cards", "card_issuer"));
  }
}

// Tests removal of use_count and use_date columns in unmasked_credit_cards
// table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion85ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_85.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 85, 79));

    // The use_count and use_date columns should exist.
    EXPECT_TRUE(
        connection.DoesColumnExist("unmasked_credit_cards", "use_count"));
    EXPECT_TRUE(
        connection.DoesColumnExist("unmasked_credit_cards", "use_date"));
    ASSERT_TRUE(connection.ExecuteScriptForTesting(R"(
      INSERT INTO unmasked_credit_cards (id, card_number_encrypted, use_count,
      use_date, unmask_date)
      VALUES ('card_1', 'DEADBEEFDEADBEEF', 20, 1588604100, 1588603065);
      INSERT INTO unmasked_credit_cards (id, card_number_encrypted, use_count,
      use_date, unmask_date)
      VALUES ('card_2', 'ABCDABCD12341234', 45, 0, 1398902400);
      INSERT INTO unmasked_credit_cards (id, card_number_encrypted, use_count,
      use_date, unmask_date)
      VALUES ('card_3', 'FEDCBA9876543210', 0, 1398905745, 1398901532);
      INSERT INTO unmasked_credit_cards (id, card_number_encrypted, use_count,
      use_date, unmask_date)
      VALUES ('card_4', '0123456789ABCDEF', 0, 0, 1398901000);
    )"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The use_count and use_date columns should no longer exist.
    EXPECT_FALSE(
        connection.DoesColumnExist("unmasked_credit_cards", "use_count"));
    EXPECT_FALSE(
        connection.DoesColumnExist("unmasked_credit_cards", "use_date"));

    // Data should have been preserved post migration
    sql::Statement s(connection.GetUniqueStatement(
        "SELECT id, card_number_encrypted, unmask_date "
        "FROM unmasked_credit_cards"));

    ASSERT_TRUE(s.Step());
    EXPECT_EQ("card_1", s.ColumnString(0));
    EXPECT_EQ("DEADBEEFDEADBEEF", s.ColumnString(1));
    EXPECT_EQ(1588603065, s.ColumnInt64(2));

    ASSERT_TRUE(s.Step());
    EXPECT_EQ("card_2", s.ColumnString(0));
    EXPECT_EQ("ABCDABCD12341234", s.ColumnString(1));
    EXPECT_EQ(1398902400, s.ColumnInt64(2));

    ASSERT_TRUE(s.Step());
    EXPECT_EQ("card_3", s.ColumnString(0));
    EXPECT_EQ("FEDCBA9876543210", s.ColumnString(1));
    EXPECT_EQ(1398901532, s.ColumnInt64(2));

    ASSERT_TRUE(s.Step());
    EXPECT_EQ("card_4", s.ColumnString(0));
    EXPECT_EQ("0123456789ABCDEF", s.ColumnString(1));
    EXPECT_EQ(1398901000, s.ColumnInt64(2));

    // No more entries
    EXPECT_FALSE(s.Step());
  }
}

// Tests addition of nickname column in credit_cards table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion86ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_86.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 86, 83));

    EXPECT_FALSE(connection.DoesColumnExist("credit_cards", "nickname"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The nickname column should exist.
    EXPECT_TRUE(connection.DoesColumnExist("credit_cards", "nickname"));
  }
}

// Version 87 added new columns to the autofill_profile_names table. This table
// was since deprecated and replaced by local_profiles. The migration unit test
// to the current version thus no longer applies.

// Tests addition of instrument_id column in masked_credit_cards table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion88ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_88.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 88, 83));

    EXPECT_FALSE(
        connection.DoesColumnExist("masked_credit_cards", "instrument_id"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The instrument_id column should exist.
    EXPECT_TRUE(
        connection.DoesColumnExist("masked_credit_cards", "instrument_id"));
  }
}

// Tests addition of promo code and display strings columns in offer_data table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion93ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_93.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 93, 83));

    EXPECT_FALSE(connection.DoesColumnExist("offer_data", "promo_code"));
    EXPECT_FALSE(connection.DoesColumnExist("offer_data", "value_prop_text"));
    EXPECT_FALSE(connection.DoesColumnExist("offer_data", "see_details_text"));
    EXPECT_FALSE(
        connection.DoesColumnExist("offer_data", "usage_instructions_text"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The new offer_data columns should exist.
    EXPECT_TRUE(connection.DoesColumnExist("offer_data", "promo_code"));
    EXPECT_TRUE(connection.DoesColumnExist("offer_data", "value_prop_text"));
    EXPECT_TRUE(connection.DoesColumnExist("offer_data", "see_details_text"));
    EXPECT_TRUE(
        connection.DoesColumnExist("offer_data", "usage_instructions_text"));
  }
}

// Tests addition of virtual_card_enrollment_state and card_art_url columns in
// masked_credit_cards table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion94ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_94.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 94, 83));

    EXPECT_FALSE(connection.DoesColumnExist("masked_credit_cards",
                                            "virtual_card_enrollment_state"));
    EXPECT_FALSE(
        connection.DoesColumnExist("masked_credit_cards", "card_art_url"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The virtual_card_enrollment_state column and the card_art_url column
    // should exist.
    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_cards",
                                           "virtual_card_enrollment_state"));
    EXPECT_TRUE(
        connection.DoesColumnExist("masked_credit_cards", "card_art_url"));
  }
}

// Version 95 added a new column to the autofill_profile table. This table
// was since deprecated and replaced by local_profiles. The migration unit test
// to the current version thus no longer applies.

// Tests addition of is_active column in keywords table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion96ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_96.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 96, 83));

    EXPECT_FALSE(connection.DoesColumnExist("keywords", "is_active"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    EXPECT_TRUE(connection.DoesColumnExist("keywords", "is_active"));
  }
}

TEST_F(WebDatabaseMigrationTest, MigrateVersion97ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_97.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 97, 83));

    // The status column should exist.
    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_cards", "status"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The status column should not exist.
    EXPECT_FALSE(connection.DoesColumnExist("masked_credit_cards", "status"));
  }
}

TEST_F(WebDatabaseMigrationTest, MigrateVersion98ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_98.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 98, 98));

    // The autofill_profiles_trash table should exist.
    EXPECT_TRUE(connection.DoesTableExist("autofill_profiles_trash"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));
    // The autofill_profiles_trash table should not exist.
    EXPECT_FALSE(connection.DoesTableExist("autofill_profiles_trash"));
  }
}

// Version 99 removed columns from the autofill_profile_names table. This table
// was since deprecated and replaced by local_profiles. The migration unit test
// to the current version thus no longer applies.

TEST_F(WebDatabaseMigrationTest, MigrateVersion100ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_100.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 100, 99));

    // The validity-related columns should exist.
    EXPECT_TRUE(connection.DoesTableExist("credit_card_art_images"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    EXPECT_FALSE(connection.DoesTableExist("credit_card_art_images"));
  }
}

// Version 101 added a new table autofill_profiles_birthdates. This table was
// since deprecated and replaced by local_profiles. The migration unit test
// to the current version thus no longer applies.

// Tests addition of starter_pack_id column in keywords table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion102ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_102.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 102, 99));

    EXPECT_FALSE(connection.DoesColumnExist("keywords", "starter_pack_id"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    EXPECT_TRUE(connection.DoesColumnExist("keywords", "starter_pack_id"));
  }
}

// Tests addition of product_description in masked_credit_cards table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion103ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_103.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 103, 99));

    EXPECT_FALSE(connection.DoesColumnExist("masked_credit_cards",
                                            "product_description"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The product_description column and should exist.
    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_cards",
                                           "product_description"));
  }
}

// Tests addition of new table 'local_ibans'.
TEST_F(WebDatabaseMigrationTest, MigrateVersion104ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_104.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(104, VersionFromConnection(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 104, 100));

    // The ibans table should not exist.
    EXPECT_FALSE(connection.DoesTableExist("ibans"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The local_ibans table should exist.
    EXPECT_TRUE(connection.DoesTableExist("local_ibans"));
  }
}

// Tests addition of new table 'ibans' with guid as PRIMARY KEY.
TEST_F(WebDatabaseMigrationTest, MigrateVersion105ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_105.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(105, VersionFromConnection(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 105, 100));

    // The ibans table should exist, but should not have been created with guid
    // as PRIMARY KEY.
    ASSERT_TRUE(connection.DoesTableExist("ibans"));
    ASSERT_EQ(connection.GetSchema().find(
                  "CREATE TABLE ibans (guid VARCHAR PRIMARY KEY"),
              std::string::npos);
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The local_ibans table should exist with guid as primary key.
    EXPECT_TRUE(connection.DoesTableExist("local_ibans"));
    ASSERT_NE(connection.GetSchema().find(
                  "CREATE TABLE \"local_ibans\" (guid VARCHAR PRIMARY KEY"),
              std::string::npos);
  }
}

// Tests that both contact_info tables are created.
TEST_F(WebDatabaseMigrationTest, MigrateVersion106ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_106.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(106, VersionFromConnection(&connection));

    // The contact_info tables should not exist.
    EXPECT_FALSE(connection.DoesTableExist("contact_info"));
    EXPECT_FALSE(connection.DoesTableExist("contact_info_types"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The contact_info tables should exist.
    EXPECT_TRUE(connection.DoesTableExist("contact_info"));
    EXPECT_TRUE(connection.DoesTableExist("contact_info_type_tokens"));
  }
}

// Tests addition of card_isser_id in masked_credit_cards table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion107ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_107.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    sql::MetaTable meta_table;
    ASSERT_TRUE(meta_table.Init(&connection, 107, 106));

    EXPECT_FALSE(
        connection.DoesColumnExist("masked_credit_cards", "card_issuer_id"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The card_issuer_id column and should exist.
    EXPECT_TRUE(
        connection.DoesColumnExist("masked_credit_cards", "card_issuer_id"));
  }
}

// Tests verifying the Virtual Card Usage Data table is created.
TEST_F(WebDatabaseMigrationTest, MigrateVersion108ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_108.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(108, VersionFromConnection(&connection));

    // The virtual_card_usage_data table should not exist.
    EXPECT_FALSE(connection.DoesTableExist("virtual_card_usage_data"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The virtual_card_usage_data tables should exist.
    EXPECT_TRUE(connection.DoesTableExist("virtual_card_usage_data"));
  }
}

// Tests that the initial_creator_id and last_modifier_id columns are added to
// the contact_info table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion109ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_109.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(109, VersionFromConnection(&connection));
    EXPECT_FALSE(
        connection.DoesColumnExist("contact_info", "initial_creator_id"));
    EXPECT_FALSE(
        connection.DoesColumnExist("contact_info", "last_modifier_id"));
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));
    EXPECT_TRUE(
        connection.DoesColumnExist("contact_info", "initial_creator_id"));
    EXPECT_TRUE(connection.DoesColumnExist("contact_info", "last_modifier_id"));
  }
}

// Tests that the virtual_card_enrollment_type column is added to the
// masked_credit_cards table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion110ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_110.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(110, VersionFromConnection(&connection));
    EXPECT_FALSE(connection.DoesColumnExist("masked_credit_cards",
                                            "virtual_card_enrollment_type"));
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));
    EXPECT_TRUE(connection.DoesColumnExist("masked_credit_cards",
                                           "virtual_card_enrollment_type"));
  }
}

// Tests that the enforced_by_policy column is added to the keywords table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion111ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_111.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(111, VersionFromConnection(&connection));
    EXPECT_FALSE(connection.DoesColumnExist("keywords", "enforced_by_policy"));
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));
    EXPECT_TRUE(connection.DoesColumnExist("keywords", "enforced_by_policy"));
  }
}

// Tests that the autofill_profiles tables are deprecated and any profiles are
// migrated to the new local_addresses and local_addresses_type_tokens tables.
TEST_F(WebDatabaseMigrationTest, MigrateVersion112ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_112.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(112, VersionFromConnection(&connection));
    EXPECT_FALSE(connection.DoesTableExist("local_addresses"));
    EXPECT_FALSE(connection.DoesTableExist("local_addresses_type_tokens"));

    // Add two profiles to the legacy tables. This cannot be done via
    // AutofillTable, since it only operates on the new local_addresses tables.
    // Note that the ZIP code must be present in the unstructured and
    // structured address table.
    ASSERT_TRUE(connection.ExecuteScriptForTesting(R"(
      INSERT INTO autofill_profiles (guid, date_modified, zipcode)
      VALUES ('00000000-0000-0000-0000-000000000000', 123, '4567');
      INSERT INTO autofill_profile_names (guid, full_name)
      VALUES ('00000000-0000-0000-0000-000000000000', 'full name');
      INSERT INTO autofill_profile_addresses (guid, zip_code)
      VALUES ('00000000-0000-0000-0000-000000000000', '4567');
      INSERT INTO autofill_profiles (guid)
      VALUES ('00000000-0000-0000-0000-000000000001');
    )"));
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));
    EXPECT_TRUE(connection.DoesTableExist("local_addresses"));
    EXPECT_TRUE(connection.DoesTableExist("local_addresses_type_tokens"));

    // Expect to find the profiles in the local_addresses tables. AutofillTable
    // will read from them.
    AutofillTable table;
    table.Init(&connection, /*meta_table=*/nullptr);
    std::unique_ptr<AutofillProfile> profile =
        table.GetAutofillProfile("00000000-0000-0000-0000-000000000000",
                                 AutofillProfile::Source::kLocalOrSyncable);
    ASSERT_TRUE(profile);
    EXPECT_EQ(profile->modification_date(), Time::FromTimeT(123));
    EXPECT_EQ(profile->GetRawInfo(autofill::NAME_FULL), u"full name");
    EXPECT_EQ(profile->GetRawInfo(autofill::ADDRESS_HOME_ZIP), u"4567");

    EXPECT_TRUE(
        table.GetAutofillProfile("00000000-0000-0000-0000-000000000001",
                                 AutofillProfile::Source::kLocalOrSyncable));
  }
}

// Tests that the autofill_profiles tables are deprecated and any profiles are
// migrated to the new local_addresses and local_addresses_type_tokens tables.
TEST_F(WebDatabaseMigrationTest, MigrateVersion113ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_113.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(113, VersionFromConnection(&connection));
    EXPECT_TRUE(connection.DoesTableExist("autofill_profiles"));
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));
    EXPECT_FALSE(connection.DoesTableExist("autofill_profiles"));
  }
}

// Tests that the IBAN value column is encrypted in local_ibans table.
TEST_F(WebDatabaseMigrationTest, MigrateVersion114ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_114.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(114, VersionFromConnection(&connection));
    EXPECT_TRUE(connection.DoesColumnExist("ibans", "value"));
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));
    EXPECT_TRUE(connection.DoesColumnExist("local_ibans", "value_encrypted"));
    EXPECT_FALSE(connection.DoesColumnExist("local_ibans", "value"));
  }
}

// Tests verifying both stored_cvc tables are created.
TEST_F(WebDatabaseMigrationTest, MigrateVersion115ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_115.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(115, VersionFromConnection(&connection));

    // The stored_cvc tables should not exist.
    EXPECT_FALSE(connection.DoesTableExist("local_stored_cvc"));
    EXPECT_FALSE(connection.DoesTableExist("server_stored_cvc"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The stored_cvc tables should exist.
    EXPECT_TRUE(connection.DoesTableExist("local_stored_cvc"));
    EXPECT_TRUE(connection.DoesTableExist("server_stored_cvc"));
  }
}

// Tests verifying an observations column is created for the
// contact_info_type_tokens and local_addresses_type_tokens tables.
TEST_F(WebDatabaseMigrationTest, MigrateVersion116ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_116.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));
    EXPECT_EQ(116, VersionFromConnection(&connection));
    EXPECT_FALSE(
        connection.DoesColumnExist("contact_info_type_tokens", "observations"));
    EXPECT_FALSE(connection.DoesColumnExist("local_addresses_type_tokens",
                                            "observations"));
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));
    EXPECT_TRUE(
        connection.DoesColumnExist("contact_info_type_tokens", "observations"));
    EXPECT_TRUE(connection.DoesColumnExist("local_addresses_type_tokens",
                                           "observations"));
  }
}

TEST_F(WebDatabaseMigrationTest, MigrateVersion117ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_117.sql")));
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(117, VersionFromConnection(&connection));
    EXPECT_TRUE(connection.DoesTableExist("payments_upi_vpa"));
  }
  DoMigration();
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));
    EXPECT_FALSE(connection.DoesTableExist("payments_upi_vpa"));
  }
}

// Tests addition of new tables 'masked_ibans' and `masked_iban_metadata`, also
// test that `ibans` has been renamed to `local_ibans`.
TEST_F(WebDatabaseMigrationTest, MigrateVersion118ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_118.sql")));

  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));
    EXPECT_EQ(118, VersionFromConnection(&connection));

    EXPECT_FALSE(connection.DoesTableExist("masked_ibans"));
    EXPECT_FALSE(connection.DoesTableExist("masked_ibans_metadata"));
    EXPECT_TRUE(connection.DoesTableExist("ibans"));
    EXPECT_FALSE(connection.DoesTableExist("local_ibans"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    // The `masked_ibans` and `masked_iban_metadata` tables should exist.
    EXPECT_TRUE(connection.DoesTableExist("masked_ibans"));
    EXPECT_TRUE(connection.DoesTableExist("masked_ibans_metadata"));
    // The `ibans` table should be renamed to `local_ibans`.
    EXPECT_TRUE(connection.DoesTableExist("local_ibans"));
    EXPECT_FALSE(connection.DoesTableExist("ibans"));
  }
}

TEST_F(WebDatabaseMigrationTest, MigrationVersion119ToCurrent) {
  ASSERT_NO_FATAL_FAILURE(LoadDatabase(FILE_PATH_LITERAL("version_119.sql")));
  // Verify pre-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(119, VersionFromConnection(&connection));

    EXPECT_FALSE(connection.DoesTableExist("bank_accounts"));
    EXPECT_FALSE(connection.DoesTableExist("payment_instruments"));
    EXPECT_FALSE(connection.DoesTableExist("payment_instruments_metadata"));
    EXPECT_FALSE(
        connection.DoesTableExist("payment_instrument_supported_rails"));
  }

  DoMigration();

  // Verify post-conditions.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(GetDatabasePath()));
    ASSERT_TRUE(sql::MetaTable::DoesTableExist(&connection));

    // Check version.
    EXPECT_EQ(kCurrentTestedVersionNumber, VersionFromConnection(&connection));

    EXPECT_TRUE(connection.DoesTableExist("bank_accounts"));
    EXPECT_TRUE(connection.DoesTableExist("payment_instruments"));
    EXPECT_TRUE(connection.DoesTableExist("payment_instruments_metadata"));
    EXPECT_TRUE(
        connection.DoesTableExist("payment_instrument_supported_rails"));
  }
}
