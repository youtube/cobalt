// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump.h"

#include "base/check.h"
#include "base/message_loop/message_pump_default.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "base/notreached.h"
#include "base/task/task_features.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE)
#include "base/message_loop/message_pump_apple.h"
#endif

namespace base {

namespace {

std::atomic_bool g_align_wake_ups = false;
#if BUILDFLAG(IS_WIN)
bool g_explicit_high_resolution_timer_win = true;
#endif  // BUILDFLAG(IS_WIN)

MessagePump::MessagePumpFactory* message_pump_for_ui_factory_ = nullptr;

}  // namespace

MessagePump::MessagePump() = default;

MessagePump::~MessagePump() = default;

// static
void MessagePump::OverrideMessagePumpForUIFactory(MessagePumpFactory* factory) {
  DCHECK(!message_pump_for_ui_factory_);
  message_pump_for_ui_factory_ = factory;
}

// static
bool MessagePump::IsMessagePumpForUIFactoryOveridden() {
  return message_pump_for_ui_factory_ != nullptr;
}

// static
std::unique_ptr<MessagePump> MessagePump::Create(MessagePumpType type) {
  switch (type) {
    case MessagePumpType::UI:
      if (message_pump_for_ui_factory_)
        return message_pump_for_ui_factory_();
#if BUILDFLAG(IS_APPLE)
      return message_pump_apple::Create();
#elif BUILDFLAG(IS_NACL) || BUILDFLAG(IS_AIX)
      // Currently NaCl and AIX don't have a UI MessagePump.
      // TODO(abarth): Figure out if we need this.
      NOTREACHED();
      return nullptr;
#else
      return std::make_unique<MessagePumpForUI>();
#endif

    case MessagePumpType::IO:
      return std::make_unique<MessagePumpForIO>();

#if BUILDFLAG(IS_ANDROID)
    case MessagePumpType::JAVA:
      return std::make_unique<MessagePumpForUI>();
#endif

#if BUILDFLAG(IS_APPLE)
    case MessagePumpType::NS_RUNLOOP:
      return std::make_unique<MessagePumpNSRunLoop>();
#endif

    case MessagePumpType::CUSTOM:
      NOTREACHED();
      return nullptr;

    case MessagePumpType::DEFAULT:
#if BUILDFLAG(IS_IOS)
      // On iOS, a native runloop is always required to pump system work.
      return std::make_unique<MessagePumpCFRunLoop>();
#else
      return std::make_unique<MessagePumpDefault>();
#endif
  }
}

// static
void MessagePump::InitializeFeatures() {
  g_align_wake_ups = FeatureList::IsEnabled(kAlignWakeUps);
#if BUILDFLAG(IS_WIN)
  g_explicit_high_resolution_timer_win =
      FeatureList::IsEnabled(kExplicitHighResolutionTimerWin);
#endif
}

TimeTicks MessagePump::AdjustDelayedRunTime(TimeTicks earliest_time,
                                            TimeTicks run_time,
                                            TimeTicks latest_time) {
  // Windows relies on the low resolution timer rather than manual wake up
  // alignment.
#if BUILDFLAG(IS_WIN)
  if (g_explicit_high_resolution_timer_win) {
    return earliest_time;
  }
#else  // BUILDFLAG(IS_WIN)
  if (g_align_wake_ups.load(std::memory_order_relaxed)) {
    TimeTicks aligned_run_time = earliest_time.SnappedToNextTick(
        TimeTicks(), GetTaskLeewayForCurrentThread());
    return std::min(aligned_run_time, latest_time);
  }
#endif
  return run_time;
}

}  // namespace base
