// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/shell/browser/migrate_storage_record/legacy_storage_writer.h"

#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_environment_variable_override.h"
#include "base/test/scoped_command_line.h"
#include "base/test/task_environment.h"
#include "cobalt/browser/switches.h"
#include "cobalt/shell/browser/migrate_storage_record/migration_manager.h"
#include "cobalt/shell/browser/migrate_storage_record/storage.pb.h"
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "content/public/test/test_storage_partition.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace cobalt {
namespace migrate_storage_record {
namespace {

constexpr char kIds[] = "yt.leanback.default::available-identities";
constexpr char kLast[] = "yt.leanback.default::last-identity-used";
constexpr char kOther[] = "yt.leanback.default::some-preference";

// Blink kLocalStorageDetectFormat encoding of a Latin-1 string.
std::vector<uint8_t> Latin1(const std::string& s) {
  std::vector<uint8_t> v{1};
  v.insert(v.end(), s.begin(), s.end());
  return v;
}

// Blink kLocalStorageDetectFormat encoding of a non-Latin-1 string.
std::vector<uint8_t> Utf16(const std::u16string& s) {
  std::vector<uint8_t> v(1 + s.size() * sizeof(char16_t));
  v[0] = 0;
  std::memcpy(v.data() + 1, s.data(), s.size() * sizeof(char16_t));
  return v;
}

// In-memory StorageArea that broadcasts mutations to its observer, like
// storage::StorageAreaImpl does for renderer Put()/Delete() calls.
class FakeStorageArea : public blink::mojom::StorageArea {
 public:
  void BindReceiver(mojo::PendingReceiver<blink::mojom::StorageArea> r) {
    receiver_.Bind(std::move(r));
  }
  mojo::PendingRemote<blink::mojom::StorageArea> BindNewRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  void AddObserver(
      mojo::PendingRemote<blink::mojom::StorageAreaObserver> o) override {
    observer_.Bind(std::move(o));
  }
  void Put(const std::vector<uint8_t>& key,
           const std::vector<uint8_t>& value,
           const std::optional<std::vector<uint8_t>>& client_old_value,
           const std::string& source,
           PutCallback callback) override {
    std::move(callback).Run(true);
  }
  void Delete(const std::vector<uint8_t>& key,
              const std::optional<std::vector<uint8_t>>& client_old_value,
              const std::string& source,
              DeleteCallback callback) override {
    std::move(callback).Run(true);
  }
  void DeleteAll(
      const std::string& source,
      mojo::PendingRemote<blink::mojom::StorageAreaObserver> new_observer,
      DeleteAllCallback callback) override {
    std::move(callback).Run(true);
  }
  void Get(const std::vector<uint8_t>& key, GetCallback callback) override {
    std::move(callback).Run(false, {});
  }
  void GetAll(mojo::PendingRemote<blink::mojom::StorageAreaObserver> o,
              GetAllCallback callback) override {
    ++get_all_count_;
    std::vector<blink::mojom::KeyValuePtr> all;
    for (const auto& [k, v] : data_) {
      all.push_back(blink::mojom::KeyValue::New(k, v));
    }
    std::move(callback).Run(std::move(all));
    if (o) {
      observer_.Bind(std::move(o));
    }
  }
  void Checkpoint() override {}

  void SimulatePut(const std::vector<uint8_t>& k,
                   const std::vector<uint8_t>& v) {
    data_[k] = v;
    observer_->KeyChanged(k, v, std::nullopt, "test");
  }
  void SimulatePut(const std::string& k, const std::string& v) {
    SimulatePut(Latin1(k), Latin1(v));
  }
  void SimulateDelete(const std::string& k) {
    data_.erase(Latin1(k));
    observer_->KeyDeleted(Latin1(k), std::nullopt, "test");
  }
  void SimulateDeleteAll() {
    data_.clear();
    observer_->AllDeleted(true, "test");
  }

  std::map<std::vector<uint8_t>, std::vector<uint8_t>> data_;
  int get_all_count_ = 0;

 private:
  mojo::Receiver<blink::mojom::StorageArea> receiver_{this};
  mojo::Remote<blink::mojom::StorageAreaObserver> observer_;
};

class FakeLocalStorageControl : public ::storage::mojom::LocalStorageControl {
 public:
  explicit FakeLocalStorageControl(FakeStorageArea* area) : area_(area) {}

