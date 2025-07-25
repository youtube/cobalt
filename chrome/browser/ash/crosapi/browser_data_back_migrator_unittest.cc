// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/browser_data_back_migrator.h"

#include <errno.h>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/crosapi/browser_data_back_migrator_metrics.h"
#include "chrome/browser/ash/crosapi/browser_data_migrator_util.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/extensions/extension_keeplist_chromeos.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace ash {

namespace {

constexpr char kAshDataFilePath[] = "AshData";
constexpr char kAshDataContent[] = "Hello, Ash, my old friend!";
constexpr size_t kAshDataSize = sizeof(kAshDataContent);

constexpr char kLacrosDataFilePath[] = "LacrosData";
constexpr char kLacrosDataContent[] = "Au revoir, Lacros!";
constexpr size_t kLacrosDataSize = sizeof(kLacrosDataContent);

// ID of an extension that only exists in Lacros after forward migration.
// NOTE: we use a sequence of characters that can't be an actual AppId here,
// so we can be sure that it won't be included in `kExtensionsAshOnly`.
constexpr char kLacrosOnlyExtensionId[] = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

constexpr char kPrimaryUser[] = "primary-user@gmail.com";
constexpr char kSecondaryUser[] = "secondary-user@gmail.com";

// ID of an extension that runs in both Lacros and Ash chrome.
std::string_view GetBothChromesExtensionId() {
  // Any id from the Ash allowlist works for tests. Pick the first
  // element of the allowlist for convenience.
  DCHECK(
      !extensions::GetExtensionsAndAppsRunInOSAndStandaloneBrowser().empty());
  return extensions::GetExtensionsAndAppsRunInOSAndStandaloneBrowser()[0];
}

const char* kAshOnlyExtensionId =
    browser_data_migrator_util::kExtensionsAshOnly[0];

// Key prefixes in LocalStorage's LevelDB.
constexpr char kMetaPrefix[] = "META:chrome-extension://";
constexpr char kKeyPrefix[] = "_chrome-extension://";

constexpr char kAshLevelDBValue[] = "ash-value";
constexpr char kLacrosLevelDBValue[] = "lacros-value";
constexpr char kAshLevelDBMeta[] = "ash-meta";
constexpr char kLacrosLevelDBMeta[] = "lacros-meta";

const int kAshPrefValue = 0;
const int kLacrosPrefValue = 1;
// Dotted paths of preferences not found in:
// - kAshOnlyPreferencesKeys
// - kLacrosOnlyPreferencesKeys
// - kSplitPreferencesKeys
constexpr char kOtherLacrosPreference[] = "xxx.xxx.xxx";
constexpr char kOtherAshPreference[] = "yyy.xxx.xxx";
constexpr char kOtherBothChromesPreference[] = "zzz.xxx.xxx";

enum class FilesSetup {
  kAshOnly = 0,
  kLacrosOnly = 1,
  kBothChromes = 2,
  kMaxValue = kBothChromes,
};

void CreateDirectoryForTesting(const base::FilePath& directory_path) {
  ASSERT_TRUE(base::CreateDirectory(directory_path));
}

void CreateDirectoryAndFile(const base::FilePath& directory_path,
                            const char* file_path,
                            const char* file_content,
                            int file_size) {
  CreateDirectoryForTesting(directory_path);
  ASSERT_TRUE(base::WriteFile(directory_path.Append(file_path),
                              base::StringPiece(file_content, file_size)));
}

void SetUpExtensions(const base::FilePath& ash_profile_dir,
                     const base::FilePath& lacros_default_profile_dir,
                     FilesSetup setup) {
  // The extension test data should have the following structure:
  // |- user
  //   |- Extensions
  //       |- <ash-only-ext>
  //           |- AshData
  //       |- <shared-ext>
  //           |- AshData
  //   |- lacros
  //       |- Default
  //           |- Extensions
  //               |- <lacros-only-ext>
  //                   |- LacrosData
  //               |- <shared-ext>
  //                   |- LacrosData
  base::FilePath ash_extensions_path =
      ash_profile_dir.Append(browser_data_migrator_util::kExtensionsFilePath);

  base::FilePath lacros_extensions_path = lacros_default_profile_dir.Append(
      browser_data_migrator_util::kExtensionsFilePath);

  if (setup != FilesSetup::kAshOnly) {
    // Generate data for a Lacros-only extension.
    CreateDirectoryAndFile(
        lacros_extensions_path.Append(kLacrosOnlyExtensionId),
        kLacrosDataFilePath, kLacrosDataContent, kLacrosDataSize);
    // Generate Lacros data for an extension existing in both Chromes.
    CreateDirectoryAndFile(
        lacros_extensions_path.Append(GetBothChromesExtensionId()),
        kLacrosDataFilePath, kLacrosDataContent, kLacrosDataSize);
  }

  if (setup != FilesSetup::kLacrosOnly) {
    // Generate data for an Ash-only extension.
    CreateDirectoryAndFile(ash_extensions_path.Append(kAshOnlyExtensionId),
                           kAshDataFilePath, kAshDataContent, kAshDataSize);
    // Generate Ash data for an extension existing in both Chromes.
    CreateDirectoryAndFile(
        ash_extensions_path.Append(GetBothChromesExtensionId()),
        kAshDataFilePath, kAshDataContent, kAshDataSize);
  }
}

void SetUpIndexedDB(const base::FilePath& ash_profile_dir,
                    const base::FilePath& lacros_default_profile_dir,
                    FilesSetup setup) {
  // The IndexedDB test data should have the following structure for full setup:
  // |- user
  //     |- IndexedDB
  //         |- chrome_extension_<shared-ext>_0.indexeddb.blob
  //             |- AshData
  //         |- chrome_extension_<shared-ext>_0.indexeddb.leveldb
  //             |- AshData
  //         |- chrome_extension_<ash-only-ext>_0.indexeddb.blob
  //             |- AshData
  //         |- chrome_extension_<ash-only-ext>_0.indexeddb.leveldb
  //             |- AshData
  //     |- lacros
  //         |- Default
  //             |- IndexedDB
  //                 |- chrome_extension_<shared-ext>_0.indexeddb.blob
  //                     |- LacrosData
  //                 |- chrome_extension_<shared-ext>_0.indexeddb.leveldb
  //                     |- LacrosData
  //                 |- chrome_extension_<lacros-only-ext>_0.indexeddb.blob
  //                     |- LacrosData
  //                 |- chrome_extension_<lacros-only-ext>_0.indexeddb.leveldb
  //                     |- LacrosData

  // Create IndexedDB files for the Lacros-only extension.
  if (setup != FilesSetup::kAshOnly) {
    const auto& [lacros_only_blob_path, lacros_only_leveldb_path] =
        browser_data_migrator_util::GetIndexedDBPaths(
            lacros_default_profile_dir, kLacrosOnlyExtensionId);
    CreateDirectoryAndFile(lacros_only_blob_path, kLacrosDataFilePath,
                           kLacrosDataContent, kLacrosDataSize);
    CreateDirectoryAndFile(lacros_only_leveldb_path, kLacrosDataFilePath,
                           kLacrosDataContent, kLacrosDataSize);
  }

  // Create IndexedDB files for the Ash-only extension.
  if (setup != FilesSetup::kLacrosOnly) {
    const auto& [ash_only_blob_path, ash_only_leveldb_path] =
        browser_data_migrator_util::GetIndexedDBPaths(ash_profile_dir,
                                                      kAshOnlyExtensionId);
    CreateDirectoryAndFile(ash_only_blob_path, kAshDataFilePath,
                           kAshDataContent, kAshDataSize);
    CreateDirectoryAndFile(ash_only_leveldb_path, kAshDataFilePath,
                           kAshDataContent, kAshDataSize);
  }

  // Create IndexedDB files for the extension existing in both Chromes.
  if (setup != FilesSetup::kAshOnly) {
    const auto& [lacros_blob_path, lacros_leveldb_path] =
        browser_data_migrator_util::GetIndexedDBPaths(
            lacros_default_profile_dir, GetBothChromesExtensionId().data());

    CreateDirectoryAndFile(lacros_blob_path, kLacrosDataFilePath,
                           kLacrosDataContent, kLacrosDataSize);
    CreateDirectoryAndFile(lacros_leveldb_path, kLacrosDataFilePath,
                           kLacrosDataContent, kLacrosDataSize);
  }

  if (setup != FilesSetup::kLacrosOnly) {
    const auto& [ash_blob_path, ash_leveldb_path] =
        browser_data_migrator_util::GetIndexedDBPaths(
            ash_profile_dir, GetBothChromesExtensionId().data());
    CreateDirectoryAndFile(ash_blob_path, kAshDataFilePath, kAshDataContent,
                           kAshDataSize);
    CreateDirectoryAndFile(ash_leveldb_path, kAshDataFilePath, kAshDataContent,
                           kAshDataSize);
  }
}

void GenerateLevelDB(const base::FilePath& path,
                     std::map<std::string, std::string> values) {
  // Open a new LevelDB database.
  leveldb_env::Options options;
  options.create_if_missing = true;
  std::unique_ptr<leveldb::DB> db;
  leveldb::Status status = leveldb_env::OpenDB(options, path.value(), &db);
  ASSERT_TRUE(status.ok());

  // Write all options in a batch.
  leveldb::WriteBatch batch;
  for (const auto& [key, value] : values) {
    batch.Put(key, value);
  }

  leveldb::WriteOptions write_options;
  write_options.sync = true;
  status = db->Write(write_options, &batch);
  ASSERT_TRUE(status.ok());
}

// Return all the key-value pairs in a LevelDB.
std::map<std::string, std::string> ReadLevelDB(const base::FilePath& path) {
  leveldb_env::Options options;
  options.create_if_missing = false;

  std::unique_ptr<leveldb::DB> db;
  leveldb::Status status = leveldb_env::OpenDB(options, path.value(), &db);
  EXPECT_TRUE(status.ok());

  std::map<std::string, std::string> db_map;
  std::unique_ptr<leveldb::Iterator> it(
      db->NewIterator(leveldb::ReadOptions()));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    db_map.emplace(it->key().ToString(), it->value().ToString());
  }

