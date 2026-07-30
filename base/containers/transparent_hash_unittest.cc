// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/transparent_hash.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace base {
namespace {

struct MyObj : public base::RefCountedThreadSafe<MyObj> {
  int x;
  explicit MyObj(int v) : x(v) {}

 private:
  friend class base::RefCountedThreadSafe<MyObj>;
  ~MyObj() = default;
};

TEST(TransparentHashTest, VectorVsSpan) {
  std::vector<int> v1 = {1, 2, 3};
  std::vector<int> v2 = {1, 2, 3};
  std::vector<int> v3 = {1, 2};

  base::span<const int> s1 = v1;
  base::span<const int> s3 = v3;

  TransparentHashAs<base::span<const int>> hash;
  TransparentEqualAs<base::span<const int>> equal;

  EXPECT_EQ(hash(v1), hash(v2));
  EXPECT_EQ(hash(v1), hash(s1));
  EXPECT_NE(hash(v1), hash(s3));

  EXPECT_TRUE(equal(v1, s1));
  EXPECT_TRUE(equal(s1, v1));
  EXPECT_TRUE(equal(v1, v2));
  EXPECT_FALSE(equal(v1, s3));
}

TEST(TransparentHashTest, ScopedRefptrVsBuiltinPtr) {
  scoped_refptr<MyObj> r1 = base::MakeRefCounted<MyObj>(1);
  scoped_refptr<MyObj> r2 = r1;
  scoped_refptr<MyObj> r3 = base::MakeRefCounted<MyObj>(2);

  MyObj* p1 = r1.get();
  MyObj* p3 = r3.get();

  TransparentHashAs<MyObj*> hash;
  TransparentEqualAs<MyObj*> equal;

  EXPECT_EQ(hash(r1), hash(p1));
  EXPECT_NE(hash(r1), hash(p3));

  EXPECT_TRUE(equal(r1, p1));
  EXPECT_TRUE(equal(p1, r1));
  EXPECT_TRUE(equal(r1, r2));
  EXPECT_FALSE(equal(r1, p3));
}

TEST(TransparentHashTest, UniquePtrVsBuiltinPtr) {
  std::unique_ptr<int> u1 = std::make_unique<int>(1);
  std::unique_ptr<int> u3 = std::make_unique<int>(2);

  int* p1 = u1.get();
  int* p3 = u3.get();

  TransparentHashAs<int*> hash;
  TransparentEqualAs<int*> equal;

  EXPECT_EQ(hash(u1), hash(p1));
  EXPECT_NE(hash(u1), hash(p3));

  EXPECT_TRUE(equal(u1, p1));
  EXPECT_TRUE(equal(p1, u1));
  EXPECT_FALSE(equal(u1, p3));
}

TEST(TransparentHashTest, SharedPtrVsBuiltinPtr) {
  // NOTE: We can't spell std::shard_ptr in code, due to a PRESUBMIT ban.
  using SharedPtrInt = decltype(std::make_shared<int>());
  SharedPtrInt s1 = std::make_shared<int>(1);
  SharedPtrInt s2 = s1;
  SharedPtrInt s3 = std::make_shared<int>(2);

  int* p1 = s1.get();
  int* p3 = s3.get();

  TransparentHashAs<int*> hash;
  TransparentEqualAs<int*> equal;

  EXPECT_EQ(hash(s1), hash(p1));
  EXPECT_NE(hash(s1), hash(p3));

  EXPECT_TRUE(equal(s1, p1));
  EXPECT_TRUE(equal(p1, s1));
  EXPECT_TRUE(equal(s1, s2));
  EXPECT_FALSE(equal(s1, p3));
}

TEST(TransparentHashTest, RawPtrVsBuiltinPtr) {
  int val = 42;
  raw_ptr<int> r(&val);
  int* p = &val;

  TransparentHashAs<int*> hash;
  TransparentEqualAs<int*> equal;

  EXPECT_EQ(hash(r), hash(p));
  EXPECT_TRUE(equal(r, p));
  EXPECT_TRUE(equal(p, r));
}

TEST(TransparentHashTest, CompoundPairVectorVsSpan) {
  std::pair<std::vector<int>, int> p1 = {{1, 2, 3}, 42};
  std::pair<std::vector<int>, int> p2 = {{1, 2, 3}, 42};
  std::pair<std::vector<int>, int> p3 = {{1, 2}, 42};

  std::pair<base::span<const int>, int> s1 = {p1.first, 42};
  std::pair<base::span<const int>, int> s3 = {p3.first, 42};

  TransparentHashAs<std::pair<base::span<const int>, int>> hash;
  TransparentEqualAs<std::pair<base::span<const int>, int>> equal;

  EXPECT_EQ(hash(p1), hash(p2));
  EXPECT_EQ(hash(p1), hash(s1));
  EXPECT_NE(hash(p1), hash(s3));

  EXPECT_TRUE(equal(p1, s1));
  EXPECT_TRUE(equal(s1, p1));
  EXPECT_TRUE(equal(p1, p2));
  EXPECT_FALSE(equal(p1, s3));
}

TEST(TransparentHashTest, FlatHashSetLookupSpan) {
  using Set = absl::flat_hash_set<std::vector<int>,
                                  TransparentHashAs<base::span<const int>>,
                                  TransparentEqualAs<base::span<const int>>>;

  Set set;
  set.insert(std::vector<int>{1, 2, 3});
  set.insert(std::vector<int>{4, 5});

  std::vector<int> q1 = {1, 2, 3};
  base::span<const int> sq1 = q1;

  std::vector<int> q2 = {1, 2};
  base::span<const int> sq2 = q2;

  EXPECT_TRUE(set.contains(sq1));
  EXPECT_TRUE(set.contains(q1));
  EXPECT_FALSE(set.contains(sq2));
  EXPECT_FALSE(set.contains(q2));
}

TEST(TransparentHashTest, FlatHashSetLookupScopedRefptr) {
  using Set =
      absl::flat_hash_set<scoped_refptr<MyObj>, TransparentHashAs<MyObj*>,
                          TransparentEqualAs<MyObj*>>;

  scoped_refptr<MyObj> r1 = base::MakeRefCounted<MyObj>(1);
  scoped_refptr<MyObj> r2 = base::MakeRefCounted<MyObj>(2);
  scoped_refptr<MyObj> r3 = base::MakeRefCounted<MyObj>(3);

  Set set;
  set.insert(r1);
  set.insert(r2);

  EXPECT_TRUE(set.contains(r1));
  EXPECT_TRUE(set.contains(r1.get()));
  EXPECT_FALSE(set.contains(r3));
  EXPECT_FALSE(set.contains(r3.get()));
}

TEST(TransparentHashTest, FlatHashSetLookupCompoundPair) {
  using Set = absl::flat_hash_set<
      std::pair<std::vector<int>, int>,
      TransparentHashAs<std::pair<base::span<const int>, int>>,
      TransparentEqualAs<std::pair<base::span<const int>, int>>>;

  Set set;
  set.insert({{1, 2, 3}, 42});
  set.insert({{4, 5}, 43});

  std::vector<int> v1 = {1, 2, 3};
  std::pair<base::span<const int>, int> q1 = {v1, 42};

  std::vector<int> v2 = {1, 2};
  std::pair<base::span<const int>, int> q2 = {v2, 42};

  EXPECT_TRUE(set.contains(q1));
  EXPECT_FALSE(set.contains(q2));
}

TEST(TransparentHashTest, StaticOperators) {
  std::vector<int> v = {1, 2, 3};
  base::span<const int> s = v;

  // Verify they can be called statically without an instance.
  EXPECT_EQ(TransparentHashAs<base::span<const int>>::operator()(v),
            TransparentHashAs<base::span<const int>>::operator()(s));

  EXPECT_TRUE(TransparentEqualAs<base::span<const int>>::operator()(v, s));

  // Verify TransparentEqualAs is constexpr.
  static constexpr int a1[] = {1, 2, 3};
  static constexpr int a2[] = {1, 2, 3};
  constexpr base::span<const int> sa1 = a1;
  constexpr base::span<const int> sa2 = a2;
  static_assert(
      TransparentEqualAs<base::span<const int>>::operator()(sa1, sa2));
}

}  // namespace
}  // namespace base