  void BindStorageArea(
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::StorageArea> receiver) override {
    bound_key_ = storage_key;
    area_->BindReceiver(std::move(receiver));
  }
  void GetUsage(GetUsageCallback callback) override {}
  void DeleteStorage(const blink::StorageKey& storage_key,
                     DeleteStorageCallback callback) override {}
  void CleanUpStorage(CleanUpStorageCallback callback) override {}
  void Flush() override {}
  void PurgeMemory() override {}
  void ApplyPolicyUpdates(std::vector<::storage::mojom::StoragePolicyUpdatePtr>
                              policy_updates) override {}
  void ForceKeepSessionState() override {}
  void NeedsFlushForTesting(NeedsFlushForTestingCallback callback) override {}

  std::optional<blink::StorageKey> bound_key_;

 private:
  raw_ptr<FakeStorageArea> area_;
};

class FakeStoragePartition : public content::TestStoragePartition {
 public:
  explicit FakeStoragePartition(FakeStorageArea* area) : control_(area) {}
  ::storage::mojom::LocalStorageControl* GetLocalStorageControl() override {
    return &control_;
  }
  FakeLocalStorageControl control_;
};

base::ScopedTempDir MakeTempHome() {
  base::ScopedTempDir dir;
  CHECK(dir.CreateUniqueTempDir());
  return dir;
}

}  // namespace

class LegacyStorageWriterTest : public testing::Test {
 protected:
  // Reads ~/.starboard.<name>.storage from the fake HOME ("" if absent).
  std::string ReadRecordFile(const std::string& name) {
    std::string bytes;
    base::ReadFileToString(
        home_.GetPath().Append(".starboard." + name + ".storage"), &bytes);
    return bytes;
  }

  // Parses a record the way C25's MemoryStore::Initialize does: "SAV1" magic,
  // then cobalt.storage.Storage. Returns origin -> {key -> value}.
  std::map<std::string, std::map<std::string, std::string>> ParseRecord(
      const std::string& bytes,
      int* cookies = nullptr) {
    EXPECT_GE(bytes.size(), 4u);
    EXPECT_EQ("SAV1", bytes.substr(0, 4));
    cobalt::storage::Storage storage;
    EXPECT_TRUE(storage.ParseFromString(bytes.substr(4)));
    if (cookies) {
      *cookies = storage.cookies_size();
    }
    std::map<std::string, std::map<std::string, std::string>> out;
    for (const auto& ls : storage.local_storages()) {
      for (const auto& e : ls.local_storage_entries()) {
        out[ls.serialized_origin()][e.key()] = e.value();
      }
    }
    return out;
  }

  const url::Origin kOrigin =
      url::Origin::Create(GURL("https://127.0.0.1:8443/tv/"));