  return db_map;
}

bool WriteJSONDict(const base::Value::Dict& json_dict,
                   const base::FilePath& path) {
  std::string serialized_dict;

  if (!base::JSONWriter::Write(json_dict, &serialized_dict)) {
    return false;
  }
  if (!(base::PathExists(path.DirName()) ||
        base::CreateDirectory(path.DirName()))) {
    return false;
  }
  if (!base::WriteFile(path, serialized_dict)) {
    return false;
  }

  return true;
}

bool ReadJSON(const base::FilePath& path, base::Value* json_out) {
  std::string file_contents;

  if (!base::ReadFileToString(path, &file_contents)) {
    return false;
  }

  absl::optional<base::Value> deserialized_json =
      base::JSONReader::Read(file_contents);
  if (!deserialized_json.has_value()) {
    return false;
  }

  *json_out = std::move(deserialized_json.value());
  return true;
}

size_t CountStringInList(const base::Value::List& list,
                         const std::string& value) {
  return std::count_if(list.cbegin(), list.cend(),
                       [&](const base::Value& item) {
                         return item.is_string() && item.GetString() == value;
                       });
}

class BrowserDataBackMigratorTest : public testing::Test {
 public:
  BrowserDataBackMigratorTest() {
    using std::string_literals::operator""s;

    kAshOnlyMetaKey = kMetaPrefix + std::string(kAshOnlyExtensionId);
    kAshOnlyValueKey =
        kKeyPrefix + std::string(kAshOnlyExtensionId) + "\x00key"s;
    kLacrosOnlyMetaKey = kMetaPrefix + std::string(kLacrosOnlyExtensionId);
    kLacrosOnlyValueKey =
        kKeyPrefix + std::string(kLacrosOnlyExtensionId) + "\x00key"s;
    kBothChromesMetaKey =
        kMetaPrefix + std::string(GetBothChromesExtensionId());
    kBothChromesValueKey =
        kKeyPrefix + std::string(GetBothChromesExtensionId()) + "\x00key"s;

    kAshOnlyStateStoreKey = std::string(kAshOnlyExtensionId) + ".key";
    kLacrosOnlyStateStoreKey = std::string(kLacrosOnlyExtensionId) + ".key";
    kBothChromesStateStoreKey =
        std::string(GetBothChromesExtensionId()) + ".key";
  }

  void SetUp() override {
    // Setup `user_data_dir_` as below.
    // This corresponds to the directory structure under /home/chronos/user.
    // ./                             /* user_data_dir_ */
    // |- user/                       /* ash_profile_dir_ */
    //     |- back_migrator_tmp/      /* tmp_profile_dir_ */
    //     |- lacros/                 /* lacros_dir_ */
    //         |- Default/            /* lacros_default_profile_dir_ */
    //             |- Extensions
    //             |- IndexedDB
    //             |- Storage
    //                 |- ext
    //     |- Cache
    //     |- Cookies
    //     |- Bookmarks
    //     |- Downloads/data
    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());

    ash_profile_dir_ = user_data_dir_.GetPath().Append("user");

    lacros_dir_ =
        ash_profile_dir_.Append(browser_data_migrator_util::kLacrosDir);

    lacros_default_profile_dir_ =
        lacros_dir_.Append(browser_data_migrator_util::kLacrosProfilePath);

    tmp_profile_dir_ =
        ash_profile_dir_.Append(browser_data_back_migrator::kTmpDir);

