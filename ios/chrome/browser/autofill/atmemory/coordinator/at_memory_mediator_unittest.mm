// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/at_memory_mediator.h"

#import "ios/chrome/browser/autofill/atmemory/ui/at_memory_consumer.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "third_party/ocmock/gtest_support.h"

namespace {

using AtMemoryMediatorTest = PlatformTest;

// Tests that setting the consumer on the mediator immediately pushes the
// kEmpty content state to it.
TEST_F(AtMemoryMediatorTest, SetsInitialEmptyStateOnConsumer) {
  AtMemoryMediator* mediator = [[AtMemoryMediator alloc] init];
  id consumer = [OCMockObject mockForProtocol:@protocol(AtMemoryConsumer)];

  OCMExpect([consumer setContentState:at_memory::AtMemoryContentState::kEmpty]);

  mediator.consumer = consumer;

  EXPECT_OCMOCK_VERIFY(consumer);
}

}  // namespace
