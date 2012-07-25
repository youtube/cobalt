// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/message_pump_default.h"

namespace {

base::MessagePump* CreateMessagePumpForUIForTests() {
  // A default MessagePump will do quite nicely in tests.
  return new base::MessagePumpDefault();
}

}  // namespace

namespace base {

void InitIOSTestMessageLoop() {
  MessageLoop::InitMessagePumpForUIFactory(&CreateMessagePumpForUIForTests);
}

}  // namespace base