    tmp_prefs_path_ = tmp_profile_dir_.Append("Preferences");
    lacros_prefs_path_ = lacros_default_profile_dir_.Append("Preferences");
    ash_prefs_path_ = ash_profile_dir_.Append("Preferences");
  }

  void TearDown() override { EXPECT_TRUE(user_data_dir_.Delete()); }

  void CreateTemporaryDirectory() {
    // During backward migration, `tmp_profile_dir_` is created in
    // `MergeSplitItems`, but we don't want to call that in tests so we generate
    // it ourselves.
    CreateDirectoryForTesting(tmp_profile_dir_);
  }

  void CreateLacrosDirectory() {
    // This directory is created during forward migration and parts of backward
    // migration code rely on its existence, i.e. do nothing if it is not found.
    CreateDirectoryForTesting(lacros_default_profile_dir_);
  }

  void SetupLocalStorageLevelDBFiles(
      const base::FilePath& ash_profile_dir,
      const base::FilePath& lacros_default_profile_dir,
      FilesSetup setup) {
    // The LevelDB test data should have the following structure for full setup,
    // with all the leaves representing LevelDB databases.
    // |- user
    //     |- Local Storage
    //         |- leveldb
    //     |- lacros
    //         |- Default
    //             |- Local Storage
    //                 |- leveldb

    if (setup != FilesSetup::kLacrosOnly) {
      // Generate Ash Local Storage leveldb.
      base::FilePath ash_local_storage_leveldb_path =
          ash_profile_dir
              .Append(browser_data_migrator_util::kLocalStorageFilePath)
              .Append(browser_data_migrator_util::kLocalStorageLeveldbName);
      std::map<std::string, std::string> ash_values;
      ash_values["VERSION"] = "1";
      ash_values[kAshOnlyMetaKey] = kAshLevelDBMeta;
      ash_values[kAshOnlyValueKey] = kAshLevelDBValue;
      ash_values[kBothChromesMetaKey] = kAshLevelDBMeta;
      ash_values[kBothChromesValueKey] = kAshLevelDBValue;
      GenerateLevelDB(ash_local_storage_leveldb_path, ash_values);
    }

    if (setup != FilesSetup::kAshOnly) {
      // Generate Lacros Local Storage leveldb.
      base::FilePath lacros_local_storage_leveldb_path =
          lacros_default_profile_dir
              .Append(browser_data_migrator_util::kLocalStorageFilePath)
              .Append(browser_data_migrator_util::kLocalStorageLeveldbName);
      std::map<std::string, std::string> lacros_values;
      lacros_values["VERSION"] = "1";
      lacros_values[kLacrosOnlyMetaKey] = kLacrosLevelDBMeta;
      lacros_values[kLacrosOnlyValueKey] = kLacrosLevelDBValue;
      lacros_values[kBothChromesMetaKey] = kLacrosLevelDBMeta;
      lacros_values[kBothChromesValueKey] = kLacrosLevelDBValue;
      GenerateLevelDB(lacros_local_storage_leveldb_path, lacros_values);
    }
  }

  void SetupStateStoreLevelDBFiles(
      const base::FilePath& ash_profile_dir,
      const base::FilePath& lacros_default_profile_dir,
      FilesSetup setup) {
    // The LevelDB test data should have the following structure for full setup,
    // with all the leaves representing LevelDB databases.
    // |- user
    //     |- Extension Rules
    //     |- Extension Scripts
    //     |- Extension State
    //     |- lacros
    //         |- Default
    //             |- Extension Rules
    //             |- Extension Scripts
    //             |- Extension State

    for (const char* path : browser_data_migrator_util::kStateStorePaths) {
      if (setup != FilesSetup::kLacrosOnly) {
        base::FilePath ash_path = ash_profile_dir.Append(path);
        std::map<std::string, std::string> ash_values;
        ash_values[kAshOnlyStateStoreKey] = kAshLevelDBValue;
        ash_values[kBothChromesStateStoreKey] = kAshLevelDBValue;
        GenerateLevelDB(ash_path, ash_values);
      }

      if (setup != FilesSetup::kAshOnly) {
        base::FilePath lacros_path = lacros_default_profile_dir.Append(path);
        std::map<std::string, std::string> lacros_values;
        lacros_values[kLacrosOnlyStateStoreKey] = kLacrosLevelDBValue;
        lacros_values[kBothChromesStateStoreKey] = kLacrosLevelDBValue;
        GenerateLevelDB(lacros_path, lacros_values);
      }
    }
  }

  void CreateAshAndLacrosPrefs(const base::Value::Dict& ash_prefs,
                               const base::Value::Dict& lacros_prefs) {
    ASSERT_TRUE(WriteJSONDict(ash_prefs, ash_prefs_path_));
    ASSERT_TRUE(WriteJSONDict(lacros_prefs, lacros_prefs_path_));
  }

  base::ScopedTempDir user_data_dir_;
  base::FilePath ash_profile_dir_;
  base::FilePath lacros_dir_;
  base::FilePath lacros_default_profile_dir_;
  base::FilePath tmp_profile_dir_;

  base::FilePath tmp_prefs_path_;
  base::FilePath ash_prefs_path_;
  base::FilePath lacros_prefs_path_;

  std::string kAshOnlyMetaKey;
  std::string kAshOnlyValueKey;
  std::string kLacrosOnlyMetaKey;
  std::string kLacrosOnlyValueKey;
  std::string kBothChromesMetaKey;
  std::string kBothChromesValueKey;

  std::string kAshOnlyStateStoreKey;
  std::string kLacrosOnlyStateStoreKey;
  std::string kBothChromesStateStoreKey;

  TestingPrefServiceSimple pref_service_;
};

class BrowserDataBackMigratorFilesSetupTest
    : public BrowserDataBackMigratorTest,
      public testing::WithParamInterface<FilesSetup> {};

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         BrowserDataBackMigratorFilesSetupTest,
                         testing::Values(FilesSetup::kAshOnly,
                                         FilesSetup::kLacrosOnly,
                                         FilesSetup::kBothChromes));

}  // namespace

TEST_F(BrowserDataBackMigratorTest, PreMigrationCleanUp) {
  // Create the temporary directory to make sure it is deleted during cleanup.
  CreateTemporaryDirectory();

  base::HistogramTester histogram_tester;

  BrowserDataBackMigrator::TaskResult result =
      BrowserDataBackMigrator::PreMigrationCleanUp(ash_profile_dir_,
                                                   lacros_default_profile_dir_);
  ASSERT_EQ(result.status, BrowserDataBackMigrator::TaskStatus::kSucceeded);

  ASSERT_FALSE(base::PathExists(tmp_profile_dir_));

  histogram_tester.ExpectTotalCount(
      browser_data_back_migrator_metrics::kPreMigrationCleanUpTimeUMA, 1);
}

