// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/task_traits.h"
#include "base/cpp14oncpp11.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(TaskTraitsTest, Default) {
  CONSTEXPR TaskTraits traits = {};
  EXPECT_FALSE(traits.priority_set_explicitly());
  EXPECT_EQ(TaskPriority::USER_VISIBLE, traits.priority());
  EXPECT_EQ(TaskShutdownBehavior::SKIP_ON_SHUTDOWN, traits.shutdown_behavior());
  EXPECT_FALSE(traits.may_block());
  EXPECT_FALSE(traits.with_base_sync_primitives());
}

TEST(TaskTraitsTest, TaskPriority) {
  CONSTEXPR TaskTraits traits = {TaskPriority::BEST_EFFORT};
  EXPECT_TRUE(traits.priority_set_explicitly());
  EXPECT_EQ(TaskPriority::BEST_EFFORT, traits.priority());
  EXPECT_EQ(TaskShutdownBehavior::SKIP_ON_SHUTDOWN, traits.shutdown_behavior());
  EXPECT_FALSE(traits.may_block());
  EXPECT_FALSE(traits.with_base_sync_primitives());
}

TEST(TaskTraitsTest, TaskShutdownBehavior) {
  CONSTEXPR TaskTraits traits = {TaskShutdownBehavior::BLOCK_SHUTDOWN};
  EXPECT_FALSE(traits.priority_set_explicitly());
  EXPECT_EQ(TaskPriority::USER_VISIBLE, traits.priority());
  EXPECT_EQ(TaskShutdownBehavior::BLOCK_SHUTDOWN, traits.shutdown_behavior());
  EXPECT_FALSE(traits.may_block());
  EXPECT_FALSE(traits.with_base_sync_primitives());
}

TEST(TaskTraitsTest, MayBlock) {
  CONSTEXPR TaskTraits traits = {MayBlock()};
  EXPECT_FALSE(traits.priority_set_explicitly());
  EXPECT_EQ(TaskPriority::USER_VISIBLE, traits.priority());
  EXPECT_EQ(TaskShutdownBehavior::SKIP_ON_SHUTDOWN, traits.shutdown_behavior());
  EXPECT_TRUE(traits.may_block());
  EXPECT_FALSE(traits.with_base_sync_primitives());
}

TEST(TaskTraitsTest, WithBaseSyncPrimitives) {
  CONSTEXPR TaskTraits traits = {WithBaseSyncPrimitives()};
  EXPECT_FALSE(traits.priority_set_explicitly());
  EXPECT_EQ(TaskPriority::USER_VISIBLE, traits.priority());
  EXPECT_EQ(TaskShutdownBehavior::SKIP_ON_SHUTDOWN, traits.shutdown_behavior());
  EXPECT_FALSE(traits.may_block());
  EXPECT_TRUE(traits.with_base_sync_primitives());
}

TEST(TaskTraitsTest, MultipleTraits) {
  CONSTEXPR TaskTraits traits = {TaskPriority::BEST_EFFORT,
                                 TaskShutdownBehavior::BLOCK_SHUTDOWN,
                                 MayBlock(), WithBaseSyncPrimitives()};
  EXPECT_TRUE(traits.priority_set_explicitly());
  EXPECT_EQ(TaskPriority::BEST_EFFORT, traits.priority());
  EXPECT_EQ(TaskShutdownBehavior::BLOCK_SHUTDOWN, traits.shutdown_behavior());
  EXPECT_TRUE(traits.may_block());
  EXPECT_TRUE(traits.with_base_sync_primitives());
}

TEST(TaskTraitsTest, Copy) {
  CONSTEXPR TaskTraits traits = {TaskPriority::BEST_EFFORT,
                                 TaskShutdownBehavior::BLOCK_SHUTDOWN,
                                 MayBlock(), WithBaseSyncPrimitives()};
  CONSTEXPR TaskTraits traits_copy(traits);
  EXPECT_EQ(traits.priority_set_explicitly(),
            traits_copy.priority_set_explicitly());
  EXPECT_EQ(traits.priority(), traits_copy.priority());
  EXPECT_EQ(traits.shutdown_behavior(), traits_copy.shutdown_behavior());
  EXPECT_EQ(traits.may_block(), traits_copy.may_block());
  EXPECT_EQ(traits.with_base_sync_primitives(),
            traits_copy.with_base_sync_primitives());
}

TEST(TaskTraitsTest, OverridePriority) {
  CONSTEXPR TaskTraits left = {TaskPriority::BEST_EFFORT};
  CONSTEXPR TaskTraits right = {TaskPriority::USER_BLOCKING};
  CONSTEXPR TaskTraits overridden = TaskTraits::Override(left, right);
  EXPECT_TRUE(overridden.priority_set_explicitly());
  EXPECT_EQ(TaskPriority::USER_BLOCKING, overridden.priority());
  EXPECT_FALSE(overridden.shutdown_behavior_set_explicitly());
  EXPECT_EQ(TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
            overridden.shutdown_behavior());
  EXPECT_FALSE(overridden.may_block());
  EXPECT_FALSE(overridden.with_base_sync_primitives());
}

