// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/cpu_probe/cpu_probe.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "build/build_config.h"
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/performance_manager/metrics/cpu_probe/cpu_probe_linux.h"
#elif BUILDFLAG(IS_WIN)
#include "chrome/browser/performance_manager/metrics/cpu_probe/cpu_probe_win.h"
#elif BUILDFLAG(IS_MAC)
#include "chrome/browser/performance_manager/metrics/cpu_probe/cpu_probe_mac.h"
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

namespace performance_manager::metrics {

// static
std::unique_ptr<CpuProbe> CpuProbe::Create() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return CpuProbeLinux::Create();
#elif BUILDFLAG(IS_WIN)
  return CpuProbeWin::Create();
#elif BUILDFLAG(IS_MAC)
  return CpuProbeMac::Create();
#else
  return nullptr;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
}

CpuProbe::CpuProbe() = default;

CpuProbe::~CpuProbe() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CpuProbe::StartSampling(base::OnceClosure started_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!got_probe_baseline_) << "got_probe_baseline_ incorrectly reset";

  // Schedule the first CpuProbe update right away. This update result will
  // not be reported but will set `got_probe_baseline_`.
  Update(base::BindOnce(&CpuProbe::OnSamplingStarted, GetWeakPtr(),
                        std::move(started_callback)));
}

void CpuProbe::RequestSample(SampleCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Don't check `got_probe_baseline_` until the result is received since it
  // will be set asynchronously.
  Update(base::BindOnce(&CpuProbe::OnPressureSampleAvailable, GetWeakPtr(),
                        std::move(callback)));
}

void CpuProbe::OnSamplingStarted(base::OnceClosure started_callback,
                                 absl::optional<PressureSample>) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(got_probe_baseline_, false);
  got_probe_baseline_ = true;
  std::move(started_callback).Run();
}

void CpuProbe::OnPressureSampleAvailable(
    SampleCallback callback,
    absl::optional<PressureSample> sample) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK_EQ(got_probe_baseline_, true);
  if (sample.has_value()) {
    CHECK_GE(sample->cpu_utilization, 0.0);
    CHECK_LE(sample->cpu_utilization, 1.0);
  }
  std::move(callback).Run(std::move(sample));
}

}  // namespace performance_manager::metrics
