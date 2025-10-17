// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

#import "base/message_loop/message_pump_mac.h"

#import <Foundation/Foundation.h>

#include <atomic>
#include <limits>
#include <memory>

#include "base/auto_reset.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/mac/call_with_eh_frame.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/message_loop/timer_slack.h"
#include "base/metrics/histogram_samples.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if !BUILDFLAG(IS_IOS)
#import <AppKit/AppKit.h>
#endif  // !BUILDFLAG(IS_IOS)

namespace base {

const CFStringRef kMessageLoopExclusiveRunLoopMode =
    CFSTR("kMessageLoopExclusiveRunLoopMode");

namespace {

// Enables two optimizations in MessagePumpCFRunLoop:
// - Skip calling CFRunLoopTimerSetNextFireDate if the next delayed wake up
//  time hasn't changed.
// - Cancel an already scheduled timer wake up if there is no delayed work.
BASE_FEATURE(kMessagePumpMacDelayedWorkOptimizations,
             "MessagePumpMacDelayedWorkOptimizations",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Caches the state of the "MessagePumpMacDelayedWorkOptimizations"
// feature for efficiency.
std::atomic_bool g_enable_optimizations = false;

// Mask that determines which modes to use.
enum { kCommonModeMask = 0x1, kAllModesMask = 0xf };

// Modes to use for MessagePumpNSApplication that are considered "safe".
// Currently just common and exclusive modes. Ideally, messages would be pumped
// in all modes, but that interacts badly with app modal dialogs (e.g. NSAlert).
enum { kNSApplicationModalSafeModeMask = 0x3 };

void NoOp(void* info) {
}

constexpr CFTimeInterval kCFTimeIntervalMax =
    std::numeric_limits<CFTimeInterval>::max();

#if !BUILDFLAG(IS_IOS)
// Set to true if MessagePumpMac::Create() is called before NSApp is
// initialized.  Only accessed from the main thread.
bool g_not_using_cr_app = false;

// The MessagePump controlling [NSApp run].
MessagePumpNSApplication* g_app_pump;
#endif  // !BUILDFLAG(IS_IOS)

}  // namespace

// A scoper for autorelease pools created from message pump run loops.
// Avoids dirtying up the ScopedNSAutoreleasePool interface for the rare
// case where an autorelease pool needs to be passed in.
class MessagePumpScopedAutoreleasePool {
 public:
  explicit MessagePumpScopedAutoreleasePool(MessagePumpCFRunLoopBase* pump) :
      pool_(pump->CreateAutoreleasePool()) {
  }

  MessagePumpScopedAutoreleasePool(const MessagePumpScopedAutoreleasePool&) =
      delete;
  MessagePumpScopedAutoreleasePool& operator=(
      const MessagePumpScopedAutoreleasePool&) = delete;

  ~MessagePumpScopedAutoreleasePool() { [pool_ drain]; }

 private:
  NSAutoreleasePool* pool_;
};

class MessagePumpCFRunLoopBase::ScopedModeEnabler {
 public:
  ScopedModeEnabler(MessagePumpCFRunLoopBase* owner, int mode_index)
      : owner_(owner), mode_index_(mode_index) {
    CFRunLoopRef loop = owner_->run_loop_;
    CFRunLoopAddTimer(loop, owner_->delayed_work_timer_, mode());
    CFRunLoopAddSource(loop, owner_->work_source_, mode());
    CFRunLoopAddSource(loop, owner_->nesting_deferred_work_source_, mode());
    CFRunLoopAddObserver(loop, owner_->pre_wait_observer_, mode());
    CFRunLoopAddObserver(loop, owner_->after_wait_observer_, mode());
    CFRunLoopAddObserver(loop, owner_->pre_source_observer_, mode());
    CFRunLoopAddObserver(loop, owner_->enter_exit_observer_, mode());
  }

  ScopedModeEnabler(const ScopedModeEnabler&) = delete;
  ScopedModeEnabler& operator=(const ScopedModeEnabler&) = delete;

  ~ScopedModeEnabler() {
    CFRunLoopRef loop = owner_->run_loop_;
    CFRunLoopRemoveObserver(loop, owner_->enter_exit_observer_, mode());
    CFRunLoopRemoveObserver(loop, owner_->pre_source_observer_, mode());
    CFRunLoopRemoveObserver(loop, owner_->pre_wait_observer_, mode());
    CFRunLoopRemoveObserver(loop, owner_->after_wait_observer_, mode());
    CFRunLoopRemoveSource(loop, owner_->nesting_deferred_work_source_, mode());
    CFRunLoopRemoveSource(loop, owner_->work_source_, mode());
    CFRunLoopRemoveTimer(loop, owner_->delayed_work_timer_, mode());
  }

