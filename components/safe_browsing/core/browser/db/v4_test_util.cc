// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/db/v4_test_util.h"

#include <algorithm>
#include <string>
#include <utility>

#include "base/strings/strcat.h"
#include "base/task/sequenced_task_runner.h"
#include "components/safe_browsing/core/browser/db/util.h"
#include "crypto/sha2.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace safe_browsing {

namespace {

const char kClient[] = "unittest";
const char kAppVer[] = "1.0";
const char kKeyParam[] = "test_key_param";
const int kDefaultStoreFileSizeInBytes = 320000;

}  // namespace

V4ProtocolConfig GetTestV4ProtocolConfig(bool disable_auto_update) {
  return V4ProtocolConfig(kClient, disable_auto_update, kKeyParam, kAppVer);
}

std::ostream& operator<<(std::ostream& os, const ThreatMetadata& meta) {
  os << "{threat_pattern_type=" << static_cast<int>(meta.threat_pattern_type)
     << ", api_permissions=[";
  for (auto p : meta.api_permissions)
    os << p << ",";
  os << "subresource_filter_match=[";
  for (auto t : meta.subresource_filter_match)
    os << static_cast<int>(t.first) << ":" << static_cast<int>(t.second) << ",";
  return os << "], population_id=" << meta.population_id << "}";
}

TestV4Store::TestV4Store(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::FilePath& store_path)
    : V4Store(task_runner,
              store_path,
              std::make_unique<InMemoryHashPrefixMap>()) {}

TestV4Store::~TestV4Store() = default;

bool TestV4Store::HasValidData() {
  return true;
}

void TestV4Store::MarkPrefixAsBad(HashPrefixStr prefix) {
  auto& vec = mock_prefixes_[prefix.size()];
  vec.insert(std::upper_bound(vec.begin(), vec.end(), prefix), prefix);
  hash_prefix_map_->Clear();
  hash_prefix_map_->Append(prefix.size(), base::StrCat(vec));
}

void TestV4Store::SetPrefixes(std::vector<HashPrefixStr> prefixes,
                              PrefixSize size) {
  std::sort(prefixes.begin(), prefixes.end());
  mock_prefixes_[size] = prefixes;
  hash_prefix_map_->Clear();
  hash_prefix_map_->Append(size, base::StrCat(prefixes));
}

TestV4Database::TestV4Database(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    std::unique_ptr<StoreMap> store_map)
    : V4Database(db_task_runner, std::move(store_map)) {}

void TestV4Database::MarkPrefixAsBad(ListIdentifier list_id,
                                     HashPrefixStr prefix) {
  V4Store* base_store = store_map_->at(list_id).get();
  TestV4Store* test_store = static_cast<TestV4Store*>(base_store);
  test_store->MarkPrefixAsBad(prefix);
}

int64_t TestV4Database::GetStoreSizeInBytes(const ListIdentifier& store) const {
  return kDefaultStoreFileSizeInBytes;
}

TestV4StoreFactory::TestV4StoreFactory() = default;

TestV4StoreFactory::~TestV4StoreFactory() = default;

std::unique_ptr<V4Store> TestV4StoreFactory::CreateV4Store(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::FilePath& store_path) {
  auto new_store = std::make_unique<TestV4Store>(task_runner, store_path);
  new_store->Initialize();
  return std::move(new_store);
}

TestV4DatabaseFactory::TestV4DatabaseFactory() = default;

TestV4DatabaseFactory::~TestV4DatabaseFactory() = default;

std::unique_ptr<V4Database, base::OnTaskRunnerDeleter>
TestV4DatabaseFactory::Create(
    const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
    std::unique_ptr<StoreMap> store_map) {
  auto v4_db = std::unique_ptr<TestV4Database, base::OnTaskRunnerDeleter>(
      new TestV4Database(db_task_runner, std::move(store_map)),
      base::OnTaskRunnerDeleter(db_task_runner));
  v4_db_ = v4_db.get();
  return std::move(v4_db);
}

void TestV4DatabaseFactory::MarkPrefixAsBad(ListIdentifier list_id,
                                            HashPrefixStr prefix) {
  v4_db_->MarkPrefixAsBad(list_id, prefix);
}

TestV4GetHashProtocolManager::TestV4GetHashProtocolManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const StoresToCheck& stores_to_check,
    const V4ProtocolConfig& config)
    : V4GetHashProtocolManager(url_loader_factory, stores_to_check, config) {}

void TestV4GetHashProtocolManager::AddToFullHashCache(FullHashInfo fhi) {
  full_hash_cache_[fhi.full_hash].full_hash_infos.push_back(fhi);
}

TestV4GetHashProtocolManagerFactory::TestV4GetHashProtocolManagerFactory() =
    default;

TestV4GetHashProtocolManagerFactory::~TestV4GetHashProtocolManagerFactory() =
    default;

std::unique_ptr<V4GetHashProtocolManager>
TestV4GetHashProtocolManagerFactory::CreateProtocolManager(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    const StoresToCheck& stores_to_check,
    const V4ProtocolConfig& config) {
  auto pm = std::make_unique<TestV4GetHashProtocolManager>(
      url_loader_factory, stores_to_check, config);
  pm_ = pm.get();
  return std::move(pm);
}

TestV4HashResponseInfo::KeyValue::KeyValue(const std::string key,
                                           const std::string value)
    : key(key), value(value) {}
TestV4HashResponseInfo::KeyValue::KeyValue(const KeyValue& other) = default;
TestV4HashResponseInfo::KeyValue::~KeyValue() = default;

TestV4HashResponseInfo::TestV4HashResponseInfo(FullHashStr full_hash,
                                               ListIdentifier list_id)
    : full_hash(full_hash), list_id(list_id) {}
TestV4HashResponseInfo::TestV4HashResponseInfo(
    const TestV4HashResponseInfo& other) = default;
TestV4HashResponseInfo::~TestV4HashResponseInfo() = default;

std::string GetV4HashResponse(
    std::vector<TestV4HashResponseInfo> response_infos) {
  FindFullHashesResponse res;
  res.mutable_negative_cache_duration()->set_seconds(600);
  for (const TestV4HashResponseInfo& info : response_infos) {
    ThreatMatch* m = res.add_matches();
    m->set_platform_type(info.list_id.platform_type());
    m->set_threat_entry_type(info.list_id.threat_entry_type());
    m->set_threat_type(info.list_id.threat_type());
    m->mutable_cache_duration()->set_seconds(300);
    m->mutable_threat()->set_hash(info.full_hash);

    for (const TestV4HashResponseInfo::KeyValue& key_value : info.key_values) {
      ThreatEntryMetadata::MetadataEntry* e =
          m->mutable_threat_entry_metadata()->add_entries();
      e->set_key(key_value.key);
      e->set_value(key_value.value);
    }
  }

  // Serialize.
  std::string res_data;
  res.SerializeToString(&res_data);

  return res_data;
}

FullHashInfo GetFullHashInfo(const GURL& url, const ListIdentifier& list_id) {
  return FullHashInfo(V4ProtocolManagerUtil::GetFullHash(url), list_id,
                      base::Time::Now() + base::Minutes(5));
}

FullHashInfo GetFullHashInfoWithMetadata(
    const GURL& url,
    const ListIdentifier& list_id,
    const ThreatMetadata& threat_metadata) {
  FullHashInfo fhi = GetFullHashInfo(url, list_id);
  fhi.metadata = threat_metadata;
  return fhi;
}

}  // namespace safe_browsing
