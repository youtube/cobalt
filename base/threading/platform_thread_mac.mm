// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"

#import <Foundation/Foundation.h>
#include <dlfcn.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/thread_policy.h>

#include "base/logging.h"
#include "base/threading/thread_local.h"

namespace base {

static ThreadLocalPointer<char> current_thread_name;

// If Cocoa is to be used on more than one thread, it must know that the
// application is multithreaded.  Since it's possible to enter Cocoa code
// from threads created by pthread_thread_create, Cocoa won't necessarily
// be aware that the application is multithreaded.  Spawning an NSThread is
// enough to get Cocoa to set up for multithreaded operation, so this is done
// if necessary before pthread_thread_create spawns any threads.
//
// http://developer.apple.com/documentation/Cocoa/Conceptual/Multithreading/CreatingThreads/chapter_4_section_4.html
void InitThreading() {
  static BOOL multithreaded = [NSThread isMultiThreaded];
  if (!multithreaded) {
    // +[NSObject class] is idempotent.
    [NSThread detachNewThreadSelector:@selector(class)
                             toTarget:[NSObject class]
                           withObject:nil];
    multithreaded = YES;

    DCHECK([NSThread isMultiThreaded]);
  }
}

// static
void PlatformThread::SetName(const char* name) {
  current_thread_name.Set(const_cast<char*>(name));

  // pthread_setname_np is only available in 10.6 or later, so test
  // for it at runtime.
  int (*dynamic_pthread_setname_np)(const char*);
  *reinterpret_cast<void**>(&dynamic_pthread_setname_np) =
      dlsym(RTLD_DEFAULT, "pthread_setname_np");
  if (!dynamic_pthread_setname_np)
    return;

  // Mac OS X does not expose the length limit of the name, so
  // hardcode it.
  const int kMaxNameLength = 63;
  std::string shortened_name = std::string(name).substr(0, kMaxNameLength);
  // pthread_setname() fails (harmlessly) in the sandbox, ignore when it does.
  // See http://crbug.com/47058
  dynamic_pthread_setname_np(shortened_name.c_str());
}

// static
const char* PlatformThread::GetName() {
  return current_thread_name.Get();
}

namespace {

void SetPriorityNormal(mach_port_t mach_thread_id) {
  // Make thread standard policy.
  // Please note that this call could fail in rare cases depending
  // on runtime conditions.
  thread_standard_policy policy;
  kern_return_t result = thread_policy_set(mach_thread_id,
                                           THREAD_STANDARD_POLICY,
                                           (thread_policy_t)&policy,
                                           THREAD_STANDARD_POLICY_COUNT);

  if (result != KERN_SUCCESS)
    VLOG(1) << "thread_policy_set() failure: " << result;
}

// Enables time-contraint policy and priority suitable for low-latency,
// glitch-resistant audio.
void SetPriorityRealtimeAudio(mach_port_t mach_thread_id) {
  kern_return_t result;

  // Increase thread priority to real-time.

  // Please note that the thread_policy_set() calls may fail in
  // rare cases if the kernel decides the system is under heavy load
  // and is unable to handle boosting the thread priority.
  // In these cases we just return early and go on with life.

  // Make thread fixed priority.
  thread_extended_policy_data_t policy;
  policy.timeshare = 0;  // Set to 1 for a non-fixed thread.
  result = thread_policy_set(mach_thread_id,
                             THREAD_EXTENDED_POLICY,
                             (thread_policy_t)&policy,
                             THREAD_EXTENDED_POLICY_COUNT);
  if (result != KERN_SUCCESS) {
    VLOG(1) << "thread_policy_set() failure: " << result;
    return;
  }

  // Set to relatively high priority.
  thread_precedence_policy_data_t precedence;
  precedence.importance = 63;
  result = thread_policy_set(mach_thread_id,
                             THREAD_PRECEDENCE_POLICY,
                             (thread_policy_t)&precedence,
                             THREAD_PRECEDENCE_POLICY_COUNT);
  if (result != KERN_SUCCESS) {
    VLOG(1) << "thread_policy_set() failure: " << result;
    return;
  }

  // Most important, set real-time constraints.

  // Define the guaranteed and max fraction of time for the audio thread.
  // These "duty cycle" values can range from 0 to 1.  A value of 0.5
  // means the scheduler would give half the time to the thread.
  // These values have empirically been found to yield good behavior.
  // Good means that audio performance is high and other threads won't starve.
  const double kGuaranteedAudioDutyCycle = 0.75;
  const double kMaxAudioDutyCycle = 0.85;

  // Define constants determining how much time the audio thread can
  // use in a given time quantum.  All times are in milliseconds.

  // About 128 frames @44.1KHz
  const double kTimeQuantum = 2.9;

  // Time guaranteed each quantum.
  const double kAudioTimeNeeded = kGuaranteedAudioDutyCycle * kTimeQuantum;

  // Maximum time each quantum.
  const double kMaxTimeAllowed = kMaxAudioDutyCycle * kTimeQuantum;

  // Get the conversion factor from milliseconds to absolute time
  // which is what the time-constraints call needs.
  mach_timebase_info_data_t tb_info;
  mach_timebase_info(&tb_info);
  double ms_to_abs_time =
      ((double)tb_info.denom / (double)tb_info.numer) * 1000000;

  thread_time_constraint_policy_data_t time_constraints;
  time_constraints.period = kTimeQuantum * ms_to_abs_time;
  time_constraints.computation = kAudioTimeNeeded * ms_to_abs_time;
  time_constraints.constraint = kMaxTimeAllowed * ms_to_abs_time;
  time_constraints.preemptible = 0;

  result = thread_policy_set(mach_thread_id,
                             THREAD_TIME_CONSTRAINT_POLICY,
                             (thread_policy_t)&time_constraints,
                             THREAD_TIME_CONSTRAINT_POLICY_COUNT);
  if (result != KERN_SUCCESS)
    VLOG(1) << "thread_policy_set() failure: " << result;

  return;
}

}  // anonymous namespace

// static
void PlatformThread::SetThreadPriority(PlatformThreadHandle handle,
                                       ThreadPriority priority) {
  // Convert from pthread_t to mach thread identifier.
  mach_port_t mach_thread_id = pthread_mach_thread_np(handle);

  switch (priority) {
    case kThreadPriority_Normal:
      SetPriorityNormal(mach_thread_id);
      break;
    case kThreadPriority_RealtimeAudio:
      SetPriorityRealtimeAudio(mach_thread_id);
      break;
  }
}

}  // namespace base