  // This function knows about the AppKit RunLoop modes observed to potentially
  // run tasks posted to Chrome's main thread task runner. Some are internal to
  // AppKit but must be observed to keep Chrome's UI responsive. Others that may
  // be interesting, but are not watched:
  //  - com.apple.hitoolbox.windows.transitionmode
  //  - com.apple.hitoolbox.windows.flushmode
  const CFStringRef& mode() const {
    static const CFStringRef modes[] = {
        // The standard Core Foundation "common modes" constant. Must always be
        // first in this list to match the value of kCommonModeMask.
        kCFRunLoopCommonModes,

        // Mode that only sees Chrome work sources.
        kMessageLoopExclusiveRunLoopMode,

        // Process work when NSMenus are fading out.
        CFSTR("com.apple.hitoolbox.windows.windowfadingmode"),

        // Process work when AppKit is highlighting an item on the main menubar.
        CFSTR("NSUnhighlightMenuRunLoopMode"),
    };
    static_assert(std::size(modes) == kNumModes, "mode size mismatch");
    static_assert((1 << kNumModes) - 1 == kAllModesMask,
                  "kAllModesMask not large enough");

    return modes[mode_index_];
  }

 private:
  const raw_ptr<MessagePumpCFRunLoopBase> owner_;  // Weak. Owns this.
  const int mode_index_;
};

// Must be called on the run loop thread.
void MessagePumpCFRunLoopBase::Run(Delegate* delegate) {
  AutoReset<bool> auto_reset_keep_running(&keep_running_, true);
  // nesting_level_ will be incremented in EnterExitRunLoop, so set
  // run_nesting_level_ accordingly.
  int last_run_nesting_level = run_nesting_level_;
  run_nesting_level_ = nesting_level_ + 1;

  Delegate* last_delegate = delegate_;
  SetDelegate(delegate);

  ScheduleWork();
  DoRun(delegate);

  // Restore the previous state of the object.
  SetDelegate(last_delegate);
  run_nesting_level_ = last_run_nesting_level;
}

void MessagePumpCFRunLoopBase::Quit() {
  if (DoQuit())
    OnDidQuit();
}

void MessagePumpCFRunLoopBase::OnDidQuit() {
  keep_running_ = false;
}

// May be called on any thread.
void MessagePumpCFRunLoopBase::ScheduleWork() {
  CFRunLoopSourceSignal(work_source_);
  CFRunLoopWakeUp(run_loop_);
}

// Must be called on the run loop thread.
void MessagePumpCFRunLoopBase::ScheduleDelayedWork(
    const Delegate::NextWorkInfo& next_work_info) {
  DCHECK(!next_work_info.is_immediate());

  if (g_enable_optimizations.load(std::memory_order_relaxed)) {
    // No-op if the delayed run time hasn't changed.
    if (next_work_info.delayed_run_time == delayed_work_scheduled_at_)
      return;
  } else {
    // Preserve the old behavior of not adjusting the timer when
    // `delayed_run_time.is_max()`.
    //
    // TODO(crbug.com/1335524): Remove this once the
    // "MessagePumpMacDelayedWorkOptimizations" feature is shipped.
    if (next_work_info.delayed_run_time.is_max())
      return;
  }

  if (next_work_info.delayed_run_time.is_max()) {
    CFRunLoopTimerSetNextFireDate(delayed_work_timer_, kCFTimeIntervalMax);
  } else {
    const double delay_seconds = next_work_info.remaining_delay().InSecondsF();

    // The tolerance needs to be set before the fire date or it may be ignored.
    if (timer_slack_ == TIMER_SLACK_MAXIMUM) {
      CFRunLoopTimerSetTolerance(delayed_work_timer_, delay_seconds * 0.5);
    } else {
      CFRunLoopTimerSetTolerance(delayed_work_timer_, 0);
    }

    CFRunLoopTimerSetNextFireDate(delayed_work_timer_,
                                  CFAbsoluteTimeGetCurrent() + delay_seconds);
  }

  delayed_work_scheduled_at_ = next_work_info.delayed_run_time;
}

void MessagePumpCFRunLoopBase::SetTimerSlack(TimerSlack timer_slack) {
  timer_slack_ = timer_slack;
}

#if BUILDFLAG(IS_IOS)
void MessagePumpCFRunLoopBase::Attach(Delegate* delegate) {}

void MessagePumpCFRunLoopBase::Detach() {}
#endif  // BUILDFLAG(IS_IOS)

// Must be called on the run loop thread.
MessagePumpCFRunLoopBase::MessagePumpCFRunLoopBase(int initial_mode_mask)
    : delegate_(nullptr),
      timer_slack_(base::TIMER_SLACK_NONE),
      nesting_level_(0),
      run_nesting_level_(0),
      deepest_nesting_level_(0),
      keep_running_(true),
      delegateless_work_(false) {
  run_loop_ = CFRunLoopGetCurrent();
  CFRetain(run_loop_);

  // Set a repeating timer with a preposterous firing time and interval.  The
  // timer will effectively never fire as-is.  The firing time will be adjusted
  // as needed when ScheduleDelayedWork is called.
  CFRunLoopTimerContext timer_context = CFRunLoopTimerContext();
  timer_context.info = this;
  delayed_work_timer_ = CFRunLoopTimerCreate(NULL,                // allocator
                                             kCFTimeIntervalMax,  // fire time
                                             kCFTimeIntervalMax,  // interval
                                             0,                   // flags
                                             0,                   // priority
                                             RunDelayedWorkTimer,
                                             &timer_context);

  CFRunLoopSourceContext source_context = CFRunLoopSourceContext();
  source_context.info = this;
  source_context.perform = RunWorkSource;
  work_source_ = CFRunLoopSourceCreate(NULL,  // allocator
                                       1,     // priority
                                       &source_context);
  source_context.perform = RunNestingDeferredWorkSource;
  nesting_deferred_work_source_ = CFRunLoopSourceCreate(NULL,  // allocator
                                                        0,     // priority
                                                        &source_context);

  CFRunLoopObserverContext observer_context = CFRunLoopObserverContext();
  observer_context.info = this;
  pre_wait_observer_ = CFRunLoopObserverCreate(NULL,  // allocator
                                               kCFRunLoopBeforeWaiting,
                                               true,  // repeat
                                               0,     // priority
                                               PreWaitObserver,
                                               &observer_context);
  after_wait_observer_ =
      CFRunLoopObserverCreate(NULL,  // allocator
                              kCFRunLoopAfterWaiting,
                              true,  // repeat
                              0,     // priority
                              AfterWaitObserver, &observer_context);
  pre_source_observer_ = CFRunLoopObserverCreate(NULL,  // allocator
                                                 kCFRunLoopBeforeSources,
                                                 true,  // repeat
                                                 0,     // priority
                                                 PreSourceObserver,
                                                 &observer_context);
  enter_exit_observer_ = CFRunLoopObserverCreate(NULL,  // allocator
                                                 kCFRunLoopEntry |
                                                     kCFRunLoopExit,
                                                 true,  // repeat
                                                 0,     // priority
                                                 EnterExitObserver,
                                                 &observer_context);
  SetModeMask(initial_mode_mask);
}

// Ideally called on the run loop thread.  If other run loops were running
// lower on the run loop thread's stack when this object was created, the
// same number of run loops must be running when this object is destroyed.
MessagePumpCFRunLoopBase::~MessagePumpCFRunLoopBase() {
  SetModeMask(0);
  CFRelease(enter_exit_observer_);
  CFRelease(pre_source_observer_);
  CFRelease(pre_wait_observer_);
  CFRelease(after_wait_observer_);
  CFRelease(nesting_deferred_work_source_);
  CFRelease(work_source_);
  CFRelease(delayed_work_timer_);
  CFRelease(run_loop_);
}

// static
void MessagePumpCFRunLoopBase::InitializeFeatures() {
  g_enable_optimizations.store(
      base::FeatureList::IsEnabled(kMessagePumpMacDelayedWorkOptimizations),
      std::memory_order_relaxed);
}

#if BUILDFLAG(IS_IOS)
void MessagePumpCFRunLoopBase::OnAttach() {
  CHECK_EQ(nesting_level_, 0);
  // On iOS: the MessagePump is attached while it's already running.
  nesting_level_ = 1;

  // There could be some native work done after attaching to the loop and before
  // |work_source_| is invoked.
  PushWorkItemScope();
}

void MessagePumpCFRunLoopBase::OnDetach() {
  // This function is called on shutdown. This can happen at either
  // `nesting_level` >=1 or 0:
  //   `nesting_level_ == 0`: When this is detached as part of tear down outside
  //   of a run loop (e.g. ~TaskEnvironment). `nesting_level_ >= 1`: When this
  //   is detached as part of a native shutdown notification ran from the
  //   message pump itself. Nesting levels higher than 1 can happen in
  //   legitimate nesting situations like the browser being dismissed while
  //   displaying a long press context menu (CRWContextMenuController).
  CHECK_GE(nesting_level_, 0);
}
#endif  // BUILDFLAG(IS_IOS)

void MessagePumpCFRunLoopBase::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;

