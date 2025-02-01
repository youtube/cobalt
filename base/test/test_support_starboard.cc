// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_pump.h"
#include "base/message_loop/message_pump_default.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "base/synchronization/waitable_event.h"

namespace {

class MessagePumpForUIStub : public base::MessagePumpUIStarboard {
 public:
 
  MessagePumpForUIStub() : base::MessagePumpForUI(), default_pump_ (new base::MessagePumpDefault) { }
  ~MessagePumpForUIStub() override {}

  // Similar to Android, in Starboad tests there isn't a native thread, 
  // as such RunLoop::Run() should be  used to run the loop instead of attaching
  // and delegating to the native  loop. 
  // As such, this override ignores the Attach() request.
  void Attach(base::MessagePump::Delegate* delegate) override {}

  // --- Delegate to the MessagePumpDefault Implementation ---

  virtual void Run(Delegate* delegate) override {
   default_pump_->Run(delegate); 
  }

  virtual void Quit() override {
   default_pump_->Quit(); 
  }

  virtual void ScheduleWork() override {
   default_pump_->ScheduleWork(); 
  }

  virtual void ScheduleDelayedWork(
      const Delegate::NextWorkInfo& next_work_info) override {
   default_pump_->ScheduleDelayedWork(next_work_info); 
  }

 private:
  std::unique_ptr<base::MessagePumpDefault> default_pump_;
};

std::unique_ptr<base::MessagePump> CreateMessagePumpForUIStub() {
  return std::unique_ptr<base::MessagePump>(new MessagePumpForUIStub());
}

}  // namespace

namespace base {


void InitStarboardTestMessageLoop() {
  if (!MessagePump::IsMessagePumpForUIFactoryOveridden())
    MessagePump::OverrideMessagePumpForUIFactory(&CreateMessagePumpForUIStub);
}

}  // namespace base