TEST_F(BrowserDataBackMigratorTest, MergeCommonExtensionsDataFiles) {
  SetUpExtensions(ash_profile_dir_, lacros_default_profile_dir_,
                  FilesSetup::kBothChromes);

  ASSERT_TRUE(BrowserDataBackMigrator::MergeCommonExtensionsDataFiles(
      ash_profile_dir_, lacros_default_profile_dir_, tmp_profile_dir_,
      browser_data_migrator_util::kExtensionsFilePath));

  // Expected structure after this merge step:
  // |- user
  //   |- Extensions
  //       |- <ash-only-ext>
  //           |- AshData
  //       |- <shared-ext>
  //           |- AshData
  //   |- back_migrator_tmp
  //       |- Extensions
  //           |- <shared-ext>
  //               |- LacrosData
  //   |- lacros
  //       |- Default
  //           |- Extensions
  //               |- <lacros-only-ext>
  //                   |- LacrosData
  //               |- <shared-ext>
  //                   |- LacrosData
  base::FilePath tmp_extensions_path =
      tmp_profile_dir_.Append(browser_data_migrator_util::kExtensionsFilePath);

  // The Lacros-only extension data does not exist at this point.
  ASSERT_FALSE(
      base::PathExists(tmp_extensions_path.Append(kLacrosOnlyExtensionId)
                           .Append(kLacrosDataFilePath)));

  // The Ash-only extension data does not exist.
  ASSERT_FALSE(base::PathExists(tmp_extensions_path.Append(kAshOnlyExtensionId)
                                    .Append(kAshDataFilePath)));

  // The Ash version of the both-Chromes extension does not exist.
  ASSERT_FALSE(
      base::PathExists(tmp_extensions_path.Append(GetBothChromesExtensionId())
                           .Append(kAshDataFilePath)));

  // The Lacros version of the both-Chromes extension exists.
  base::FilePath lacros_tmp_file_path =
      tmp_extensions_path.Append(GetBothChromesExtensionId())
          .Append(kLacrosDataFilePath);
  ASSERT_TRUE(base::PathExists(lacros_tmp_file_path));

  // The contents of the file in the temporary directory are the same as the
  // contents of the file in the original Lacros directory.
  base::FilePath lacros_original_file_path =
      lacros_default_profile_dir_
          .Append(browser_data_migrator_util::kExtensionsFilePath)
          .Append(kLacrosOnlyExtensionId)
          .Append(kLacrosDataFilePath);

  std::string tmp_data;
  ASSERT_TRUE(base::ReadFileToString(lacros_tmp_file_path, &tmp_data));
  std::string original_data;
  ASSERT_TRUE(
      base::ReadFileToString(lacros_original_file_path, &original_data));
  EXPECT_EQ(tmp_data, original_data);
}

TEST_P(BrowserDataBackMigratorFilesSetupTest, MergeCommonIndexedDB) {
  auto files_setup = GetParam();
  SetUpIndexedDB(ash_profile_dir_, lacros_default_profile_dir_, files_setup);

  const char* extension_id = GetBothChromesExtensionId().data();

  ASSERT_TRUE(BrowserDataBackMigrator::MergeCommonIndexedDB(
      ash_profile_dir_, lacros_default_profile_dir_, extension_id));

  const auto& [ash_blob_path, ash_leveldb_path] =
      browser_data_migrator_util::GetIndexedDBPaths(ash_profile_dir_,
                                                    extension_id);
  const auto& [lacros_blob_path, lacros_leveldb_path] =
      browser_data_migrator_util::GetIndexedDBPaths(lacros_default_profile_dir_,
                                                    extension_id);

  // The Lacros files do not exist - they've either been moved to Ash or they
  // did not exist in the first place.
  ASSERT_FALSE(base::PathExists(lacros_blob_path.Append(kLacrosDataFilePath)));
  ASSERT_FALSE(
      base::PathExists(lacros_leveldb_path.Append(kLacrosDataFilePath)));

  if (files_setup == FilesSetup::kAshOnly) {
    // The Ash version is still in Ash.
    ASSERT_TRUE(base::PathExists(ash_blob_path.Append(kAshDataFilePath)));
    ASSERT_TRUE(base::PathExists(ash_leveldb_path.Append(kAshDataFilePath)));
  } else {
    // The Ash version has been deleted.
    ASSERT_FALSE(base::PathExists(ash_blob_path.Append(kAshDataFilePath)));
    ASSERT_FALSE(base::PathExists(ash_leveldb_path.Append(kAshDataFilePath)));

    // The Lacros version has been moved to Ash.
    ASSERT_TRUE(base::PathExists(ash_blob_path.Append(kLacrosDataFilePath)));
    ASSERT_TRUE(base::PathExists(ash_leveldb_path.Append(kLacrosDataFilePath)));
  }
}

TEST_P(BrowserDataBackMigratorFilesSetupTest, MergeLocalStorageLevelDB) {
  auto files_setup = GetParam();
  SetupLocalStorageLevelDBFiles(ash_profile_dir_, lacros_default_profile_dir_,
                                files_setup);
  CreateTemporaryDirectory();

  base::FilePath ash_local_storage = ash_profile_dir_.Append(
      browser_data_migrator_util::kLocalStorageFilePath);
  base::FilePath lacros_local_storage = lacros_default_profile_dir_.Append(
      browser_data_migrator_util::kLocalStorageFilePath);
  base::FilePath tmp_local_storage = tmp_profile_dir_.Append(
      browser_data_migrator_util::kLocalStorageFilePath);

  if (files_setup != FilesSetup::kAshOnly) {
    // If the Lacros LevelDB version exists, use it as basis and then overwrite
    // some of its contents with the Ash LevelDB version, if it exists. We
    // expect both copy and merge steps to succeed.
    ASSERT_FALSE(base::PathExists(tmp_local_storage));
    ASSERT_TRUE(BrowserDataBackMigrator::CopyLevelDBBase(lacros_local_storage,
                                                         tmp_local_storage));
    ASSERT_TRUE(base::PathExists(tmp_local_storage));
    ASSERT_TRUE(BrowserDataBackMigrator::MergeLevelDB(
        ash_local_storage.Append(
            browser_data_migrator_util::kLocalStorageLeveldbName),
        tmp_local_storage.Append(
            browser_data_migrator_util::kLocalStorageLeveldbName),
        browser_data_migrator_util::LevelDBType::kLocalStorage));

    // Check the contents of the LevelDB database. It should always contain the
    // data for Lacros-only extensions and extensions in both Chromes.
    auto db_map = ReadLevelDB(tmp_local_storage.Append(
        browser_data_migrator_util::kLocalStorageLeveldbName));

    EXPECT_EQ("1", db_map["VERSION"]);

    EXPECT_EQ(kLacrosLevelDBMeta, db_map[kLacrosOnlyMetaKey]);
    EXPECT_EQ(kLacrosLevelDBValue, db_map[kLacrosOnlyValueKey]);

    // If LevelDB exists in Ash, then its extension data should be present too.
    if (files_setup == FilesSetup::kBothChromes) {
      EXPECT_EQ(7u, db_map.size());

      EXPECT_EQ(kAshLevelDBMeta, db_map[kBothChromesMetaKey]);
      EXPECT_EQ(kAshLevelDBValue, db_map[kBothChromesValueKey]);

      EXPECT_EQ(kAshLevelDBMeta, db_map[kAshOnlyMetaKey]);
      EXPECT_EQ(kAshLevelDBValue, db_map[kAshOnlyValueKey]);
    } else {
      EXPECT_EQ(5u, db_map.size());

      EXPECT_EQ(kLacrosLevelDBMeta, db_map[kBothChromesMetaKey]);
      EXPECT_EQ(kLacrosLevelDBValue, db_map[kBothChromesValueKey]);
    }
  } else {
    // For FilesSetup::kAshOnly, there is no Lacros LevelDB to be used as a
    // basis for merge. Therefore both the copy and the merge step fail.
    ASSERT_FALSE(BrowserDataBackMigrator::CopyLevelDBBase(lacros_local_storage,
                                                          tmp_local_storage));
    ASSERT_FALSE(BrowserDataBackMigrator::MergeLevelDB(
        ash_local_storage.Append(
            browser_data_migrator_util::kLocalStorageLeveldbName),
        tmp_local_storage.Append(
            browser_data_migrator_util::kLocalStorageLeveldbName),
        browser_data_migrator_util::LevelDBType::kLocalStorage));
  }
}