  if (delegate) {
    // If any work showed up but could not be dispatched for want of a
    // delegate, set it up for dispatch again now that a delegate is
    // available.
    if (delegateless_work_) {
      CFRunLoopSourceSignal(work_source_);
      delegateless_work_ = false;
    }
  }
}

// Base version returns a standard NSAutoreleasePool.
AutoreleasePoolType* MessagePumpCFRunLoopBase::CreateAutoreleasePool() {
  return [[NSAutoreleasePool alloc] init];
}

void MessagePumpCFRunLoopBase::SetModeMask(int mode_mask) {
  for (size_t i = 0; i < kNumModes; ++i) {
    bool enable = mode_mask & (0x1 << i);
    if (enable == !enabled_modes_[i]) {
      enabled_modes_[i] =
          enable ? std::make_unique<ScopedModeEnabler>(this, i) : nullptr;
    }
  }
}

int MessagePumpCFRunLoopBase::GetModeMask() const {
  int mask = 0;
  for (size_t i = 0; i < kNumModes; ++i)
    mask |= enabled_modes_[i] ? (0x1 << i) : 0;
  return mask;
}

void MessagePumpCFRunLoopBase::PopWorkItemScope() {
  // A WorkItemScope should never have been pushed unless the loop was entered.
  DCHECK_NE(nesting_level_, 0);
  // If no WorkItemScope was pushed it cannot be popped.
  DCHECK_GT(stack_.size(), 0u);

  stack_.pop();
}

void MessagePumpCFRunLoopBase::PushWorkItemScope() {
  // A WorkItemScope should never be pushed unless the loop was entered.
  DCHECK_NE(nesting_level_, 0);

  // See RunWork() comments on why the size of |stack| is never bigger than
  // |nesting_level_| even in nested loops.
  DCHECK_LT(stack_.size(), static_cast<size_t>(nesting_level_));

  if (delegate_) {
    stack_.push(delegate_->BeginWorkItem());
  } else {
    stack_.push(absl::nullopt);
  }
}

// Called from the run loop.
// static
void MessagePumpCFRunLoopBase::RunDelayedWorkTimer(CFRunLoopTimerRef timer,
                                                   void* info) {
  MessagePumpCFRunLoopBase* self = static_cast<MessagePumpCFRunLoopBase*>(info);
  // The timer fired, assume we have work and let RunWork() figure out what to
  // do and what to schedule after.
  base::mac::CallWithEHFrame(^{
    // It would be incorrect to expect that `self->delayed_work_scheduled_at_`
    // is smaller than or equal to `TimeTicks::Now()` because the fire date of a
    // CFRunLoopTimer can be adjusted slightly.
    // https://developer.apple.com/documentation/corefoundation/1543570-cfrunlooptimercreate?language=objc
    DCHECK(!self->delayed_work_scheduled_at_.is_max());

    self->delayed_work_scheduled_at_ = base::TimeTicks::Max();
    self->RunWork();
  });
}

// Called from the run loop.
// static
void MessagePumpCFRunLoopBase::RunWorkSource(void* info) {
  MessagePumpCFRunLoopBase* self = static_cast<MessagePumpCFRunLoopBase*>(info);
  base::mac::CallWithEHFrame(^{
    self->RunWork();
  });
}

// Called by MessagePumpCFRunLoopBase::RunWorkSource and RunDelayedWorkTimer.
bool MessagePumpCFRunLoopBase::RunWork() {
  if (!delegate_) {
    // This point can be reached with a nullptr |delegate_| if Run is not on the
    // stack but foreign code is spinning the CFRunLoop.  Arrange to come back
    // here when a delegate is available.
    delegateless_work_ = true;
    return false;
  }
  if (!keep_running())
    return false;

  // The NSApplication-based run loop only drains the autorelease pool at each
  // UI event (NSEvent).  The autorelease pool is not drained for each
  // CFRunLoopSource target that's run.  Use a local pool for any autoreleased
  // objects if the app is not currently handling a UI event to ensure they're
  // released promptly even in the absence of UI events.
  MessagePumpScopedAutoreleasePool autorelease_pool(this);

  // Pop the current work item scope as it captures any native work happening
  // *between* DoWork()'s. This DoWork() happens in sequence to that native
  // work, not nested within it.
  PopWorkItemScope();
  Delegate::NextWorkInfo next_work_info = delegate_->DoWork();
  // DoWork() (and its own work item coverage) is over so push a new scope to
  // cover any native work that could possibly happen before the next RunWork().
  PushWorkItemScope();

  if (next_work_info.is_immediate()) {
    CFRunLoopSourceSignal(work_source_);
    return true;
  } else {
    // This adjusts the next delayed wake up time (potentially cancels an
    // already scheduled wake up if there is no delayed work).
    ScheduleDelayedWork(next_work_info);
    return false;
  }
}

void MessagePumpCFRunLoopBase::RunIdleWork() {
  if (!delegate_) {
    // This point can be reached with a nullptr delegate_ if Run is not on the
    // stack but foreign code is spinning the CFRunLoop.
    return;
  }
  if (!keep_running())
    return;
  // The NSApplication-based run loop only drains the autorelease pool at each
  // UI event (NSEvent).  The autorelease pool is not drained for each
  // CFRunLoopSource target that's run.  Use a local pool for any autoreleased
  // objects if the app is not currently handling a UI event to ensure they're
  // released promptly even in the absence of UI events.
  MessagePumpScopedAutoreleasePool autorelease_pool(this);
  bool did_work = delegate_->DoIdleWork();
  if (did_work)
    CFRunLoopSourceSignal(work_source_);
}

// Called from the run loop.
// static
void MessagePumpCFRunLoopBase::RunNestingDeferredWorkSource(void* info) {
  MessagePumpCFRunLoopBase* self = static_cast<MessagePumpCFRunLoopBase*>(info);
  base::mac::CallWithEHFrame(^{
    self->RunNestingDeferredWork();
  });
}

// Called by MessagePumpCFRunLoopBase::RunNestingDeferredWorkSource.
void MessagePumpCFRunLoopBase::RunNestingDeferredWork() {
  if (!delegate_) {
    // This point can be reached with a nullptr |delegate_| if Run is not on the
    // stack but foreign code is spinning the CFRunLoop.  There's no sense in
    // attempting to do any work or signalling the work sources because
    // without a delegate, work is not possible.
    return;
  }

  // Attempt to do work, if there's any more work to do this call will re-signal
  // |work_source_| and keep things going; otherwise, PreWaitObserver will be
  // invoked by the native pump to declare us idle.
  RunWork();
}

void MessagePumpCFRunLoopBase::BeforeWait() {
  if (!delegate_) {
    // This point can be reached with a nullptr |delegate_| if Run is not on the
    // stack but foreign code is spinning the CFRunLoop.
    return;
  }
  delegate_->BeforeWait();
}

// Called before the run loop goes to sleep or exits, or processes sources.
void MessagePumpCFRunLoopBase::MaybeScheduleNestingDeferredWork() {
  // deepest_nesting_level_ is set as run loops are entered.  If the deepest
  // level encountered is deeper than the current level, a nested loop
  // (relative to the current level) ran since the last time nesting-deferred
  // work was scheduled.  When that situation is encountered, schedule
  // nesting-deferred work in case any work was deferred because nested work
  // was disallowed.
  if (deepest_nesting_level_ > nesting_level_) {
    deepest_nesting_level_ = nesting_level_;
    CFRunLoopSourceSignal(nesting_deferred_work_source_);
  }
}

// Called from the run loop.
// static
void MessagePumpCFRunLoopBase::PreWaitObserver(CFRunLoopObserverRef observer,
                                               CFRunLoopActivity activity,
                                               void* info) {
  MessagePumpCFRunLoopBase* self = static_cast<MessagePumpCFRunLoopBase*>(info);
  base::mac::CallWithEHFrame(^{
    // Current work item tracking needs to go away since execution will stop.
    // Matches the PushWorkItemScope() in AfterWaitObserver() (with an arbitrary
    // amount of matching Pop/Push in between when running work items).
    self->PopWorkItemScope();

    // Attempt to do some idle work before going to sleep.
    self->RunIdleWork();

    // The run loop is about to go to sleep.  If any of the work done since it
    // started or woke up resulted in a nested run loop running,
    // nesting-deferred work may have accumulated.  Schedule it for processing
    // if appropriate.
    self->MaybeScheduleNestingDeferredWork();

    // Notify the delegate that the loop is about to sleep.
    self->BeforeWait();
  });
}

// Called from the run loop.
// static
void MessagePumpCFRunLoopBase::AfterWaitObserver(CFRunLoopObserverRef observer,
                                                 CFRunLoopActivity activity,
                                                 void* info) {
  MessagePumpCFRunLoopBase* self = static_cast<MessagePumpCFRunLoopBase*>(info);
  base::mac::CallWithEHFrame(^{
    // Emerging from sleep, any work happening after this (outside of a
    // RunWork()) should be considered native work. Matching PopWorkItemScope()
    // is in BeforeWait().
    self->PushWorkItemScope();
  });
}

// Called from the run loop.
// static
void MessagePumpCFRunLoopBase::PreSourceObserver(CFRunLoopObserverRef observer,
                                                 CFRunLoopActivity activity,
                                                 void* info) {
  MessagePumpCFRunLoopBase* self = static_cast<MessagePumpCFRunLoopBase*>(info);

  // The run loop has reached the top of the loop and is about to begin
  // processing sources.  If the last iteration of the loop at this nesting
  // level did not sleep or exit, nesting-deferred work may have accumulated
  // if a nested loop ran.  Schedule nesting-deferred work for processing if
  // appropriate.
  base::mac::CallWithEHFrame(^{
    self->MaybeScheduleNestingDeferredWork();
  });
}

// Called from the run loop.
// static
void MessagePumpCFRunLoopBase::EnterExitObserver(CFRunLoopObserverRef observer,
                                                 CFRunLoopActivity activity,
                                                 void* info) {
  MessagePumpCFRunLoopBase* self = static_cast<MessagePumpCFRunLoopBase*>(info);

  switch (activity) {
    case kCFRunLoopEntry:
      ++self->nesting_level_;

      // There could be some native work done after entering the loop and before
      // the next observer.
      self->PushWorkItemScope();
      if (self->nesting_level_ > self->deepest_nesting_level_) {
        self->deepest_nesting_level_ = self->nesting_level_;
      }
      break;

    case kCFRunLoopExit:
      // Not all run loops go to sleep.  If a run loop is stopped before it
      // goes to sleep due to a CFRunLoopStop call, or if the timeout passed
      // to CFRunLoopRunInMode expires, the run loop may proceed directly from
      // handling sources to exiting without any sleep.  This most commonly
      // occurs when CFRunLoopRunInMode is passed a timeout of 0, causing it
      // to make a single pass through the loop and exit without sleep.  Some
      // native loops use CFRunLoop in this way.  Because PreWaitObserver will
      // not be called in these case, MaybeScheduleNestingDeferredWork needs
      // to be called here, as the run loop exits.
      //
      // MaybeScheduleNestingDeferredWork consults self->nesting_level_
      // to determine whether to schedule nesting-deferred work.  It expects
      // the nesting level to be set to the depth of the loop that is going
      // to sleep or exiting.  It must be called before decrementing the
      // value so that the value still corresponds to the level of the exiting
      // loop.
      base::mac::CallWithEHFrame(^{
        self->MaybeScheduleNestingDeferredWork();
      });

      // Current work item tracking needs to go away since execution will stop.
      self->PopWorkItemScope();

      --self->nesting_level_;
      break;

    default:
      break;
  }

  base::mac::CallWithEHFrame(^{
    self->EnterExitRunLoop(activity);
  });
}

// Called by MessagePumpCFRunLoopBase::EnterExitRunLoop.  The default
// implementation is a no-op.
void MessagePumpCFRunLoopBase::EnterExitRunLoop(CFRunLoopActivity activity) {
}

MessagePumpCFRunLoop::MessagePumpCFRunLoop()
    : MessagePumpCFRunLoopBase(kCommonModeMask), quit_pending_(false) {}

MessagePumpCFRunLoop::~MessagePumpCFRunLoop() {}

// Called by MessagePumpCFRunLoopBase::DoRun.  If other CFRunLoopRun loops were
// running lower on the run loop thread's stack when this object was created,
// the same number of CFRunLoopRun loops must be running for the outermost call
// to Run.  Run/DoRun are reentrant after that point.
void MessagePumpCFRunLoop::DoRun(Delegate* delegate) {
  // This is completely identical to calling CFRunLoopRun(), except autorelease
  // pool management is introduced.
  int result;
  do {
    MessagePumpScopedAutoreleasePool autorelease_pool(this);
    result = CFRunLoopRunInMode(kCFRunLoopDefaultMode,
                                kCFTimeIntervalMax,
                                false);
  } while (result != kCFRunLoopRunStopped && result != kCFRunLoopRunFinished);
}

// Must be called on the run loop thread.
bool MessagePumpCFRunLoop::DoQuit() {
  // Stop the innermost run loop managed by this MessagePumpCFRunLoop object.
  if (nesting_level() == run_nesting_level()) {
    // This object is running the innermost loop, just stop it.
    CFRunLoopStop(run_loop());
    return true;
  } else {
    // There's another loop running inside the loop managed by this object.
    // In other words, someone else called CFRunLoopRunInMode on the same
    // thread, deeper on the stack than the deepest Run call.  Don't preempt
    // other run loops, just mark this object to quit the innermost Run as
    // soon as the other inner loops not managed by Run are done.
    quit_pending_ = true;
    return false;
  }
}

// Called by MessagePumpCFRunLoopBase::EnterExitObserver.
void MessagePumpCFRunLoop::EnterExitRunLoop(CFRunLoopActivity activity) {
  if (activity == kCFRunLoopExit &&
      nesting_level() == run_nesting_level() &&
      quit_pending_) {
    // Quit was called while loops other than those managed by this object
    // were running further inside a run loop managed by this object.  Now
    // that all unmanaged inner run loops are gone, stop the loop running
    // just inside Run.
    CFRunLoopStop(run_loop());
    quit_pending_ = false;
    OnDidQuit();
  }
}

MessagePumpNSRunLoop::MessagePumpNSRunLoop()
    : MessagePumpCFRunLoopBase(kCommonModeMask) {
  CFRunLoopSourceContext source_context = CFRunLoopSourceContext();
  source_context.perform = NoOp;
  quit_source_ = CFRunLoopSourceCreate(NULL,  // allocator
                                       0,     // priority
                                       &source_context);
  CFRunLoopAddSource(run_loop(), quit_source_, kCFRunLoopCommonModes);
}

MessagePumpNSRunLoop::~MessagePumpNSRunLoop() {
  CFRunLoopRemoveSource(run_loop(), quit_source_, kCFRunLoopCommonModes);
  CFRelease(quit_source_);
}

void MessagePumpNSRunLoop::DoRun(Delegate* delegate) {
  while (keep_running()) {
    // NSRunLoop manages autorelease pools itself.
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                             beforeDate:[NSDate distantFuture]];
  }
}

