// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/storage_area.h"

#include <set>
#include <vector>

#include "cobalt/dom/storage.h"
#include "cobalt/dom/global_stats.h"
#include "cobalt/storage/store/memory_store.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace cobalt {
namespace dom {

namespace {
class MockStorage : public Storage {
 public:
  MockStorage() : Storage(NULL, kSessionStorage, NULL) {
      ON_CALL(*this, origin()).WillByDefault(
          Return(GURL("https://www.example.com")));
      EXPECT_CALL(*this, origin()).Times(1);
  }
  MOCK_METHOD3(DispatchEvent, bool(const base::Optional<std::string>&,
                                   const base::Optional<std::string>&,
                                   const base::Optional<std::string>&));
  MOCK_CONST_METHOD0(origin, GURL());
};

base::Optional<std::string> ToOptStr(const char* str) {
  return base::Optional<std::string>(str);
}

}  // namespace

class StorageAreaTest : public ::testing::Test {
 protected:
  StorageAreaTest();
  ~StorageAreaTest() override;
};

StorageAreaTest::StorageAreaTest() {
  EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
}

StorageAreaTest::~StorageAreaTest() {
  EXPECT_TRUE(GlobalStats::GetInstance()->CheckNoLeaks());
}

TEST_F(StorageAreaTest, InitialState) {
  scoped_refptr<MockStorage> mock_storage(new MockStorage());
  EXPECT_EQ(0, mock_storage->length());
  EXPECT_EQ(0, mock_storage->area_->size_bytes());
}

TEST_F(StorageAreaTest, Identifier) {
  scoped_refptr<MockStorage> mock_storage(new MockStorage());
  EXPECT_EQ(0, mock_storage->length());
  loader::Origin origin(GURL("https://www.example.com"));
  EXPECT_EQ(origin, mock_storage->area_->origin());
}

TEST_F(StorageAreaTest, SetItem) {
  scoped_refptr<MockStorage> mock_storage(new MockStorage());
  EXPECT_CALL(*mock_storage,
              DispatchEvent(base::Optional<std::string>("key"),
                            base::Optional<std::string>(base::nullopt),
                            base::Optional<std::string>("value")));
  mock_storage->SetItem("key", "value");
}

TEST_F(StorageAreaTest, IndexedKeys) {
  scoped_refptr<MockStorage> mock_storage(new MockStorage());

  std::set<std::string> found_keys;
  std::set<std::string> test_keys;
  test_keys.insert("key0");
  test_keys.insert("key1");
  test_keys.insert("key2");
  EXPECT_CALL(*mock_storage, DispatchEvent(_, _, _)).Times(3);
  for (std::set<std::string>::const_iterator it = test_keys.begin();
       it != test_keys.end(); ++it) {
    mock_storage->SetItem(*it, "value");
  }
  for (size_t i = 0; i < test_keys.size(); ++i) {
    found_keys.insert(mock_storage->Key(static_cast<unsigned int>(i)).value());
  }
  EXPECT_EQ(test_keys, found_keys);
}

TEST_F(StorageAreaTest, Overwrite) {
  scoped_refptr<MockStorage> mock_storage(new MockStorage());
  EXPECT_EQ(base::nullopt, mock_storage->GetItem("key"));
  EXPECT_CALL(*mock_storage,
              DispatchEvent(ToOptStr("key"), _, ToOptStr("old_value")));
  mock_storage->SetItem("key", "old_value");
  EXPECT_EQ(std::string("old_value"), mock_storage->GetItem("key"));
  EXPECT_CALL(*mock_storage,
              DispatchEvent(ToOptStr("key"), ToOptStr("old_value"),
                            ToOptStr("new_value")));
  mock_storage->SetItem("key", "new_value");
  EXPECT_EQ(std::string("new_value"), mock_storage->GetItem("key"));
}

TEST_F(StorageAreaTest, KeyOrder) {
  scoped_refptr<MockStorage> mock_storage(new MockStorage());

  std::vector<std::string> found_keys0;
  std::vector<std::string> found_keys1;
  std::vector<std::string> found_keys2;
  std::set<std::string> test_keys;
  test_keys.insert("key0");
  test_keys.insert("key1");
  test_keys.insert("key2");
  EXPECT_CALL(*mock_storage, DispatchEvent(_, _, _)).Times(3);
  for (std::set<std::string>::const_iterator it = test_keys.begin();
       it != test_keys.end(); ++it) {
    mock_storage->SetItem(*it, "value");
  }
  // Verify that the key order is consistent.
  for (size_t i = 0; i < test_keys.size(); ++i) {
    found_keys0.push_back(
        mock_storage->Key(static_cast<unsigned int>(i)).value());
  }
  for (size_t i = 0; i < test_keys.size(); ++i) {
    found_keys1.push_back(
        mock_storage->Key(static_cast<unsigned int>(i)).value());
  }
  EXPECT_EQ(found_keys0, found_keys1);

  // Verify that key order doesn't change even when the value does.
  EXPECT_CALL(*mock_storage, DispatchEvent(_, _, _));
  mock_storage->SetItem("key0", "new_value");
  for (size_t i = 0; i < test_keys.size(); ++i) {
    found_keys2.push_back(
        mock_storage->Key(static_cast<unsigned int>(i)).value());
  }
  EXPECT_EQ(found_keys0, found_keys2);
}

}  // namespace dom
}  // namespace cobalt
