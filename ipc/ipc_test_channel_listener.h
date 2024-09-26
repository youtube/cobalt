// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_TEST_CHANNEL_LISTENER_H_
#define IPC_IPC_TEST_CHANNEL_LISTENER_H_

#include <stddef.h>

#include "base/memory/raw_ptr.h"
#include "ipc/ipc_listener.h"

namespace IPC {

class Sender;

// A generic listener that expects messages of a certain type (see
// OnMessageReceived()), and either sends a generic response or quits after the
// 50th message (or on channel error).
class TestChannelListener : public Listener {
 public:
  static const size_t kLongMessageStringNumBytes = 50000;
  static void SendOneMessage(Sender* sender, const char* text);

  TestChannelListener() : sender_(nullptr), messages_left_(50) {}
  ~TestChannelListener() override {}

  bool OnMessageReceived(const Message& message) override;
  void OnChannelError() override;

  void Init(Sender* s) {
    sender_ = s;
  }

  bool HasSentAll() const { return 0 == messages_left_; }

 protected:
  void SendNextMessage();

 private:
  raw_ptr<Sender> sender_;
  int messages_left_;
};

}

#endif // IPC_IPC_TEST_CHANNEL_LISTENER_H_