TEST(TaskTraitsTest, OverrideShutdownBehavior) {
  CONSTEXPR TaskTraits left = {TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};
  CONSTEXPR TaskTraits right = {TaskShutdownBehavior::BLOCK_SHUTDOWN};
  CONSTEXPR TaskTraits overridden = TaskTraits::Override(left, right);
  EXPECT_FALSE(overridden.priority_set_explicitly());
  EXPECT_EQ(TaskPriority::USER_VISIBLE, overridden.priority());
  EXPECT_TRUE(overridden.shutdown_behavior_set_explicitly());
  EXPECT_EQ(TaskShutdownBehavior::BLOCK_SHUTDOWN,
            overridden.shutdown_behavior());
  EXPECT_FALSE(overridden.may_block());
  EXPECT_FALSE(overridden.with_base_sync_primitives());
}

TEST(TaskTraitsTest, OverrideMayBlock) {
  {
    CONSTEXPR TaskTraits left = {MayBlock()};
    CONSTEXPR TaskTraits right = {};
    CONSTEXPR TaskTraits overridden = TaskTraits::Override(left, right);
    EXPECT_FALSE(overridden.priority_set_explicitly());
    EXPECT_EQ(TaskPriority::USER_VISIBLE, overridden.priority());
    EXPECT_FALSE(overridden.shutdown_behavior_set_explicitly());
    EXPECT_EQ(TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
              overridden.shutdown_behavior());
    EXPECT_TRUE(overridden.may_block());
    EXPECT_FALSE(overridden.with_base_sync_primitives());
  }
  {
    CONSTEXPR TaskTraits left = {};
    CONSTEXPR TaskTraits right = {MayBlock()};
    CONSTEXPR TaskTraits overridden = TaskTraits::Override(left, right);
    EXPECT_FALSE(overridden.priority_set_explicitly());
    EXPECT_EQ(TaskPriority::USER_VISIBLE, overridden.priority());
    EXPECT_FALSE(overridden.shutdown_behavior_set_explicitly());
    EXPECT_EQ(TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
              overridden.shutdown_behavior());
    EXPECT_TRUE(overridden.may_block());
    EXPECT_FALSE(overridden.with_base_sync_primitives());
  }
}

TEST(TaskTraitsTest, OverrideWithBaseSyncPrimitives) {
  {
    CONSTEXPR TaskTraits left = {WithBaseSyncPrimitives()};
    CONSTEXPR TaskTraits right = {};
    CONSTEXPR TaskTraits overridden = TaskTraits::Override(left, right);
    EXPECT_FALSE(overridden.priority_set_explicitly());
    EXPECT_EQ(TaskPriority::USER_VISIBLE, overridden.priority());
    EXPECT_FALSE(overridden.shutdown_behavior_set_explicitly());
    EXPECT_EQ(TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
              overridden.shutdown_behavior());
    EXPECT_FALSE(overridden.may_block());
    EXPECT_TRUE(overridden.with_base_sync_primitives());
  }
  {
    CONSTEXPR TaskTraits left = {};
    CONSTEXPR TaskTraits right = {WithBaseSyncPrimitives()};
    CONSTEXPR TaskTraits overridden = TaskTraits::Override(left, right);
    EXPECT_FALSE(overridden.priority_set_explicitly());
    EXPECT_EQ(TaskPriority::USER_VISIBLE, overridden.priority());
    EXPECT_FALSE(overridden.shutdown_behavior_set_explicitly());
    EXPECT_EQ(TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
              overridden.shutdown_behavior());
    EXPECT_FALSE(overridden.may_block());
    EXPECT_TRUE(overridden.with_base_sync_primitives());
  }
}

TEST(TaskTraitsTest, OverrideMultipleTraits) {
  CONSTEXPR TaskTraits left = {MayBlock(), TaskPriority::BEST_EFFORT,
                               TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};
  CONSTEXPR TaskTraits right = {WithBaseSyncPrimitives(),
                                TaskPriority::USER_BLOCKING};
  CONSTEXPR TaskTraits overridden = TaskTraits::Override(left, right);
  EXPECT_TRUE(overridden.priority_set_explicitly());
  EXPECT_EQ(right.priority(), overridden.priority());
  EXPECT_TRUE(overridden.shutdown_behavior_set_explicitly());
  EXPECT_EQ(left.shutdown_behavior(), overridden.shutdown_behavior());
  EXPECT_TRUE(overridden.may_block());
  EXPECT_TRUE(overridden.with_base_sync_primitives());
}

}  // namespace base
