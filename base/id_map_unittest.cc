// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/id_map.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

class IDMapTest : public testing::Test {
};

class TestObject {
};

class DestructorCounter {
 public:
  explicit DestructorCounter(int* counter) : counter_(counter) {}
  ~DestructorCounter() { ++(*counter_); }
 private:
  int* counter_;
};

TEST_F(IDMapTest, Basic) {
  IDMap<TestObject> map;
  EXPECT_TRUE(map.IsEmpty());
  EXPECT_EQ(0U, map.size());

  TestObject obj1;
  TestObject obj2;

  int32 id1 = map.Add(&obj1);
  EXPECT_FALSE(map.IsEmpty());
  EXPECT_EQ(1U, map.size());
  EXPECT_EQ(&obj1, map.Lookup(id1));

  int32 id2 = map.Add(&obj2);
  EXPECT_FALSE(map.IsEmpty());
  EXPECT_EQ(2U, map.size());

  EXPECT_EQ(&obj1, map.Lookup(id1));
  EXPECT_EQ(&obj2, map.Lookup(id2));

  map.Remove(id1);
  EXPECT_FALSE(map.IsEmpty());
  EXPECT_EQ(1U, map.size());

  map.Remove(id2);
  EXPECT_TRUE(map.IsEmpty());
  EXPECT_EQ(0U, map.size());

  map.AddWithID(&obj1, 1);
  map.AddWithID(&obj2, 2);
  EXPECT_EQ(&obj1, map.Lookup(1));
  EXPECT_EQ(&obj2, map.Lookup(2));
}

TEST_F(IDMapTest, IteratorRemainsValidWhenRemovingCurrentElement) {
  IDMap<TestObject> map;

  TestObject obj1;
  TestObject obj2;
  TestObject obj3;

  map.Add(&obj1);
  map.Add(&obj2);
  map.Add(&obj3);

  {
    IDMap<TestObject>::const_iterator iter(&map);
    while (!iter.IsAtEnd()) {
      map.Remove(iter.GetCurrentKey());
      iter.Advance();
    }

    // Test that while an iterator is still in scope, we get the map emptiness
    // right (http://crbug.com/35571).
    EXPECT_TRUE(map.IsEmpty());
    EXPECT_EQ(0U, map.size());
  }

  EXPECT_TRUE(map.IsEmpty());
  EXPECT_EQ(0U, map.size());
}

TEST_F(IDMapTest, IteratorRemainsValidWhenRemovingOtherElements) {
  IDMap<TestObject> map;

  const int kCount = 5;
  TestObject obj[kCount];
  int32 ids[kCount];

  for (int i = 0; i < kCount; i++)
    ids[i] = map.Add(&obj[i]);

  int counter = 0;
  for (IDMap<TestObject>::const_iterator iter(&map);
       !iter.IsAtEnd(); iter.Advance()) {
    switch (counter) {
      case 0:
        EXPECT_EQ(ids[0], iter.GetCurrentKey());
        EXPECT_EQ(&obj[0], iter.GetCurrentValue());
        map.Remove(ids[1]);
        break;
      case 1:
        EXPECT_EQ(ids[2], iter.GetCurrentKey());
        EXPECT_EQ(&obj[2], iter.GetCurrentValue());
        map.Remove(ids[3]);
        break;
      case 2:
        EXPECT_EQ(ids[4], iter.GetCurrentKey());
        EXPECT_EQ(&obj[4], iter.GetCurrentValue());
        map.Remove(ids[0]);
        break;
      default:
        FAIL() << "should not have that many elements";
        break;
    }

    counter++;
  }
}

TEST_F(IDMapTest, OwningPointersDeletesThemOnRemove) {
  const int kCount = 3;

  int external_del_count = 0;
  DestructorCounter* external_obj[kCount];
  int map_external_ids[kCount];

  int owned_del_count = 0;
  DestructorCounter* owned_obj[kCount];
  int map_owned_ids[kCount];

  IDMap<DestructorCounter> map_external;
  IDMap<DestructorCounter, IDMapOwnPointer> map_owned;

  for (int i = 0; i < kCount; ++i) {
    external_obj[i] = new DestructorCounter(&external_del_count);
    map_external_ids[i] = map_external.Add(external_obj[i]);

    owned_obj[i] = new DestructorCounter(&owned_del_count);
    map_owned_ids[i] = map_owned.Add(owned_obj[i]);
  }

  for (int i = 0; i < kCount; ++i) {
    EXPECT_EQ(external_del_count, 0);
    EXPECT_EQ(owned_del_count, i);

    map_external.Remove(map_external_ids[i]);
    map_owned.Remove(map_owned_ids[i]);
  }

  for (int i = 0; i < kCount; ++i) {
    delete external_obj[i];
  }

  EXPECT_EQ(external_del_count, kCount);
  EXPECT_EQ(owned_del_count, kCount);
}

TEST_F(IDMapTest, OwningPointersDeletesThemOnDestruct) {
  const int kCount = 3;

  int external_del_count = 0;
  DestructorCounter* external_obj[kCount];
  int map_external_ids[kCount];

  int owned_del_count = 0;
  DestructorCounter* owned_obj[kCount];
  int map_owned_ids[kCount];

  {
    IDMap<DestructorCounter> map_external;
    IDMap<DestructorCounter, IDMapOwnPointer> map_owned;

    for (int i = 0; i < kCount; ++i) {
      external_obj[i] = new DestructorCounter(&external_del_count);
      map_external_ids[i] = map_external.Add(external_obj[i]);

      owned_obj[i] = new DestructorCounter(&owned_del_count);
      map_owned_ids[i] = map_owned.Add(owned_obj[i]);
    }
  }

  EXPECT_EQ(external_del_count, 0);

  for (int i = 0; i < kCount; ++i) {
    delete external_obj[i];
  }

  EXPECT_EQ(external_del_count, kCount);
  EXPECT_EQ(owned_del_count, kCount);
}

}  // namespace