TEST_P(BrowserDataBackMigratorFilesSetupTest, MergeStateStoreLevelDB) {
  auto files_setup = GetParam();
  SetupStateStoreLevelDBFiles(ash_profile_dir_, lacros_default_profile_dir_,
                              files_setup);
  CreateTemporaryDirectory();

  for (const char* path : browser_data_migrator_util::kStateStorePaths) {
    base::FilePath ash_path = ash_profile_dir_.Append(path);
    base::FilePath lacros_path = lacros_default_profile_dir_.Append(path);
    base::FilePath tmp_path = tmp_profile_dir_.Append(path);

    if (files_setup != FilesSetup::kAshOnly) {
      ASSERT_FALSE(base::PathExists(tmp_path));
      ASSERT_TRUE(
          BrowserDataBackMigrator::CopyLevelDBBase(lacros_path, tmp_path));
      ASSERT_TRUE(base::PathExists(tmp_path));
      ASSERT_TRUE(BrowserDataBackMigrator::MergeLevelDB(
          ash_path, tmp_path,
          browser_data_migrator_util::LevelDBType::kStateStore));

      auto db_map = ReadLevelDB(tmp_path);

      EXPECT_EQ(kLacrosLevelDBValue, db_map[kLacrosOnlyStateStoreKey]);

      if (files_setup == FilesSetup::kBothChromes) {
        EXPECT_EQ(3u, db_map.size());
        EXPECT_EQ(kAshLevelDBValue, db_map[kBothChromesStateStoreKey]);
        EXPECT_EQ(kAshLevelDBValue, db_map[kAshOnlyStateStoreKey]);
      } else {
        EXPECT_EQ(2u, db_map.size());
        EXPECT_EQ(kLacrosLevelDBValue, db_map[kBothChromesStateStoreKey]);
      }
    } else {
      ASSERT_FALSE(
          BrowserDataBackMigrator::CopyLevelDBBase(lacros_path, tmp_path));
      ASSERT_FALSE(BrowserDataBackMigrator::MergeLevelDB(
          ash_path, tmp_path,
          browser_data_migrator_util::LevelDBType::kStateStore));
    }
  }
}

TEST_F(BrowserDataBackMigratorTest,
       MergesAshOnlyPreferencesCorrectly) {
  // AshPrefs
  // {
  //   kOtherAshPreference: kAshPrefValue,
  //   browser_data_migrator_util::kAshOnlyPreferencesKeys[0]: kAshPrefValue,
  // }
  //
  // LacrosPrefs
  // {
  //   browser_data_migrator_util::kAshOnlyPreferencesKeys[0]: kLacrosPrefValue,
  // }
  base::Value::Dict ash_prefs;
  ash_prefs.SetByDottedPath(
      browser_data_migrator_util::kAshOnlyPreferencesKeys[0], kAshPrefValue);
  ash_prefs.SetByDottedPath(kOtherAshPreference, kAshPrefValue);

  base::Value::Dict lacros_prefs;
  lacros_prefs.SetByDottedPath(
      browser_data_migrator_util::kAshOnlyPreferencesKeys[0], kLacrosPrefValue);

  CreateTemporaryDirectory();

  CreateAshAndLacrosPrefs(ash_prefs, lacros_prefs);

  ASSERT_TRUE(BrowserDataBackMigrator::MergePreferences(
      ash_prefs_path_, lacros_prefs_path_, tmp_prefs_path_));

  // Expected MergedPrefs
  // {
  //   kOtherAshPreference: kAshPrefValue,
  //   browser_data_migrator_util::kAshOnlyPreferencesKeys[0]: kAshPrefValue,
  // }
  base::Value merged_prefs;
  ASSERT_TRUE(ReadJSON(tmp_prefs_path_, &merged_prefs));

  const base::Value* merged_ash_pref = merged_prefs.GetDict().FindByDottedPath(
      browser_data_migrator_util::kAshOnlyPreferencesKeys[0]);
  ASSERT_TRUE(merged_ash_pref);
  ASSERT_EQ(merged_ash_pref->GetInt(), kAshPrefValue);
  const base::Value* merged_other_ash_pref =
      merged_prefs.GetDict().FindByDottedPath(kOtherAshPreference);
  ASSERT_TRUE(merged_other_ash_pref);
  ASSERT_EQ(merged_other_ash_pref->GetInt(), kAshPrefValue);
}

TEST_F(BrowserDataBackMigratorTest,
       MergesDictSplitPreferencesCorrectly) {
  // AshPrefs
  // {
  //   browser_data_migrator_util::kSplitPreferencesKeys[0]: {
  //    browser_data_migrator_util::kExtensionsAshOnly[0]: kAshPrefValue,
  //    browser_data_migrator_util::kExtensionsBothChromes[0]: kAshPrefValue,
  //    kLacrosOnlyExtensionId: kAshPrefValue
  //   }
  // }
  //
  // LacrosPrefs
  // {
  //   browser_data_migrator_util::kSplitPreferencesKeys[0]: {
  //    browser_data_migrator_util::kExtensionsAshOnly[0]: kLacrosPrefValue,
  //    browser_data_migrator_util::kExtensionsBothChromes[0]: kLacrosPrefValue,
  //    kLacrosOnlyExtensionId: kLacrosPrefValue
  //   }
  // }
  base::Value::Dict ash_prefs;
  base::Value::Dict ash_split_pref_dict;
  ash_split_pref_dict.SetByDottedPath(
      browser_data_migrator_util::kExtensionsAshOnly[0], kAshPrefValue);
  ash_split_pref_dict.SetByDottedPath(GetBothChromesExtensionId(),
                                      kAshPrefValue);
  ash_split_pref_dict.SetByDottedPath(kLacrosOnlyExtensionId, kAshPrefValue);
  ash_prefs.SetByDottedPath(
      browser_data_migrator_util::kSplitPreferencesKeys[0],
      base::Value(std::move(ash_split_pref_dict)));

  base::Value::Dict lacros_prefs;
  base::Value::Dict lacros_split_pref_dict;
  lacros_split_pref_dict.SetByDottedPath(
      browser_data_migrator_util::kExtensionsAshOnly[0], kLacrosPrefValue);
  lacros_split_pref_dict.SetByDottedPath(GetBothChromesExtensionId(),
                                         kLacrosPrefValue);
  lacros_split_pref_dict.SetByDottedPath(kLacrosOnlyExtensionId,
                                         kLacrosPrefValue);
  lacros_prefs.SetByDottedPath(
      browser_data_migrator_util::kSplitPreferencesKeys[0],
      base::Value(std::move(lacros_split_pref_dict)));

  CreateTemporaryDirectory();

  CreateAshAndLacrosPrefs(ash_prefs, lacros_prefs);

  ASSERT_TRUE(BrowserDataBackMigrator::MergePreferences(
      ash_prefs_path_, lacros_prefs_path_, tmp_prefs_path_));

  // Expected MergedPrefs
  // {
  //   browser_data_migrator_util::kSplitPreferencesKeys[0]: {
  //    browser_data_migrator_util::kExtensionsAshOnly[0]: kAshPrefValue,
  //    browser_data_migrator_util::kExtensionsBothChromes[0]: kAshPrefValue,
  //    kLacrosOnlyExtensionId: kLacrosPrefValue
  //   }
  // }
  base::Value merged_prefs;
  ASSERT_TRUE(ReadJSON(tmp_prefs_path_, &merged_prefs));

  const base::Value* split_pref = merged_prefs.GetDict().FindByDottedPath(
      browser_data_migrator_util::kSplitPreferencesKeys[0]);
  ASSERT_TRUE(split_pref);
  const base::Value::Dict* split_pref_dict = &split_pref->GetDict();
  const base::Value* ash_extension_value = split_pref_dict->FindByDottedPath(
      browser_data_migrator_util::kExtensionsAshOnly[0]);
  ASSERT_TRUE(ash_extension_value);
  ASSERT_EQ(ash_extension_value->GetInt(), kAshPrefValue);
  const base::Value* common_extension_value =
      split_pref_dict->FindByDottedPath(GetBothChromesExtensionId());
  ASSERT_TRUE(common_extension_value);
  ASSERT_EQ(common_extension_value->GetInt(), kAshPrefValue);
  const base::Value* lacros_extension_value =
      split_pref_dict->FindByDottedPath(kLacrosOnlyExtensionId);
  ASSERT_TRUE(lacros_extension_value);
  ASSERT_EQ(lacros_extension_value->GetInt(), kLacrosPrefValue);
}

