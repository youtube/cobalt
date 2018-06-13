// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "cobalt/storage/store/memory_store.h"

#include "base/debug/trace_event.h"
#include "cobalt/storage/storage_constants.h"
#include "cobalt/storage/store/storage.pb.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace storage {

namespace {

class MemoryStoreTest : public ::testing::Test {
 protected:
  MemoryStoreTest() : id1_("a_site_id1"), id2_("a_site_id2") {
    base::Time current_time = base::Time::FromInternalValue(12345);
    expiration_time_ = current_time + base::TimeDelta::FromDays(1);

    Cookie* cookie = storage_proto_.add_cookies();
    cookie->set_name("name");
    cookie->set_value("value");
    cookie->set_domain("domain");
    cookie->set_path("/path/foo");
    cookie->set_creation_time_us(current_time.ToInternalValue());
    cookie->set_expiration_time_us(expiration_time_.ToInternalValue());
    cookie->set_last_access_time_us(current_time.ToInternalValue());
    cookie->set_secure(true);
    cookie->set_http_only(true);

    size_t size = storage_proto_.ByteSize();
    storage_data_.resize(size + kStorageHeaderSize);
    memcpy(reinterpret_cast<char*>(&storage_data_[0]), kStorageHeader,
           kStorageHeaderSize);
    storage_proto_.SerializeToArray(
        reinterpret_cast<char*>(&storage_data_[kStorageHeaderSize]), size);

    memory_store_.Initialize(storage_data_);

    cookie_.reset(new net::CanonicalCookie(
        GURL(""), "name", "value", "domain", "/path/foo", "" /* mac_key */,
        "" /* mac_algorithm */, current_time, expiration_time_, current_time,
        true /* secure */, true /* http_only */));

    last_access_time_ = current_time + base::TimeDelta::FromDays(50);

    updated_cookie_.reset(new net::CanonicalCookie(
        GURL(""), "name", "value", "domain", "/path/foo", "" /* mac_key */,
        "" /* mac_algorithm */, current_time, expiration_time_, current_time,
        true /* secure */, true /* http_only */));

    new_cookie_.reset(new net::CanonicalCookie(
        GURL(""), "name1", "value2", "domain2", "/path/foo2", "" /* mac_key */,
        "" /* mac_algorithm */, current_time, expiration_time_, current_time,
        false /* secure */, false /* http_only */));
  }
  ~MemoryStoreTest() {}

