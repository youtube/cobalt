/*
 * Copyright 2017 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "nb/concurrent_map.h"
#include "nb/hash.h"
#include "nb/simple_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nb {
namespace {

class CapturesIntValues {
 public:
  virtual void Visit(const int& key, const int& val) {
    visited_values_.push_back(std::make_pair(key, val));
  }

  std::map<int, int> ToMap() const {
    std::map<int, int> output;
    output.insert(visited_values_.begin(), visited_values_.end());
    return output;
  }
  std::vector<std::pair<int, int> > visited_values_;
};

TEST(ConcurrentMap, SetIfMissing) {
  IntToIntConcurrentMap int_map;
  ASSERT_FALSE(int_map.Has(1));
  ASSERT_TRUE(int_map.SetIfMissing(1, -1));
  ASSERT_TRUE(int_map.Has(1));
  ASSERT_FALSE(int_map.SetIfMissing(1, -2));  // False because exists.

  IntToIntConcurrentMap::EntryHandle entry_handle;

  ASSERT_TRUE(int_map.Get(1, &entry_handle));
  ASSERT_TRUE(entry_handle.Valid());
  ASSERT_EQ(-1, entry_handle.Value());

  // 2 is a missing key and doesn't exist.
  ASSERT_FALSE(int_map.Get(2, &entry_handle));
  ASSERT_FALSE(entry_handle.Valid());
}

TEST(ConcurrentMap, Set) {
  IntToIntConcurrentMap int_map;
  ASSERT_FALSE(int_map.Has(1));
  int_map.Set(1, -1);
  ASSERT_TRUE(int_map.Has(1));
  int_map.Set(1, -2);

  IntToIntConcurrentMap::EntryHandle entry_handle;

  ASSERT_TRUE(int_map.Get(1, &entry_handle));
  ASSERT_TRUE(entry_handle.Valid());
  ASSERT_EQ(-2, entry_handle.Value());  // Value was replaced.
}

TEST(ConcurrentMap, GetOrCreate) {
  IntToIntConcurrentMap int_map;

  // Create a default mapping 1=>0, then reset the value to -2.
  {
    IntToIntConcurrentMap::EntryHandle entry_handle;

    int_map.GetOrCreate(1, &entry_handle);  // Does not exist prior.

    ASSERT_TRUE(entry_handle.Valid());  // Value default created.
    EXPECT_EQ(0, entry_handle.Value());
    entry_handle.ValueMutable() = -2;
  }

  // Get the value again, expect it to be -2.
  {
    IntToIntConcurrentMap::EntryHandle entry_handle;

    EXPECT_TRUE(int_map.Get(1, &entry_handle));
    ASSERT_TRUE(entry_handle.Valid());
    EXPECT_EQ(-2, entry_handle.Value());
  }
}

TEST(ConcurrentMap, GetConst) {
  IntToIntConcurrentMap int_map;
  IntToIntConcurrentMap::ConstEntryHandle const_entry_handle;

  EXPECT_FALSE(int_map.Get(1, &const_entry_handle));
  EXPECT_FALSE(const_entry_handle.Valid());

  EXPECT_TRUE(int_map.SetIfMissing(1, -1));

  EXPECT_TRUE(int_map.Get(1, &const_entry_handle));
  EXPECT_TRUE(const_entry_handle.Valid());
  EXPECT_EQ(-1, const_entry_handle.Value());
}

TEST(ConcurrentMap, Remove) {
  IntToIntConcurrentMap int_map;
  int_map.Set(1, -1);
  EXPECT_EQ(1, int_map.GetSize());

  EXPECT_FALSE(int_map.Remove(2));  // Doesn't exist.
  EXPECT_TRUE(int_map.Remove(1));
  EXPECT_EQ(0, int_map.GetSize());
  EXPECT_TRUE(int_map.IsEmpty());
}

TEST(ConcurrentMap, RemoveUsingHandle) {
  IntToIntConcurrentMap int_map;
  int_map.Set(1, -1);
  int_map.Set(2, -2);
  EXPECT_EQ(2, int_map.GetSize());

  IntToIntConcurrentMap::EntryHandle entry_handle;
  int_map.Get(2, &entry_handle);

  int_map.Remove(&entry_handle);
  EXPECT_EQ(1, int_map.GetSize());
  EXPECT_TRUE(int_map.Has(1));
  EXPECT_FALSE(int_map.Has(2));

  int_map.Get(1, &entry_handle);
  int_map.Remove(&entry_handle);
  EXPECT_EQ(0, int_map.GetSize());
}

TEST(ConcurrentMap, Clear) {
  IntToIntConcurrentMap int_map;
  int_map.Set(1, -1);
  int_map.Set(2, -2);

  EXPECT_EQ(2, int_map.GetSize());
  EXPECT_EQ(2, int_map.GetSize());

  int_map.Clear();
  EXPECT_TRUE(int_map.IsEmpty());
  EXPECT_EQ(0, int_map.GetSize());
}

TEST(ConcurrentMap, ForEach) {
  IntToIntConcurrentMap int_map;
  int_map.Set(1, -1);
  int_map.Set(2, -2);

  CapturesIntValues captures_int_values;

  int_map.ForEach(&captures_int_values);

  std::map<int, int> std_int_map = captures_int_values.ToMap();
  EXPECT_EQ(std_int_map[1], -1);
  EXPECT_EQ(std_int_map[2], -2);
}

class InsertThread : public SimpleThread {
 public:
  InsertThread(int start_val, int end_val, IntToIntConcurrentMap* output)
      : SimpleThread("InsertThread"),
        test_ok_(true),
        start_value_(start_val),
        end_value_(end_val),
        concurrent_map_(output) {}

  virtual void Run() {
    for (int i = start_value_; i < end_value_; ++i) {
      if (!concurrent_map_->SetIfMissing(i, i)) {
        test_ok_ = false;
      }
    }
  }

  bool test_ok_;
  int start_value_;
  int end_value_;
  IntToIntConcurrentMap* concurrent_map_;
};

TEST(ConcurrentMap, MultiThreadTest) {
  IntToIntConcurrentMap shared_int_map;
  std::vector<InsertThread*> threads;

  threads.push_back(new InsertThread(0, 5000, &shared_int_map));
  threads.push_back(new InsertThread(5000, 10000, &shared_int_map));

  for (int i = 0; i < threads.size(); ++i) {
    threads[i]->Start();
  }

  for (int i = 0; i < threads.size(); ++i) {
    threads[i]->Join();
    EXPECT_TRUE(threads[i]->test_ok_);
    delete threads[i];
  }

  threads.clear();

  ASSERT_EQ(shared_int_map.GetSize(), 10000);

  CapturesIntValues captures_int_values;

  shared_int_map.ForEach(&captures_int_values);

  std::map<int, int> std_int_map = captures_int_values.ToMap();

  for (int i = 0; i < 10000; ++i) {
    ASSERT_EQ(std_int_map[i], i);

    IntToIntConcurrentMap::ConstEntryHandle const_entry_handle;
    ASSERT_TRUE(shared_int_map.Get(i, &const_entry_handle));
    ASSERT_TRUE(const_entry_handle.Valid());
    ASSERT_EQ(i, const_entry_handle.Value());
  }
}

}  // namespace
}  // namespace nb
