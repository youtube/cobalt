// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_ANDROID_H_
#define BASE_MESSAGE_PUMP_ANDROID_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/message_pump.h"
#include "base/time.h"

namespace base {

// This class implements a MessagePump needed for TYPE_UI MessageLoops on
// OS_ANDROID platform.
class MessagePumpForUI : public MessagePump {
 public:
  MessagePumpForUI();
  virtual ~MessagePumpForUI();

  virtual void Run(Delegate* delegate) OVERRIDE;
  virtual void Quit() OVERRIDE;
  virtual void ScheduleWork() OVERRIDE;
  virtual void ScheduleDelayedWork(const TimeTicks& delayed_work_time) OVERRIDE;

  virtual void Start(Delegate* delegate);

 private:
  MessageLoop::AutoRunState* state_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpForUI);
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_ANDROID_H_