  // HOME must be overridden before the TaskEnvironment starts worker threads:
  // Starboard resolves the storage path via getenv("HOME") on every call.
  base::ScopedTempDir home_ = MakeTempHome();
  base::ScopedEnvironmentVariableOverride home_override_{
      "HOME", home_.GetPath().value()};
  base::test::TaskEnvironment env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(LegacyStorageWriterTest, DecodeLatin1) {
  EXPECT_EQ("ab", DecodeLocalStorageString({1, 'a', 'b'}));
  // Latin-1 bytes >= 0x80 must become valid UTF-8, not raw bytes.
  EXPECT_EQ("\xC3\xA9", DecodeLocalStorageString({1, 0xE9}));
  EXPECT_EQ("", DecodeLocalStorageString({1}));
}

TEST_F(LegacyStorageWriterTest, DecodeUtf16) {
  // U+20AC EURO SIGN, host (little) endian on x64.
  EXPECT_EQ("\xE2\x82\xAC", DecodeLocalStorageString({0, 0xAC, 0x20}));
  EXPECT_EQ("a\xE2\x82\xAC", DecodeLocalStorageString(Utf16(u"a\u20AC")));
}

TEST_F(LegacyStorageWriterTest, DecodeEmptyAndCorrupt) {
  EXPECT_EQ("", DecodeLocalStorageString({}));
  EXPECT_EQ(std::nullopt, DecodeLocalStorageString({0, 0x41}));  // Odd size.
  EXPECT_EQ(std::nullopt, DecodeLocalStorageString({7, 'x'}));   // Bad format.
}

TEST_F(LegacyStorageWriterTest, DecodeUnpairedSurrogateGivesValidUtf8) {
  std::optional<std::string> s =
      DecodeLocalStorageString(Utf16(std::u16string(1, char16_t{0xD800})));
  ASSERT_TRUE(s.has_value());
  EXPECT_EQ("\xEF\xBF\xBD", *s);  // U+FFFD replacement character.
}

TEST_F(LegacyStorageWriterTest, SerializeEmptyIsHeaderOnly) {
  EXPECT_EQ("SAV1", SerializeLegacyStorageRecord(kOrigin, {}));
}

TEST_F(LegacyStorageWriterTest, SerializeRoundTripsLikeC25) {
  std::map<std::string, std::string> entries = {{kIds, "{\"data\":1}"},
                                                {kLast, "{\"data\":2}"}};
  for (const char* url :
       {"https://www.youtube.com/tv", "https://127.0.0.1:8443/tv/"}) {
    url::Origin origin = url::Origin::Create(GURL(url));
    int cookies = -1;
    auto parsed =
        ParseRecord(SerializeLegacyStorageRecord(origin, entries), &cookies);
    EXPECT_EQ(0, cookies);
    ASSERT_EQ(1u, parsed.size());
    EXPECT_EQ(origin.Serialize(), parsed.begin()->first);
    EXPECT_EQ(entries, parsed.begin()->second);
  }
  EXPECT_EQ(
      "https://www.youtube.com",
      url::Origin::Create(GURL("https://www.youtube.com/tv")).Serialize());
  EXPECT_EQ("https://127.0.0.1:8443", kOrigin.Serialize());
}

TEST_F(LegacyStorageWriterTest, InitialSnapshotWritesOnlyAllowlistedKeys) {
  FakeStorageArea area;
  area.data_[Latin1(kIds)] = Latin1("ids-v1");
  area.data_[Latin1(kLast)] = Latin1("last-v1");
  area.data_[Latin1(kOther)] = Latin1("must-not-be-copied");
  area.data_[Latin1("unrelated")] = Latin1("x");

  LegacyStorageWriter writer(kOrigin, "test");
  writer.Start(area.BindNewRemote());
  env_.RunUntilIdle();

  auto parsed = ParseRecord(ReadRecordFile("test"));
  ASSERT_EQ(1u, parsed.size());
  EXPECT_EQ((std::map<std::string, std::string>{{kIds, "ids-v1"},
                                                {kLast, "last-v1"}}),
            parsed["https://127.0.0.1:8443"]);
}

TEST_F(LegacyStorageWriterTest, InitialSnapshotWithoutCredentialIsHeaderOnly) {
  FakeStorageArea area;
  area.data_[Latin1(kOther)] = Latin1("pref");

  LegacyStorageWriter writer(kOrigin, "test");
  writer.Start(area.BindNewRemote());
  env_.RunUntilIdle();

  EXPECT_EQ("SAV1", ReadRecordFile("test"));
}

TEST_F(LegacyStorageWriterTest, RewritesRecordOnCredentialChange) {
  FakeStorageArea area;
  area.data_[Latin1(kIds)] = Latin1("ids-v1");
  area.data_[Latin1(kLast)] = Latin1("last-v1");
  LegacyStorageWriter writer(kOrigin, "test");
  writer.Start(area.BindNewRemote());
  env_.RunUntilIdle();

  // A credential change is written after the coalescing delay, not before.
  area.SimulatePut(kIds, "ids-v2");
  env_.RunUntilIdle();
  EXPECT_EQ(
      "ids-v1",
      ParseRecord(ReadRecordFile("test"))["https://127.0.0.1:8443"][kIds]);
  env_.FastForwardBy(LegacyStorageWriter::kWriteDelay);
  env_.RunUntilIdle();
  EXPECT_EQ(
      "ids-v2",
      ParseRecord(ReadRecordFile("test"))["https://127.0.0.1:8443"][kIds]);

  // Non-Latin-1 values are converted to UTF-8.
  area.SimulatePut(Latin1(kLast), Utf16(u"last-\u20AC"));
  env_.FastForwardBy(LegacyStorageWriter::kWriteDelay);
  env_.RunUntilIdle();
  EXPECT_EQ(
      "last-\xE2\x82\xAC",
      ParseRecord(ReadRecordFile("test"))["https://127.0.0.1:8443"][kLast]);
}

TEST_F(LegacyStorageWriterTest, NonCredentialChangeDoesNotRewrite) {
  FakeStorageArea area;
  area.data_[Latin1(kIds)] = Latin1("ids-v1");
  LegacyStorageWriter writer(kOrigin, "test");
  writer.Start(area.BindNewRemote());
  env_.RunUntilIdle();
  const int get_all_after_start = area.get_all_count_;

  area.SimulatePut(kOther, "pref");
  env_.FastForwardBy(LegacyStorageWriter::kWriteDelay * 2);
  env_.RunUntilIdle();
  EXPECT_EQ(get_all_after_start, area.get_all_count_);
  auto parsed = ParseRecord(ReadRecordFile("test"));
  EXPECT_EQ((std::map<std::string, std::string>{{kIds, "ids-v1"}}),
            parsed["https://127.0.0.1:8443"]);
}

TEST_F(LegacyStorageWriterTest, SignOutWritesHeaderOnlyRecord) {
  FakeStorageArea area;
  area.data_[Latin1(kIds)] = Latin1("ids-v1");
  area.data_[Latin1(kLast)] = Latin1("last-v1");
  area.data_[Latin1(kOther)] = Latin1("pref");
  LegacyStorageWriter writer(kOrigin, "test");
  writer.Start(area.BindNewRemote());
  env_.RunUntilIdle();

  area.SimulateDelete(kIds);
  area.SimulateDelete(kLast);
  env_.FastForwardBy(LegacyStorageWriter::kWriteDelay);
  env_.RunUntilIdle();
  // Not deleted: a 0-byte/missing record would make C25 fall back to
  // ~/.starboard.storage.
  EXPECT_EQ("SAV1", ReadRecordFile("test"));
}

TEST_F(LegacyStorageWriterTest, ClearAllWritesHeaderOnlyRecord) {
  FakeStorageArea area;
  area.data_[Latin1(kIds)] = Latin1("ids-v1");
  LegacyStorageWriter writer(kOrigin, "test");
  writer.Start(area.BindNewRemote());
  env_.RunUntilIdle();

  area.SimulateDeleteAll();
  env_.FastForwardBy(LegacyStorageWriter::kWriteDelay);
  env_.RunUntilIdle();
  EXPECT_EQ("SAV1", ReadRecordFile("test"));
}

TEST_F(LegacyStorageWriterTest, DestructorFlushesPendingChange) {
  FakeStorageArea area;
  area.data_[Latin1(kIds)] = Latin1("ids-v1");
  auto writer = std::make_unique<LegacyStorageWriter>(kOrigin, "test");
  writer->Start(area.BindNewRemote());
  env_.RunUntilIdle();

  area.SimulatePut(kIds, "ids-v2");
  env_.RunUntilIdle();  // Event delivered; the write timer is still pending.
  writer.reset();
  env_.RunUntilIdle();
  EXPECT_EQ(
      "ids-v2",
      ParseRecord(ReadRecordFile("test"))["https://127.0.0.1:8443"][kIds]);
}

TEST_F(LegacyStorageWriterTest, CreateForInitialUrlDerivesOriginAndRecordName) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      switches::kInitialURL, "https://127.0.0.1:8443/tv/?mode=check#frag");
  FakeStorageArea area;
  area.data_[Latin1(kIds)] = Latin1("ids-v1");
  area.data_[Latin1(kLast)] = Latin1("last-v1");
  FakeStoragePartition partition(&area);