bool MessagePumpNSRunLoop::DoQuit() {
  CFRunLoopSourceSignal(quit_source_);
  CFRunLoopWakeUp(run_loop());
  return true;
}

#if BUILDFLAG(IS_IOS)
MessagePumpUIApplication::MessagePumpUIApplication()
    : MessagePumpCFRunLoopBase(kCommonModeMask), run_loop_(NULL) {}

MessagePumpUIApplication::~MessagePumpUIApplication() {}

void MessagePumpUIApplication::DoRun(Delegate* delegate) {
  NOTREACHED();
}

bool MessagePumpUIApplication::DoQuit() {
  NOTREACHED();
  return false;
}

void MessagePumpUIApplication::Attach(Delegate* delegate) {
  DCHECK(!run_loop_);
  run_loop_ = new RunLoop();

  CHECK(run_loop_->BeforeRun());
  SetDelegate(delegate);

  OnAttach();
}

void MessagePumpUIApplication::Detach() {
  DCHECK(run_loop_);
  run_loop_->AfterRun();
  SetDelegate(nullptr);
  run_loop_ = nullptr;

  OnDetach();
}

#else

ScopedPumpMessagesInPrivateModes::ScopedPumpMessagesInPrivateModes() {
  DCHECK(g_app_pump);
  DCHECK_EQ(kNSApplicationModalSafeModeMask, g_app_pump->GetModeMask());
  // Pumping events in private runloop modes is known to interact badly with
  // app modal windows like NSAlert.
  if ([NSApp modalWindow])
    return;
  g_app_pump->SetModeMask(kAllModesMask);
}

