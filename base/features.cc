// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/features.h"
#include "base/feature_list.h"

#include "base/cpu_reduction_experiment.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/threading/platform_thread.h"
#include "build/buildflag.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include "base/message_loop/message_pump_epoll.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "base/files/file.h"
#include "base/message_loop/message_pump_apple.h"
#include "base/message_loop/message_pump_kqueue.h"
#include "base/synchronization/condition_variable.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/input_hint_checker.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "base/task/sequence_manager/thread_controller_power_monitor.h"
#include "base/threading/platform_thread_win.h"
#endif

namespace base::features {

// Alphabetical:

// Enforce that writeable file handles passed to untrusted processes are not
// backed by executable files.
BASE_FEATURE(kEnforceNoExecutableFileHandles,
             "EnforceNoExecutableFileHandles",
             FEATURE_ENABLED_BY_DEFAULT);

// Optimizes parsing and loading of data: URLs.
BASE_FEATURE(kOptimizeDataUrls, "OptimizeDataUrls", FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kSupportsUserDataFlatHashMap,
             "SupportsUserDataFlatHashMap",
             FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Force to enable LowEndDeviceMode partially on Android mid-range devices.
// Such devices aren't considered low-end, but we'd like experiment with
// a subset of low-end features to see if we get a good memory vs. performance
// tradeoff.
//
// TODO(crbug.com/1434873): |#if| out 32-bit before launching or going to
// high Stable %, because we will enable the feature only for <8GB 64-bit
// devices, where we didn't ship yet. However, we first need a larger
// population to collect data.
BASE_FEATURE(kPartialLowEndModeOnMidRangeDevices,
             "PartialLowEndModeOnMidRangeDevices",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID)

void Init(EmitThreadControllerProfilerMetadata
              emit_thread_controller_profiler_metadata) {
  InitializeCpuReductionExperiment();
  sequence_manager::internal::SequenceManagerImpl::InitializeFeatures();
  sequence_manager::internal::ThreadController::InitializeFeatures(
      emit_thread_controller_profiler_metadata);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  MessagePumpEpoll::InitializeFeatures();
#endif

#if BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_CHROMEOS)
  PlatformThread::InitializeFeatures();
#endif

#if BUILDFLAG(IS_APPLE)
  ConditionVariable::InitializeFeatures();
  File::InitializeFeatures();
  MessagePumpCFRunLoopBase::InitializeFeatures();
  MessagePumpKqueue::InitializeFeatures();
#endif

#if BUILDFLAG(IS_ANDROID)
  android::InputHintChecker::InitializeFeatures();
#endif

#if BUILDFLAG(IS_WIN)
  sequence_manager::internal::ThreadControllerPowerMonitor::
      InitializeFeatures();
  InitializePlatformThreadFeatures();
#endif
}

}  // namespace base::features
