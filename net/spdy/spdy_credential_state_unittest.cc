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
      origin1_("https://1.com"),
      origin2_("https://2.com"),
      origin3_("https://3.com"),
      origin4_("https://4.com"),
      origin5_("https://5.com"),
      origin6_("https://6.com"),
      origin11_("https://11.com"),
      host1_("https://www.1.com:443") {
  }

 protected:
  SpdyCredentialState state_;
  const GURL origin1_;
  const GURL origin2_;
  const GURL origin3_;
  const GURL origin4_;
  const GURL origin5_;
  const GURL origin6_;
  const GURL origin11_;
  const GURL host1_;

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
  EXPECT_TRUE(state_.HasCredential(host1_));
  EXPECT_FALSE(state_.HasCredential(origin11_));
  EXPECT_FALSE(state_.HasCredential(origin2_));
  EXPECT_FALSE(state_.HasCredential(origin3_));
}

TEST_F(SpdyCredentialStateTest, SetCredentialAddsToEndOfList) {
  EXPECT_EQ(1u, (state_.SetHasCredential(origin1_)));
  EXPECT_EQ(2u, (state_.SetHasCredential(origin2_)));
  EXPECT_EQ(3u, (state_.SetHasCredential(origin3_)));
}

TEST_F(SpdyCredentialStateTest, SetReturnsPositionIfAlreadyInList) {
  EXPECT_EQ(1u, (state_.SetHasCredential(origin1_)));
  EXPECT_EQ(2u, (state_.SetHasCredential(origin2_)));
  EXPECT_EQ(1u, (state_.SetHasCredential(origin1_)));
  EXPECT_EQ(2u, (state_.SetHasCredential(origin2_)));
}

TEST_F(SpdyCredentialStateTest, SetReplacesOldestElementWhenFull) {
  EXPECT_EQ(1u, (state_.SetHasCredential(origin1_)));
  EXPECT_EQ(2u, (state_.SetHasCredential(origin2_)));
  EXPECT_EQ(3u, (state_.SetHasCredential(origin3_)));
  EXPECT_EQ(4u, (state_.SetHasCredential(origin4_)));
  EXPECT_EQ(1u, (state_.SetHasCredential(origin5_)));
  EXPECT_EQ(2u, (state_.SetHasCredential(origin6_)));
  EXPECT_EQ(3u, (state_.SetHasCredential(origin1_)));
  EXPECT_EQ(4u, (state_.SetHasCredential(origin2_)));
}

TEST_F(SpdyCredentialStateTest, ResizeAddsEmptySpaceAtEnd) {
  EXPECT_EQ(1u, (state_.SetHasCredential(origin1_)));
  EXPECT_EQ(2u, (state_.SetHasCredential(origin2_)));
  EXPECT_EQ(3u, (state_.SetHasCredential(origin3_)));
  EXPECT_EQ(4u, (state_.SetHasCredential(origin4_)));
  state_.Resize(6);
  EXPECT_EQ(1u, (state_.SetHasCredential(origin1_)));
  EXPECT_EQ(2u, (state_.SetHasCredential(origin2_)));
  EXPECT_EQ(3u, (state_.SetHasCredential(origin3_)));
  EXPECT_EQ(4u, (state_.SetHasCredential(origin4_)));
  EXPECT_EQ(5u, (state_.SetHasCredential(origin5_)));
  EXPECT_EQ(6u, (state_.SetHasCredential(origin6_)));
}

TEST_F(SpdyCredentialStateTest, ResizeTrunatesFromEnd) {
  EXPECT_EQ(1u, (state_.SetHasCredential(origin1_)));
  EXPECT_EQ(2u, (state_.SetHasCredential(origin2_)));
  EXPECT_EQ(3u, (state_.SetHasCredential(origin3_)));
  EXPECT_EQ(4u, (state_.SetHasCredential(origin4_)));
  state_.Resize(2);
  EXPECT_TRUE(state_.HasCredential(origin1_));
  EXPECT_TRUE(state_.HasCredential(origin2_));
  EXPECT_FALSE(state_.HasCredential(origin3_));
  EXPECT_FALSE(state_.HasCredential(origin4_));
  EXPECT_EQ(1u, (state_.SetHasCredential(origin5_)));
  EXPECT_EQ(2u, (state_.SetHasCredential(origin6_)));
}


}  // namespace net
