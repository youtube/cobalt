// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/spdy/spdy_credential_state.h"

#include "net/base/host_port_pair.h"
#include "testing/platform_test.h"

namespace net {

class SpdyCredentialStateTest : public PlatformTest {
 public:
  SpdyCredentialStateTest()
    : state_(4),
      origin1_("1.com", 80),
      origin2_("2.com", 80),
      origin3_("3.com", 80),
      origin4_("4.com", 80),
      origin5_("5.com", 80),
      origin6_("6.com", 80) {
  }

 protected:
  SpdyCredentialState state_;
  const HostPortPair origin1_;
  const HostPortPair origin2_;
  const HostPortPair origin3_;
  const HostPortPair origin4_;
  const HostPortPair origin5_;
  const HostPortPair origin6_;

  DISALLOW_COPY_AND_ASSIGN(SpdyCredentialStateTest);
};

TEST_F(SpdyCredentialStateTest, HasCredentialReturnsFalseWhenEmpty) {
  EXPECT_FALSE(state_.HasCredential(origin1_));
  EXPECT_FALSE(state_.HasCredential(origin2_));
  EXPECT_FALSE(state_.HasCredential(origin3_));
}

TEST_F(SpdyCredentialStateTest, HasCredentialReturnsTrueWhenAdded) {
  state_.SetHasCredential(origin1_);
  EXPECT_TRUE(state_.HasCredential(origin1_));
  EXPECT_FALSE(state_.HasCredential(origin2_));
  EXPECT_FALSE(state_.HasCredential(origin3_));
}

TEST_F(SpdyCredentialStateTest, SetCredentialAddsToEndOfList) {
  EXPECT_EQ(0u, (state_.SetHasCredential(origin1_)));
  EXPECT_EQ(1u, (state_.SetHasCredential(origin2_)));
  EXPECT_EQ(2u, (state_.SetHasCredential(origin3_)));
}

TEST_F(SpdyCredentialStateTest, SetReturnsPositionIfAlreadyInList) {
  EXPECT_EQ(0u, (state_.SetHasCredential(origin1_)));
  EXPECT_EQ(1u, (state_.SetHasCredential(origin2_)));
  EXPECT_EQ(0u, (state_.SetHasCredential(origin1_)));
  EXPECT_EQ(1u, (state_.SetHasCredential(origin2_)));
}

TEST_F(SpdyCredentialStateTest, SetReplacesOldestElementWhenFull) {
  EXPECT_EQ(0u, (state_.SetHasCredential(origin1_)));
  EXPECT_EQ(1u, (state_.SetHasCredential(origin2_)));
  EXPECT_EQ(2u, (state_.SetHasCredential(origin3_)));
  EXPECT_EQ(3u, (state_.SetHasCredential(origin4_)));
  EXPECT_EQ(0u, (state_.SetHasCredential(origin5_)));
  EXPECT_EQ(1u, (state_.SetHasCredential(origin6_)));
  EXPECT_EQ(2u, (state_.SetHasCredential(origin1_)));
  EXPECT_EQ(3u, (state_.SetHasCredential(origin2_)));
}

TEST_F(SpdyCredentialStateTest, ResizeAddsEmptySpaceAtEnd) {
  EXPECT_EQ(0u, (state_.SetHasCredential(origin1_)));
  EXPECT_EQ(1u, (state_.SetHasCredential(origin2_)));
  EXPECT_EQ(2u, (state_.SetHasCredential(origin3_)));
  EXPECT_EQ(3u, (state_.SetHasCredential(origin4_)));
  state_.Resize(6);
  EXPECT_EQ(0u, (state_.SetHasCredential(origin1_)));
  EXPECT_EQ(1u, (state_.SetHasCredential(origin2_)));
  EXPECT_EQ(2u, (state_.SetHasCredential(origin3_)));
  EXPECT_EQ(3u, (state_.SetHasCredential(origin4_)));
  EXPECT_EQ(4u, (state_.SetHasCredential(origin5_)));
  EXPECT_EQ(5u, (state_.SetHasCredential(origin6_)));
}

TEST_F(SpdyCredentialStateTest, ResizeTrunatesFromEnd) {
  EXPECT_EQ(0u, (state_.SetHasCredential(origin1_)));
  EXPECT_EQ(1u, (state_.SetHasCredential(origin2_)));
  EXPECT_EQ(2u, (state_.SetHasCredential(origin3_)));
  EXPECT_EQ(3u, (state_.SetHasCredential(origin4_)));
  state_.Resize(2);
  EXPECT_TRUE(state_.HasCredential(origin1_));
  EXPECT_TRUE(state_.HasCredential(origin2_));
  EXPECT_FALSE(state_.HasCredential(origin3_));
  EXPECT_FALSE(state_.HasCredential(origin4_));
  EXPECT_EQ(0u, (state_.SetHasCredential(origin5_)));
  EXPECT_EQ(1u, (state_.SetHasCredential(origin6_)));
}


}  // namespace net
