// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CHILD_CALL_STACK_PROFILE_COLLECTOR_H_
#define COMPONENTS_METRICS_CHILD_CALL_STACK_PROFILE_COLLECTOR_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "components/metrics/public/interfaces/call_stack_profile_collector.mojom.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace service_manager {
class InterfaceProvider;
}

namespace metrics {

// ChildCallStackProfileCollector collects stacks at startup, caching them
// internally until a CallStackProfileCollector interface is available. If a
// CallStackProfileCollector is provided via the InterfaceProvider supplied to
// SetParentProfileCollector, the cached stacks are sent via that interface. All
// future stacks received via callbacks supplied by GetProfilerCallback are sent
// via that interface as well.
//
// If no CallStackProfileCollector is provided via InterfaceProvider, any cached
// stacks and all future stacks received via callbacks supplied by
// GetProfilerCallback are flushed. In typical usage this should not happen
// because the browser is expected to always supply a CallStackProfileCollector.
//
// This class is only necessary if a CallStackProfileCollector is not available
// at the time the profiler is created. Otherwise the CallStackProfileCollector
// can be used directly.
//
// To use, create as a leaky lazy instance:
//
//   base::LazyInstance<metrics::ChildCallStackProfileCollector>::Leaky
//       g_call_stack_profile_collector = LAZY_INSTANCE_INITIALIZER;
//
// Then, invoke Collect() in CallStackProfileBuilder::OnProfileCompleted() to
// collect a profile.
//
// When the mojo InterfaceProvider becomes available, provide it via
// SetParentProfileCollector().
class ChildCallStackProfileCollector {
 public:
  ChildCallStackProfileCollector();
  ~ChildCallStackProfileCollector();

  // Sets the CallStackProfileCollector interface from |parent_collector|. This
  // function MUST be invoked exactly once, regardless of whether
  // |parent_collector| is null, as it flushes pending data in either case.
  void SetParentProfileCollector(
      metrics::mojom::CallStackProfileCollectorPtr parent_collector);

  // Collects |profile| whose collection start time is |start_timestamp|.
  void Collect(base::TimeTicks start_timestamp, SampledProfile profile);

 private:
  friend class ChildCallStackProfileCollectorTest;

  // Bundles together a collected profile and the collection state for
  // storage, pending availability of the parent mojo interface. |profile|
  // is not const& because it must be passed with std::move.
  struct ProfileState {
    ProfileState();
    ProfileState(ProfileState&&);
    ProfileState(base::TimeTicks start_timestamp, SampledProfile profile);
    ~ProfileState();

    ProfileState& operator=(ProfileState&&);

    base::TimeTicks start_timestamp;

    // The sampled profile.
    SampledProfile profile;

   private:
    DISALLOW_COPY_AND_ASSIGN(ProfileState);
  };

  // This object may be accessed on any thread, including the profiler
  // thread. The expected use case for the object is to be created and have
  // GetProfilerCallback before the message loop starts, which prevents the use
  // of PostTask and the like for inter-thread communication.
  base::Lock lock_;

  // Whether to retain the profile when the interface is not set. Remains true
  // until the invocation of SetParentProfileCollector(), at which point it is
  // false for the rest of the object lifetime.
  bool retain_profiles_ = true;

  // The task runner associated with the parent interface.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  // The interface to use to collect the stack profiles provided to this
  // object. Initially null until SetParentProfileCollector() is invoked, at
  // which point it may either become set or remain null. If set, stacks are
  // collected via the interface, otherwise they are ignored.
  mojom::CallStackProfileCollectorPtr parent_collector_;

  // Profiles being cached by this object, pending a parent interface to be
  // supplied.
  std::vector<ProfileState> profiles_;

  DISALLOW_COPY_AND_ASSIGN(ChildCallStackProfileCollector);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CHILD_CALL_STACK_PROFILE_COLLECTOR_H_
