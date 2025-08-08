// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_test_channel_listener.h"

#include "base/run_loop.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace IPC {

// static
void TestChannelListener::SendOneMessage(IPC::Sender* sender,
                                         const char* text) {
  static int message_index = 0;

  IPC::Message* message = new IPC::Message(0,
                                           2,
                                           IPC::Message::PRIORITY_NORMAL);
  message->WriteInt(message_index++);
  message->WriteString(std::string(text));

  // Make sure we can handle large messages.
  char junk[kLongMessageStringNumBytes];
  memset(junk, 'a', sizeof(junk)-1);
  junk[sizeof(junk)-1] = 0;
  message->WriteString(std::string(junk));

  sender->Send(message);
}


bool TestChannelListener::OnMessageReceived(const IPC::Message& message) {
  base::PickleIterator iter(message);

  int ignored;
  EXPECT_TRUE(iter.ReadInt(&ignored));
  std::string data;
  EXPECT_TRUE(iter.ReadString(&data));
  std::string big_string;
  EXPECT_TRUE(iter.ReadString(&big_string));
  EXPECT_EQ(kLongMessageStringNumBytes - 1, big_string.length());

  SendNextMessage();
  return true;
}

void TestChannelListener::OnChannelError() {
  // There is a race when closing the channel so the last message may be lost.
  EXPECT_LE(messages_left_, 1);
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
}

void TestChannelListener::SendNextMessage() {
  if (--messages_left_ <= 0)
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  else
    SendOneMessage(sender_, "Foo");
}

}