ScopedPumpMessagesInPrivateModes::~ScopedPumpMessagesInPrivateModes() {
  DCHECK(g_app_pump);
  g_app_pump->SetModeMask(kNSApplicationModalSafeModeMask);
}

int ScopedPumpMessagesInPrivateModes::GetModeMaskForTest() {
  return g_app_pump ? g_app_pump->GetModeMask() : -1;
}

MessagePumpNSApplication::MessagePumpNSApplication()
    : MessagePumpCFRunLoopBase(kNSApplicationModalSafeModeMask),
      running_own_loop_(false),
      quit_pending_(false) {
  DCHECK_EQ(nullptr, g_app_pump);
  g_app_pump = this;
}

MessagePumpNSApplication::~MessagePumpNSApplication() {
  DCHECK_EQ(this, g_app_pump);
  g_app_pump = nullptr;
}

void MessagePumpNSApplication::DoRun(Delegate* delegate) {
  bool last_running_own_loop_ = running_own_loop_;

  // NSApp must be initialized by calling:
  // [{some class which implements CrAppProtocol} sharedApplication]
  // Most likely candidates are CrApplication or BrowserCrApplication.
  // These can be initialized from C++ code by calling
  // RegisterCrApp() or RegisterBrowserCrApp().
  CHECK(NSApp);

  if (![NSApp isRunning]) {
    running_own_loop_ = false;
    // NSApplication manages autorelease pools itself when run this way.
    [NSApp run];
  } else {
    running_own_loop_ = true;
    NSDate* distant_future = [NSDate distantFuture];
    while (keep_running()) {
      MessagePumpScopedAutoreleasePool autorelease_pool(this);
      NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                          untilDate:distant_future
                                             inMode:NSDefaultRunLoopMode
                                            dequeue:YES];
      if (event) {
        [NSApp sendEvent:event];
      }
    }
  }

  running_own_loop_ = last_running_own_loop_;
}

