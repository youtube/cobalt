/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "perfetto/protozero/message_handle.h"

#include "perfetto/protozero/root_message.h"
#include "test/gtest_and_gmock.h"

namespace protozero {

namespace {

TEST(MessageHandleTest, MoveHandleSharedMessageDoesntFinalize) {
  RootMessage<Message> message;

  MessageHandle<Message> handle_1(&message);
  handle_1 = MessageHandle<Message>(&message);
  ASSERT_FALSE(handle_1->is_finalized());
}

}  // namespace
}  // namespace protozero