TEST_F(BrowserDataBackMigratorTest,
       MergesListSplitPreferencesCorrectly) {
  // AshPrefs
  // {
  //   browser_data_migrator_util::kSplitPreferencesKeys[0]: [
  //    browser_data_migrator_util::kExtensionsAshOnly[0],
  //    browser_data_migrator_util::kExtensionsBothChromes[0],
  //    kLacrosOnlyExtensionId
  //   ]
  // }
  //
  // LacrosPrefs
  // {
  //   browser_data_migrator_util::kSplitPreferencesKeys[0]: [
  //    browser_data_migrator_util::kExtensionsAshOnly[0],
  //    browser_data_migrator_util::kExtensionsBothChromes[0],
  //    kLacrosOnlyExtensionId
  //   ]
  // }
  base::Value::Dict ash_prefs;
  base::Value::List ash_split_pref_list;
  ash_split_pref_list.Append(browser_data_migrator_util::kExtensionsAshOnly[0]);
  ash_split_pref_list.Append(GetBothChromesExtensionId());
  ash_split_pref_list.Append(kLacrosOnlyExtensionId);
  ash_prefs.SetByDottedPath(
      browser_data_migrator_util::kSplitPreferencesKeys[0],
      base::Value(std::move(ash_split_pref_list)));

  base::Value::Dict lacros_prefs;
  base::Value::List lacros_split_pref_list;
  lacros_split_pref_list.Append(kLacrosOnlyExtensionId);
  lacros_prefs.SetByDottedPath(
      browser_data_migrator_util::kSplitPreferencesKeys[0],
      base::Value(std::move(lacros_split_pref_list)));

  CreateTemporaryDirectory();

  CreateAshAndLacrosPrefs(ash_prefs, lacros_prefs);

  ASSERT_TRUE(BrowserDataBackMigrator::MergePreferences(
      ash_prefs_path_, lacros_prefs_path_, tmp_prefs_path_));

  // Expected MergedPrefs
  // {
  //   browser_data_migrator_util::kSplitPreferencesKeys[0]: [
  //    browser_data_migrator_util::kExtensionsAshOnly[0],
  //    browser_data_migrator_util::kExtensionsBothChromes[0],
  //    kLacrosOnlyExtensionId
  //   ]
  // }
  base::Value merged_prefs;
  ASSERT_TRUE(ReadJSON(tmp_prefs_path_, &merged_prefs));

  const base::Value* split_pref = merged_prefs.GetDict().FindByDottedPath(
      browser_data_migrator_util::kSplitPreferencesKeys[0]);
  ASSERT_TRUE(split_pref);
  const base::Value::List* split_pref_list = &split_pref->GetList();
  ASSERT_EQ(
      CountStringInList(*split_pref_list,
                        browser_data_migrator_util::kExtensionsAshOnly[0]),
      1u);
  ASSERT_EQ(CountStringInList(*split_pref_list,
                              std::string(GetBothChromesExtensionId())),
            1u);
  ASSERT_EQ(CountStringInList(*split_pref_list, kLacrosOnlyExtensionId), 1u);
}

TEST_F(BrowserDataBackMigratorTest,
       MergesLacrosPreferencesCorrectly) {
  // AshPrefs
  // {
  //   kOtherAshPreference: kAshPrefValue,
  //   browser_data_migrator_util::kLacrosOnlyPreferencesKeys[0]: kAshPrefValue,
  //   kOtherBothChromesPreference: kAshPrefValue,
  // }
  //
  // LacrosPrefs
  // {
  //   kOtherBothChromesPreference: kLacrosPrefValue,
  //   browser_data_migrator_util::kAshOnlyPreferencesKeys[0]: kLacrosPrefValue,
  //   kOtherLacrosPreference: kLacrosPrefValue,
  // }
  base::Value::Dict ash_prefs;
  ash_prefs.SetByDottedPath(
      browser_data_migrator_util::kLacrosOnlyPreferencesKeys[0], kAshPrefValue);
  ash_prefs.SetByDottedPath(kOtherAshPreference, kAshPrefValue);
  ash_prefs.SetByDottedPath(kOtherBothChromesPreference, kAshPrefValue);

  base::Value::Dict lacros_prefs;
  lacros_prefs.SetByDottedPath(
      browser_data_migrator_util::kLacrosOnlyPreferencesKeys[0],
      kLacrosPrefValue);
  lacros_prefs.SetByDottedPath(kOtherBothChromesPreference, kLacrosPrefValue);
  lacros_prefs.SetByDottedPath(kOtherLacrosPreference, kLacrosPrefValue);

  CreateTemporaryDirectory();

  CreateAshAndLacrosPrefs(ash_prefs, lacros_prefs);

  ASSERT_TRUE(BrowserDataBackMigrator::MergePreferences(
      ash_prefs_path_, lacros_prefs_path_, tmp_prefs_path_));

  // Expected MergedPrefs
  // {
  //   kOtherAshPreference: kAshPrefValue,
  //   browser_data_migrator_util::kLacrosOnlyPreferencesKeys[0]: kLacrosPrefValue,
  //   kOtherBothChromesPreference: kLacrosPrefValue,
  //   kOtherLacrosPreference: kLacrosPrefValue,
  // }
  base::Value merged_prefs;
  ASSERT_TRUE(ReadJSON(tmp_prefs_path_, &merged_prefs));

  const base::Value* lacros_preference =
      merged_prefs.GetDict().FindByDottedPath(
          browser_data_migrator_util::kLacrosOnlyPreferencesKeys[0]);
  ASSERT_TRUE(lacros_preference);
  ASSERT_EQ(lacros_preference->GetInt(), kLacrosPrefValue);
  const base::Value* other_ash_preference =
      merged_prefs.GetDict().FindByDottedPath(kOtherAshPreference);
  ASSERT_TRUE(other_ash_preference);
  ASSERT_EQ(other_ash_preference->GetInt(), kAshPrefValue);
  const base::Value* other_common_preference =
      merged_prefs.GetDict().FindByDottedPath(kOtherBothChromesPreference);
  ASSERT_TRUE(other_common_preference);
  ASSERT_EQ(other_common_preference->GetInt(), kLacrosPrefValue);
  const base::Value* other_lacros_preference =
      merged_prefs.GetDict().FindByDottedPath(kOtherLacrosPreference);
  ASSERT_TRUE(other_lacros_preference);
  ASSERT_EQ(other_lacros_preference->GetInt(), kLacrosPrefValue);
}

