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

#include "starboard/common/ref_counted.h"

#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

class RefCountedTestClass : public RefCounted<RefCountedTestClass> {
 public:
  explicit RefCountedTestClass(int* destructor_count)
      : destructor_count_(destructor_count) {}

 private:
  friend class RefCounted<RefCountedTestClass>;
  ~RefCountedTestClass() {
    if (destructor_count_) {
      ++(*destructor_count_);
    }
  }

  int* destructor_count_;
};

class BaseClass : public RefCounted<BaseClass> {
 public:
  explicit BaseClass(int* destructor_count)
      : destructor_count_(destructor_count) {}

 protected:
  friend class RefCounted<BaseClass>;
  virtual ~BaseClass() {
    if (destructor_count_) {
      ++(*destructor_count_);
    }
  }

 private:
  int* destructor_count_;
};

class DerivedClass : public BaseClass {
 public:
  explicit DerivedClass(int* destructor_count) : BaseClass(destructor_count) {}

 private:
  friend class RefCounted<BaseClass>;
  ~DerivedClass() override {}
};

TEST(ScopedRefptrTest, MoveAssignmentSameType) {
  int destructor_count1 = 0;
  int destructor_count2 = 0;

  auto p1 = make_scoped_refptr<RefCountedTestClass>(&destructor_count1);
  RefCountedTestClass* raw_ptr1 = p1.get();
  EXPECT_TRUE(p1->HasOneRef());

  scoped_refptr<RefCountedTestClass> p2;
  p2 = std::move(p1);
  EXPECT_EQ(nullptr, p1.get());
  EXPECT_EQ(raw_ptr1, p2.get());
  EXPECT_TRUE(p2->HasOneRef());
  EXPECT_EQ(0, destructor_count1);

  auto p3 = make_scoped_refptr<RefCountedTestClass>(&destructor_count2);
  EXPECT_EQ(0, destructor_count2);

  // Move-assigning p2 into p3 should destroy the old object managed by p3.
  p3 = std::move(p2);
  EXPECT_EQ(1, destructor_count2);
  EXPECT_EQ(0, destructor_count1);
  EXPECT_EQ(nullptr, p2.get());
  EXPECT_EQ(raw_ptr1, p3.get());
  EXPECT_TRUE(p3->HasOneRef());
}

TEST(ScopedRefptrTest, MoveAssignmentDerivedToBase) {
  int destructor_count1 = 0;
  int destructor_count2 = 0;

  auto derived = make_scoped_refptr<DerivedClass>(&destructor_count1);
  DerivedClass* raw_derived = derived.get();
  EXPECT_TRUE(derived->HasOneRef());

  auto base = make_scoped_refptr<BaseClass>(&destructor_count2);
  EXPECT_EQ(0, destructor_count2);

  base = std::move(derived);
  EXPECT_EQ(1, destructor_count2);
  EXPECT_EQ(0, destructor_count1);
  EXPECT_EQ(nullptr, derived.get());
  EXPECT_EQ(raw_derived, base.get());
  EXPECT_TRUE(base->HasOneRef());
}

TEST(ScopedRefptrTest, SelfMoveAssignment) {
  int destructor_count = 0;
  auto p1 = make_scoped_refptr<RefCountedTestClass>(&destructor_count);
  RefCountedTestClass* raw_ptr = p1.get();

  scoped_refptr<RefCountedTestClass>& p1_ref = p1;
  p1 = std::move(p1_ref);

  EXPECT_EQ(0, destructor_count);
  EXPECT_EQ(raw_ptr, p1.get());
  EXPECT_TRUE(p1->HasOneRef());
}

}  // namespace
}  // namespace starboard
