// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_CPU_PROBE_CPU_PROBE_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_CPU_PROBE_CPU_PROBE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "chrome/browser/performance_manager/metrics/cpu_probe/pressure_sample.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_manager::metrics {

// Interface for retrieving the CPU load from the underlying OS on request.
//
// Operating systems differ in how they summarize the info needed to derive the
// compute pressure state. For example, the Linux kernel exposes CPU utilization
// as a summary over the device's entire uptime, while the Windows WMI exposes
// CPU utilization over the last second.
//
// This interface abstracts over the differences with a unified model where the
// implementation is responsible for integrating over the time between two
// Update() calls.
//
// Callers must call StartSampling() to establish a baseline. After this they
// can call RequestUpdate(), generally on a fixed schedule. Each call returns
// the total CPU usage of processors since the baseline or the last
// RequestUpdate() call.
//
// Instances are not thread-safe and should be used on the same sequence.
class CpuProbe {
 public:
  using SampleCallback =
      base::OnceCallback<void(absl::optional<PressureSample>)>;

  // Instantiates the CpuProbe subclass most suitable for the current platform.
  //
  // Returns nullptr if no suitable implementation exists.
  static std::unique_ptr<CpuProbe> Create();

  CpuProbe(const CpuProbe&) = delete;
  CpuProbe& operator=(const CpuProbe&) = delete;

  virtual ~CpuProbe();

  // Samples the CPU load to get a baseline for calls to RequestSample().
  // `started_callback` will be invoked once the baseline is available, so tests
  // can verify the timing.
  void StartSampling(base::OnceClosure started_callback = base::DoNothing());

  // Invokes `callback` with the CPU load since the last call to
  // RequestSample(). It's an error to call this without calling StartSampling()
  // first.
  void RequestSample(SampleCallback callback);

 protected:
  // The constructor is intentionally only exposed to subclasses. Production
  // code must use the Create() factory method.
  CpuProbe();

  // Implemented by subclasses to retrieve the CPU usage for different operating
  // systems. It must call `callback` with the result.
  virtual void Update(SampleCallback callback) = 0;

  // Called to return a WeakPtr to the CpuProbe. Subclasses must own a
  // WeakPtrFactory to implement this.
  virtual base::WeakPtr<CpuProbe> GetWeakPtr() = 0;

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  // Called with the result of an Update() triggered by StartSampling(). Will do
  // some bookkeeping and then call `started_callback`, ignoring the
  // PressureSample.
  void OnSamplingStarted(base::OnceClosure started_callback,
                         absl::optional<PressureSample>);

  // Called with the result of an Update() triggered by RequestSample(). Will do
  // some bookkeeping and then pass `sample` to `callback`.
  void OnPressureSampleAvailable(SampleCallback callback,
                                 absl::optional<PressureSample> sample);

  // True if the CpuProbe state will be reported after the next update.
  //
  // The PressureSample reported by many CpuProbe implementations relies
  // on the differences observed between two Update() calls. For this reason,
  // the PressureSample reported after a first Update() call is not
  // reported via the SampleCallback.
  bool got_probe_baseline_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
};

}  // namespace performance_manager::metrics

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_METRICS_CPU_PROBE_CPU_PROBE_H_