TEST_F(BrowserDataBackMigratorTest, MergesDictWithKeysContainingDot) {
  // AshPrefs
  // {}
  //
  // LacrosPrefs
  //   "foo": {
  //     "bar": {
  //       "baz": {
  //         "https://www.google.com/": kLacrosPrefValue
  //       }
  //     }
  //   }
  base::Value::Dict ash_prefs;

  base::Value::Dict lacros_prefs;
  base::Value::Dict nested_dict;
  nested_dict.Set("http://www.google.com", kLacrosPrefValue);
  lacros_prefs.SetByDottedPath("foo.bar.baz", std::move(nested_dict));

  CreateTemporaryDirectory();

  CreateAshAndLacrosPrefs(ash_prefs, lacros_prefs);

  ASSERT_TRUE(BrowserDataBackMigrator::MergePreferences(
      ash_prefs_path_, lacros_prefs_path_, tmp_prefs_path_));

  // Expected
  //   "foo": {
  //     "bar": {
  //       "baz": {
  //         "https://www.google.com/": kLacrosPrefValue
  //       }
  //     }
  //   }
  base::Value merged_prefs;
  ASSERT_TRUE(ReadJSON(tmp_prefs_path_, &merged_prefs));

  ASSERT_TRUE(merged_prefs.is_dict());
  EXPECT_EQ(merged_prefs.GetDict(), lacros_prefs);
}

// Checks that upon canceling migration, the temporary directory Lacros user
//  directory and the are deleted.
TEST_F(BrowserDataBackMigratorTest, CancelMigration) {
  base::test::TaskEnvironment task_environment;
  const std::string user_id_hash = "abcd";

  CreateTemporaryDirectory();
  CreateLacrosDirectory();

  EXPECT_TRUE(base::PathExists(tmp_profile_dir_));
  EXPECT_TRUE(base::PathExists(lacros_default_profile_dir_));

  std::unique_ptr<BrowserDataBackMigrator> migrator =
      std::make_unique<BrowserDataBackMigrator>(ash_profile_dir_, user_id_hash,
                                                &pref_service_);

  base::test::TestFuture<void> cancellation_completed;
  migrator->CancelMigration(cancellation_completed.GetCallback());
  ASSERT_TRUE(cancellation_completed.Wait());

  EXPECT_FALSE(base::PathExists(tmp_profile_dir_));
  EXPECT_FALSE(base::PathExists(lacros_default_profile_dir_));
}

TEST_P(BrowserDataBackMigratorFilesSetupTest,
       DeletesLacrosItemsFromAshDirCorrectly) {
  auto files_setup = GetParam();
  SetUpExtensions(ash_profile_dir_, lacros_default_profile_dir_, files_setup);
  SetupLocalStorageLevelDBFiles(ash_profile_dir_, lacros_default_profile_dir_,
                                files_setup);
  EXPECT_TRUE(base::WriteFile(ash_profile_dir_.Append("README"), ""));

  auto result = BrowserDataBackMigrator::DeleteAshItems(ash_profile_dir_);

  ASSERT_EQ(result.status, BrowserDataBackMigrator::TaskStatus::kSucceeded);
  EXPECT_FALSE(base::PathExists(ash_profile_dir_.Append(
      browser_data_migrator_util::kExtensionsFilePath)));
  EXPECT_FALSE(base::PathExists(ash_profile_dir_.Append(
      browser_data_migrator_util::kLocalStorageFilePath)));
  EXPECT_TRUE(base::PathExists(ash_profile_dir_.Append("README")));
}

TEST_F(BrowserDataBackMigratorFilesSetupTest,
       MovesLacrosItemsToAshDirCorrectly) {
  SetUpExtensions(ash_profile_dir_, lacros_default_profile_dir_,
                  FilesSetup::kLacrosOnly);

  auto result =
      BrowserDataBackMigrator::MoveLacrosItemsToAshDir(ash_profile_dir_);

  ASSERT_EQ(result.status, BrowserDataBackMigrator::TaskStatus::kSucceeded);
  EXPECT_TRUE(base::PathExists(ash_profile_dir_.Append(
      browser_data_migrator_util::kExtensionsFilePath)));
  EXPECT_TRUE(base::PathExists(
      ash_profile_dir_.Append(browser_data_migrator_util::kExtensionsFilePath)
          .Append(kLacrosOnlyExtensionId)));
  EXPECT_FALSE(base::PathExists(lacros_default_profile_dir_.Append(
      browser_data_migrator_util::kExtensionsFilePath)));
}

namespace {

// This implementation of RAII for LacrosDataBackwardMigrationMode is intended
// to make it easy reset the state between runs.
class ScopedLacrosDataBackwardMigrationModeCache {
 public:
  explicit ScopedLacrosDataBackwardMigrationModeCache(
      crosapi::browser_util::LacrosDataBackwardMigrationMode mode) {
    SetLacrosDataBackwardMigrationMode(mode);
  }
  ScopedLacrosDataBackwardMigrationModeCache(
      const ScopedLacrosDataBackwardMigrationModeCache&) = delete;
  ScopedLacrosDataBackwardMigrationModeCache& operator=(
      const ScopedLacrosDataBackwardMigrationModeCache&) = delete;
  ~ScopedLacrosDataBackwardMigrationModeCache() {
    crosapi::browser_util::ClearLacrosDataBackwardMigrationModeCacheForTest();
  }

 private:
  void SetLacrosDataBackwardMigrationMode(
      crosapi::browser_util::LacrosDataBackwardMigrationMode mode) {
    policy::PolicyMap policy;
    policy.Set(policy::key::kLacrosDataBackwardMigrationMode,
               policy::POLICY_LEVEL_MANDATORY, policy::POLICY_SCOPE_USER,
               policy::POLICY_SOURCE_CLOUD,
               base::Value(GetLacrosDataBackwardMigrationModeName(mode)),
               /*external_data_fetcher=*/nullptr);
    crosapi::browser_util::CacheLacrosDataBackwardMigrationMode(policy);
  }
};

// This implementation of RAII for the backward migration flag to make it easy
// to reset state between tests.
class ScopedLacrosDataBackwardMigrationModeCommandLine {
 public:
  explicit ScopedLacrosDataBackwardMigrationModeCommandLine(
      crosapi::browser_util::LacrosDataBackwardMigrationMode mode) {
    base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
    cmdline->AppendSwitchASCII(
        crosapi::browser_util::kLacrosDataBackwardMigrationModePolicySwitch,
        GetLacrosDataBackwardMigrationModeName(mode));
  }
  ScopedLacrosDataBackwardMigrationModeCommandLine(
      const ScopedLacrosDataBackwardMigrationModeCommandLine&) = delete;
  ScopedLacrosDataBackwardMigrationModeCommandLine& operator=(
      const ScopedLacrosDataBackwardMigrationModeCommandLine&) = delete;
  ~ScopedLacrosDataBackwardMigrationModeCommandLine() {
    base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
    cmdline->RemoveSwitch(
        crosapi::browser_util::kLacrosDataBackwardMigrationModePolicySwitch);
  }
};

}  // namespace

