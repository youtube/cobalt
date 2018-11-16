// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CALL_STACK_PROFILE_BUILDER_H_
#define COMPONENTS_METRICS_CALL_STACK_PROFILE_BUILDER_H_

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/sampling_heap_profiler/module_cache.h"
#include "base/time/time.h"
#include "components/metrics/call_stack_profile_params.h"
#include "components/metrics/child_call_stack_profile_collector.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

class SampledProfile;

// An instance of the class is meant to be passed to base::StackSamplingProfiler
// to collect profiles. The profiles collected are uploaded via the metrics log.
class CallStackProfileBuilder
    : public base::StackSamplingProfiler::ProfileBuilder {
 public:
  // Frame represents an individual sampled stack frame with module information.
  struct Frame {
    Frame(uintptr_t instruction_pointer, size_t module_index);
    ~Frame();

    // Default constructor to satisfy IPC macros. Do not use explicitly.
    Frame();

    // The sampled instruction pointer within the function.
    uintptr_t instruction_pointer;

    // Index of the module in the associated vector of mofules. We don't
    // represent module state directly here to save space.
    size_t module_index;
  };

  // Sample represents a set of stack frames with some extra information.
  struct Sample {
    Sample();
    Sample(const Sample& sample);
    ~Sample();

    // These constructors are used only during testing.
    Sample(const Frame& frame);
    Sample(const std::vector<Frame>& frames);

    // The entire stack frame when the sample is taken.
    std::vector<Frame> frames;

    // A bit-field indicating which process milestones have passed. This can be
    // used to tell where in the process lifetime the samples are taken. Just
    // as a "lifetime" can only move forward, these bits mark the milestones of
    // the processes life as they occur. Bits can be set but never reset. The
    // actual definition of the individual bits is left to the user of this
    // module.
    uint32_t process_milestones = 0;
  };

  // These milestones of a process lifetime can be passed as process "mile-
  // stones" to CallStackProfileBuilder::SetProcessMilestone(). Be sure to
  // update the translation constants at the top of the .cc file when this is
  // changed.
  enum Milestones : int {
    MAIN_LOOP_START,
    MAIN_NAVIGATION_START,
    MAIN_NAVIGATION_FINISHED,
    FIRST_NONEMPTY_PAINT,

    SHUTDOWN_START,

    MILESTONES_MAX_VALUE
  };

  // |completed_callback| is made when sampling a profile completes. Other
  // threads, including the UI thread, may block on callback completion so this
  // should run as quickly as possible.
  //
  // IMPORTANT NOTE: The callback is invoked on a thread the profiler
  // constructs, rather than on the thread used to construct the profiler, and
  // thus the callback must be callable on any thread.
  CallStackProfileBuilder(
      const CallStackProfileParams& profile_params,
      base::OnceClosure completed_callback = base::OnceClosure());

  ~CallStackProfileBuilder() override;

  // base::StackSamplingProfiler::ProfileBuilder:
  void RecordAnnotations() override;
  void OnSampleCompleted(
      std::vector<base::StackSamplingProfiler::Frame> frames) override;
  void OnProfileCompleted(base::TimeDelta profile_duration,
                          base::TimeDelta sampling_period) override;

  // Sets the callback to use for reporting browser process profiles. This
  // indirection is required to avoid a dependency on unnecessary metrics code
  // in child processes.
  static void SetBrowserProcessReceiverCallback(
      const base::RepeatingCallback<void(base::TimeTicks, SampledProfile)>&
          callback);

  // Sets the current system state that is recorded with each captured stack
  // frame. This is thread-safe so can be called from anywhere. The parameter
  // value should be from an enumeration of the appropriate type with values
  // ranging from 0 to 31, inclusive. This sets bits within Sample field of
  // |process_milestones|. The actual meanings of these bits are defined
  // (globally) by the caller(s).
  static void SetProcessMilestone(int milestone);

  // Sets the CallStackProfileCollector interface from |browser_interface|.
  // This function must be called within child processes.
  static void SetParentProfileCollectorForChildProcess(
      metrics::mojom::CallStackProfileCollectorPtr browser_interface);

 protected:
  // Test seam.
  virtual void PassProfilesToMetricsProvider(SampledProfile sampled_profile);

 private:
  // The collected stack samples in proto buffer message format.
  CallStackProfile proto_profile_;

  // The current sample being recorded.
  Sample sample_;

  // The indexes of samples, indexed by the sample.
  std::map<Sample, int> sample_index_;

  // The indexes of modules, indexed by module's base_address.
  std::map<uintptr_t, size_t> module_index_;

  // The distinct modules in the current profile.
  std::vector<base::ModuleCache::Module> modules_;

  // The process milestones of a previous sample.
  uint32_t milestones_ = 0;

  // Callback made when sampling a profile completes.
  base::OnceClosure completed_callback_;

  // The parameters associated with the sampled profile.
  const CallStackProfileParams profile_params_;

  // The start time of a profile collection.
  const base::TimeTicks profile_start_time_;

  DISALLOW_COPY_AND_ASSIGN(CallStackProfileBuilder);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CALL_STACK_PROFILE_BUILDER_H_