bool MessagePumpNSApplication::DoQuit() {
  // If the app is displaying a modal window in a native run loop, we can only
  // quit our run loop after the window is closed. Otherwise the [NSApplication
  // stop] below will apply to the modal window run loop instead. To work around
  // this, the quit is applied when we re-enter our own run loop after the
  // window is gone (see MessagePumpNSApplication::EnterExitRunLoop).
  if (nesting_level() > run_nesting_level() &&
      [[NSApplication sharedApplication] modalWindow] != nil) {
    quit_pending_ = true;
    return false;
  }

  if (!running_own_loop_) {
    [[NSApplication sharedApplication] stop:nil];
  }

  // Send a fake event to wake the loop up.
  [NSApp postEvent:[NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                      location:NSZeroPoint
                                 modifierFlags:0
                                     timestamp:0
                                  windowNumber:0
                                       context:nil
                                       subtype:0
                                         data1:0
                                         data2:0]
           atStart:NO];
  return true;
}

void MessagePumpNSApplication::EnterExitRunLoop(CFRunLoopActivity activity) {
  // If we previously tried quitting while a modal window was active, check if
  // the window is gone now and we're no longer nested in a system run loop.
  if (activity == kCFRunLoopEntry && quit_pending_ &&
      nesting_level() <= run_nesting_level() &&
      [[NSApplication sharedApplication] modalWindow] == nil) {
    quit_pending_ = false;
    if (DoQuit())
      OnDidQuit();
  }
}