  auto writer = LegacyStorageWriter::CreateForInitialUrl(&partition);
  ASSERT_TRUE(writer);
  env_.RunUntilIdle();

  ASSERT_TRUE(partition.control_.bound_key_.has_value());
  EXPECT_EQ(blink::StorageKey::CreateFirstParty(kOrigin),
            *partition.control_.bound_key_);
  // Record name = C25 GetApplicationKey:
  // base64url("https://127.0.0.1:8443/tv/") with query and ref stripped.
  const std::string name = "aHR0cHM6Ly8xMjcuMC4wLjE6ODQ0My90di8=";
  EXPECT_EQ(name, GetApplicationKey(GURL("https://127.0.0.1:8443/tv/?x=1#y")));
  auto parsed = ParseRecord(ReadRecordFile(name));
  EXPECT_EQ((std::map<std::string, std::string>{{kIds, "ids-v1"},
                                                {kLast, "last-v1"}}),
            parsed["https://127.0.0.1:8443"]);
}

TEST_F(LegacyStorageWriterTest, CreateForInitialUrlRejectsInvalidUrl) {
  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(switches::kInitialURL,
                                                          "not a url");
  FakeStorageArea area;
  FakeStoragePartition partition(&area);
  EXPECT_FALSE(LegacyStorageWriter::CreateForInitialUrl(&partition));
  EXPECT_FALSE(LegacyStorageWriter::CreateForInitialUrl(nullptr));
}

}  // namespace migrate_storage_record
}  // namespace cobalt
