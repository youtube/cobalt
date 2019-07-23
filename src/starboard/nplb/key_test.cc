// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/key.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbKeyTest, CanReference) {
  EXPECT_NE(kSbKeyUnknown, kSbKeyMediaRewind);
  EXPECT_NE(kSbKeyUnknown, kSbKeyMediaFastForward);
  EXPECT_NE(kSbKeyUnknown, kSbKeyRed);
  EXPECT_NE(kSbKeyUnknown, kSbKeyGreen);
  EXPECT_NE(kSbKeyUnknown, kSbKeyYellow);
  EXPECT_NE(kSbKeyUnknown, kSbKeyBlue);
  EXPECT_NE(kSbKeyUnknown, kSbKeySubtitle);
  EXPECT_NE(kSbKeyUnknown, kSbKeyChannelUp);
  EXPECT_NE(kSbKeyUnknown, kSbKeyChannelDown);
  EXPECT_NE(kSbKeyUnknown, kSbKeyClosedCaption);
  EXPECT_NE(kSbKeyUnknown, kSbKeyInfo);
  EXPECT_NE(kSbKeyUnknown, kSbKeyGuide);
  EXPECT_NE(kSbKeyUnknown, kSbKeyLast);
  EXPECT_NE(kSbKeyUnknown, kSbKeyPreviousChannel);
  EXPECT_NE(kSbKeyUnknown, kSbKeyLaunchThisApplication);
  EXPECT_NE(kSbKeyUnknown, kSbKeyMediaAudioTrack);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