class BrowserDataBackMigratorTriggeringTest : public testing::Test {
 public:
  void SetUp() override {
    scoped_disabled_feature.InitAndDisableFeature(
        ash::features::kLacrosProfileBackwardMigration);
  }

 private:
  base::test::ScopedFeatureList scoped_disabled_feature;
};

TEST_F(BrowserDataBackMigratorTriggeringTest, DefaultDisabledBeforeInit) {
  EXPECT_FALSE(BrowserDataBackMigrator::IsBackMigrationEnabled(
      crosapi::browser_util::PolicyInitState::kBeforeInit));
}

TEST_F(BrowserDataBackMigratorTriggeringTest, DefaultDisabledAfterInit) {
  EXPECT_FALSE(BrowserDataBackMigrator::IsBackMigrationEnabled(
      crosapi::browser_util::PolicyInitState::kAfterInit));
}

TEST_F(BrowserDataBackMigratorTriggeringTest, FeatureEnabledBeforeInit) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      ash::features::kLacrosProfileBackwardMigration);

  EXPECT_TRUE(BrowserDataBackMigrator::IsBackMigrationEnabled(
      crosapi::browser_util::PolicyInitState::kAfterInit));
}

TEST_F(BrowserDataBackMigratorTriggeringTest, FeatureEnabledAfterInit) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      ash::features::kLacrosProfileBackwardMigration);

  EXPECT_TRUE(BrowserDataBackMigrator::IsBackMigrationEnabled(
      crosapi::browser_util::PolicyInitState::kAfterInit));
}

TEST_F(BrowserDataBackMigratorTriggeringTest, PolicyEnabledBeforeInit) {
  // Simulate the flag being set by session_manager.
  ScopedLacrosDataBackwardMigrationModeCommandLine scoped_cmdline(
      crosapi::browser_util::LacrosDataBackwardMigrationMode::kKeepAll);

  EXPECT_TRUE(BrowserDataBackMigrator::IsBackMigrationEnabled(
      crosapi::browser_util::PolicyInitState::kBeforeInit));
}

TEST_F(BrowserDataBackMigratorTriggeringTest, PolicyEnabledAfterInit) {
  ScopedLacrosDataBackwardMigrationModeCache scoped_policy(
      crosapi::browser_util::LacrosDataBackwardMigrationMode::kKeepAll);

  EXPECT_TRUE(BrowserDataBackMigrator::IsBackMigrationEnabled(
      crosapi::browser_util::PolicyInitState::kAfterInit));
}

class BrowserDataBackMigratorShouldMigrateBackTest : public testing::Test {
 public:
  BrowserDataBackMigratorShouldMigrateBackTest() = default;
  ~BrowserDataBackMigratorShouldMigrateBackTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(base::PathService::Override(chrome::DIR_USER_DATA,
                                            user_data_dir_.GetPath()));
    fake_user_manager_.Reset(std::make_unique<ash::FakeChromeUserManager>());
  }

  void TearDown() override { EXPECT_TRUE(user_data_dir_.Delete()); }

  void AddRegularUser(const std::string& email) {
    AccountId account_id = AccountId::FromUserEmail(email);
    const user_manager::User* user = fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);

    // Create the Lacros directory when a user is added. All checks rely on this
    // directory being present, so creating it puts focus on other conditions.
    ash_profile_dir_ = user_data_dir_.GetPath().Append(
        ProfileHelper::GetUserProfileDir(user->username_hash()));
    ASSERT_TRUE(base::CreateDirectory(ash_profile_dir_));
    lacros_dir_ =
        ash_profile_dir_.Append(browser_data_migrator_util::kLacrosDir);
    ASSERT_TRUE(base::CreateDirectory(lacros_dir_));
  }

 protected:
  ash::FakeChromeUserManager* user_manager() {
    return fake_user_manager_.Get();
  }

 private:
  base::ScopedTempDir user_data_dir_;
  base::FilePath ash_profile_dir_;
  base::FilePath lacros_dir_;

  ScopedTestingLocalState scoped_local_state_{
      TestingBrowserProcess::GetGlobal()};
  content::BrowserTaskEnvironment task_environment_;
  user_manager::TypedScopedUserManager<ash::FakeChromeUserManager>
      fake_user_manager_;
};

TEST_F(BrowserDataBackMigratorShouldMigrateBackTest,
       CommandLineForceMigration) {
  AddRegularUser(kPrimaryUser);
  const user_manager::User* const user = user_manager()->GetPrimaryUser();

  {
    base::test::ScopedCommandLine command_line;
    command_line.GetProcessCommandLine()->AppendSwitchASCII(
        switches::kForceBrowserDataBackwardMigration, "force-migration");
    EXPECT_TRUE(BrowserDataBackMigrator::ShouldMigrateBack(
        user->GetAccountId(), user->username_hash(),
        crosapi::browser_util::PolicyInitState::kAfterInit));
  }
}

TEST_F(BrowserDataBackMigratorShouldMigrateBackTest, CommandLineForceSkip) {
  AddRegularUser(kPrimaryUser);
  const user_manager::User* const user = user_manager()->GetPrimaryUser();

  {
    base::test::ScopedCommandLine command_line;
    command_line.GetProcessCommandLine()->AppendSwitchASCII(
        switches::kForceBrowserDataBackwardMigration, "force-skip");
    EXPECT_FALSE(BrowserDataBackMigrator::ShouldMigrateBack(
        user->GetAccountId(), user->username_hash(),
        crosapi::browser_util::PolicyInitState::kAfterInit));
  }
}

TEST_F(BrowserDataBackMigratorShouldMigrateBackTest,
       MaybeRestartToMigrateSecondaryUser) {
  // Add two users to simulate multi user session.
  AddRegularUser(kPrimaryUser);
  AddRegularUser(kSecondaryUser);
  const auto* const primary_user = user_manager()->GetPrimaryUser();
  const auto* const secondary_user =
      user_manager()->FindUser(AccountId::FromUserEmail(kSecondaryUser));
  EXPECT_NE(primary_user, secondary_user);

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {ash::features::kLacrosProfileBackwardMigration}, {});
    // Migration should be triggered for the primary user.
    EXPECT_TRUE(BrowserDataBackMigrator::ShouldMigrateBack(
        primary_user->GetAccountId(), primary_user->username_hash(),
        crosapi::browser_util::PolicyInitState::kAfterInit));
    // But not for secondary users.
    EXPECT_FALSE(BrowserDataBackMigrator::ShouldMigrateBack(
        secondary_user->GetAccountId(), secondary_user->username_hash(),
        crosapi::browser_util::PolicyInitState::kAfterInit));
  }
}

}  // namespace ash
