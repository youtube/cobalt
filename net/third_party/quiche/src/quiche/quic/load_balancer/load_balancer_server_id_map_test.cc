// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/load_balancer/load_balancer_server_id_map.h"

#include "quiche/quic/platform/api/quic_expect_bug.h"
#include "quiche/quic/platform/api/quic_test.h"
#include "quiche/quic/test_tools/quic_test_utils.h"

namespace quic {

namespace test {

namespace {

constexpr uint8_t kServerId[] = {0xed, 0x79, 0x3a, 0x51};

class LoadBalancerServerIdMapTest : public QuicTest {
 public:
  const LoadBalancerServerId valid_server_id_ =
      *LoadBalancerServerId::Create(kServerId);
  const LoadBalancerServerId invalid_server_id_ =
      *LoadBalancerServerId::Create(absl::Span<const uint8_t>(kServerId, 3));
};

TEST_F(LoadBalancerServerIdMapTest, CreateWithBadServerIdLength) {
  EXPECT_QUIC_BUG(EXPECT_EQ(LoadBalancerServerIdMap<int>::Create(0), nullptr),
                  "Tried to configure map with server ID length 0");
  EXPECT_QUIC_BUG(EXPECT_EQ(LoadBalancerServerIdMap<int>::Create(16), nullptr),
                  "Tried to configure map with server ID length 16");
}

TEST_F(LoadBalancerServerIdMapTest, AddOrReplaceWithBadServerIdLength) {
  int record = 1;
  auto pool = LoadBalancerServerIdMap<int>::Create(4);
  EXPECT_NE(pool, nullptr);
  EXPECT_QUIC_BUG(pool->AddOrReplace(invalid_server_id_, record),
                  "Server ID of 3 bytes; this map requires 4");
}

TEST_F(LoadBalancerServerIdMapTest, LookupWithBadServerIdLength) {
  int record = 1;
  auto pool = LoadBalancerServerIdMap<int>::Create(4);
  EXPECT_NE(pool, nullptr);
  pool->AddOrReplace(valid_server_id_, record);
  EXPECT_QUIC_BUG(EXPECT_FALSE(pool->Lookup(invalid_server_id_).has_value()),
                  "Lookup with a 3 byte server ID, map requires 4");
  EXPECT_QUIC_BUG(EXPECT_EQ(pool->LookupNoCopy(invalid_server_id_), nullptr),
                  "Lookup with a 3 byte server ID, map requires 4");
}

TEST_F(LoadBalancerServerIdMapTest, LookupWhenEmpty) {
  auto pool = LoadBalancerServerIdMap<int>::Create(4);
  EXPECT_NE(pool, nullptr);
  EXPECT_EQ(pool->LookupNoCopy(valid_server_id_), nullptr);
  absl::optional<int> result = pool->Lookup(valid_server_id_);
  EXPECT_FALSE(result.has_value());
}

TEST_F(LoadBalancerServerIdMapTest, AddLookup) {
  int record1 = 1, record2 = 2;
  auto pool = LoadBalancerServerIdMap<int>::Create(4);
  EXPECT_NE(pool, nullptr);
  auto other_server_id = LoadBalancerServerId::Create({0x01, 0x02, 0x03, 0x04});
  EXPECT_TRUE(other_server_id.has_value());
  pool->AddOrReplace(valid_server_id_, record1);
  pool->AddOrReplace(*other_server_id, record2);
  absl::optional<int> result = pool->Lookup(valid_server_id_);
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(*result, record1);
  auto result_ptr = pool->LookupNoCopy(valid_server_id_);
  EXPECT_NE(result_ptr, nullptr);
  EXPECT_EQ(*result_ptr, record1);
  result = pool->Lookup(*other_server_id);
  EXPECT_TRUE(result.has_value());
  EXPECT_EQ(*result, record2);
}

TEST_F(LoadBalancerServerIdMapTest, AddErase) {
  int record = 1;
  auto pool = LoadBalancerServerIdMap<int>::Create(4);
  EXPECT_NE(pool, nullptr);
  pool->AddOrReplace(valid_server_id_, record);
  EXPECT_EQ(*pool->LookupNoCopy(valid_server_id_), record);
  pool->Erase(valid_server_id_);
  EXPECT_EQ(pool->LookupNoCopy(valid_server_id_), nullptr);
}

}  // namespace

}  // namespace test

}  // namespace quic