MessagePumpCrApplication::MessagePumpCrApplication() {
}

MessagePumpCrApplication::~MessagePumpCrApplication() {
}

// Prevents an autorelease pool from being created if the app is in the midst of
// handling a UI event because various parts of AppKit depend on objects that
// are created while handling a UI event to be autoreleased in the event loop.
// An example of this is NSWindowController. When a window with a window
// controller is closed it goes through a stack like this:
// (Several stack frames elided for clarity)
//
// #0 [NSWindowController autorelease]
// #1 DoAClose
// #2 MessagePumpCFRunLoopBase::DoWork()
// #3 [NSRunLoop run]
// #4 [NSButton performClick:]
// #5 [NSWindow sendEvent:]
// #6 [NSApp sendEvent:]
// #7 [NSApp run]
//
// -performClick: spins a nested run loop. If the pool created in DoWork was a
// standard NSAutoreleasePool, it would release the objects that were
// autoreleased into it once DoWork released it. This would cause the window
// controller, which autoreleased itself in frame #0, to release itself, and
// possibly free itself. Unfortunately this window controller controls the
// window in frame #5. When the stack is unwound to frame #5, the window would
// no longer exists and crashes may occur. Apple gets around this by never
// releasing the pool it creates in frame #4, and letting frame #7 clean it up
// when it cleans up the pool that wraps frame #7. When an autorelease pool is
// released it releases all other pools that were created after it on the
// autorelease pool stack.
//
// CrApplication is responsible for setting handlingSendEvent to true just
// before it sends the event through the event handling mechanism, and
// returning it to its previous value once the event has been sent.
AutoreleasePoolType* MessagePumpCrApplication::CreateAutoreleasePool() {
  if (MessagePumpMac::IsHandlingSendEvent())
    return nil;
  return MessagePumpNSApplication::CreateAutoreleasePool();
}