  std::string id1_;
  std::string id2_;
  Storage storage_proto_;
  std::vector<uint8> storage_data_;
  scoped_ptr<net::CanonicalCookie> cookie_;
  scoped_ptr<net::CanonicalCookie> updated_cookie_;
  scoped_ptr<net::CanonicalCookie> new_cookie_;
  base::Time last_access_time_;
  base::Time expiration_time_;
  MemoryStore memory_store_;
};

TEST_F(MemoryStoreTest, Initialize) {
  EXPECT_TRUE(memory_store_.Initialize(storage_data_));
}

TEST_F(MemoryStoreTest, Serialize) {
  std::vector<uint8> out;
  memory_store_.Serialize(&out);
  EXPECT_EQ(storage_data_, out);
}

TEST_F(MemoryStoreTest, GetAllCookies) {
  std::vector<net::CanonicalCookie*> cookies;
  memory_store_.GetAllCookies(&cookies);

  EXPECT_EQ(cookies.size(), 1);
  EXPECT_TRUE(cookies[0]->IsEquivalent(*cookie_));
  for (auto* cookie : cookies) {
    delete cookie;
  }
}

TEST_F(MemoryStoreTest, AddCookie) {
  std::vector<net::CanonicalCookie*> cookies;
  memory_store_.GetAllCookies(&cookies);

  EXPECT_EQ(cookies.size(), 1);
  EXPECT_TRUE(cookies[0]->IsEquivalent(*cookie_));
  for (auto* cookie : cookies) {
    delete cookie;
  }
  cookies.clear();

  memory_store_.AddCookie(*new_cookie_, expiration_time_.ToInternalValue());

  memory_store_.GetAllCookies(&cookies);
  EXPECT_EQ(cookies.size(), 2);
  EXPECT_TRUE(cookies[0]->IsEquivalent(*cookie_));
  EXPECT_TRUE(cookies[1]->IsEquivalent(*new_cookie_));
  for (auto* cookie : cookies) {
    delete cookie;
  }
  cookies.clear();
}

TEST_F(MemoryStoreTest, UpdateCookieAccessTime) {
  memory_store_.UpdateCookieAccessTime(*cookie_,
                                       last_access_time_.ToInternalValue());
  std::vector<net::CanonicalCookie*> cookies;
  memory_store_.GetAllCookies(&cookies);

  EXPECT_EQ(cookies.size(), 1);

  EXPECT_TRUE(cookies[0]->IsEquivalent(*updated_cookie_));
  for (auto* cookie : cookies) {
    delete cookie;
  }
}

TEST_F(MemoryStoreTest, DeleteCookie) {
  std::vector<net::CanonicalCookie*> cookies;
  memory_store_.GetAllCookies(&cookies);
  EXPECT_EQ(cookies.size(), 1);
  for (auto* cookie : cookies) {
    delete cookie;
  }
  cookies.clear();

  memory_store_.DeleteCookie(*cookie_);
  memory_store_.GetAllCookies(&cookies);

  EXPECT_TRUE(cookies.empty());
}

TEST_F(MemoryStoreTest, ReadWriteToLocalStorage) {
  MemoryStore::LocalStorageMap test_vals;
  test_vals["key0"] = "value0";
  test_vals["key1"] = "value1";

  for (const auto& it : test_vals) {
    memory_store_.WriteToLocalStorage(id1_, it.first, it.second);
  }

  memory_store_.WriteToLocalStorage(id2_, "key0", "value0");
  MemoryStore::LocalStorageMap read_vals;
  memory_store_.ReadAllLocalStorage(id1_, &read_vals);
  EXPECT_EQ(test_vals, read_vals);
}

TEST_F(MemoryStoreTest, DeleteFromLocalStorage) {
  MemoryStore::LocalStorageMap test_vals;
  test_vals["key0"] = "value0";
  test_vals["key1"] = "value1";

  for (const auto& it : test_vals) {
    memory_store_.WriteToLocalStorage(id1_, it.first, it.second);
  }

  memory_store_.DeleteFromLocalStorage(id1_, "key0");
  test_vals.erase("key0");

  MemoryStore::LocalStorageMap read_vals;
  memory_store_.ReadAllLocalStorage(id1_, &read_vals);

  EXPECT_EQ(test_vals, read_vals);
}

TEST_F(MemoryStoreTest, ClearLocalStorage) {
  MemoryStore::LocalStorageMap test_vals;
  test_vals["key0"] = "value0";
  test_vals["key1"] = "value1";

  for (const auto& it : test_vals) {
    memory_store_.WriteToLocalStorage(id1_, it.first, it.second);
  }

  memory_store_.ClearLocalStorage(id1_);

  MemoryStore::LocalStorageMap read_vals;
  memory_store_.ReadAllLocalStorage(id1_, &read_vals);

  EXPECT_TRUE(read_vals.empty());
}

TEST_F(MemoryStoreTest, EmptyStorage) {
  MemoryStore empty_store;
  std::vector<uint8> in(kStorageHeader, kStorageHeader + kStorageHeaderSize);
  empty_store.Initialize(in);

  std::vector<uint8> out;
  empty_store.Serialize(&out);
  EXPECT_EQ(memcmp(kStorageHeader, reinterpret_cast<const char*>(&out[0]),
                   kStorageHeaderSize),
            0);
}
}  // namespace
}  // namespace storage
}  // namespace cobalt