// static
bool MessagePumpMac::UsingCrApp() {
  DCHECK([NSThread isMainThread]);

  // If NSApp is still not initialized, then the subclass used cannot
  // be determined.
  DCHECK(NSApp);

  // The pump was created using MessagePumpNSApplication.
  if (g_not_using_cr_app)
    return false;

  return [NSApp conformsToProtocol:@protocol(CrAppProtocol)];
}

// static
bool MessagePumpMac::IsHandlingSendEvent() {
  DCHECK([NSApp conformsToProtocol:@protocol(CrAppProtocol)]);
  NSObject<CrAppProtocol>* app = static_cast<NSObject<CrAppProtocol>*>(NSApp);
  return [app isHandlingSendEvent];
}
#endif  // BUILDFLAG(IS_IOS)

// static
std::unique_ptr<MessagePump> MessagePumpMac::Create() {
  if ([NSThread isMainThread]) {
#if BUILDFLAG(IS_IOS)
    return std::make_unique<MessagePumpUIApplication>();
#else
    if ([NSApp conformsToProtocol:@protocol(CrAppProtocol)])
      return std::make_unique<MessagePumpCrApplication>();

    // The main-thread MessagePump implementations REQUIRE an NSApp.
    // Executables which have specific requirements for their
    // NSApplication subclass should initialize appropriately before
    // creating an event loop.
    [NSApplication sharedApplication];
    g_not_using_cr_app = true;
    return std::make_unique<MessagePumpNSApplication>();
#endif
  }

  return std::make_unique<MessagePumpNSRunLoop>();
}

}  // namespace base
