// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_COMPUTE_PRESSURE_HOST_PROCESSOR_INFO_SCANNER_H_
#define SERVICES_DEVICE_COMPUTE_PRESSURE_HOST_PROCESSOR_INFO_SCANNER_H_

#include <vector>

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "services/device/compute_pressure/core_times.h"

namespace device {

// Parses CPU time usage stats from host_processor_info.
//
// This class is not thread-safe. Each instance must be used on the same
// sequence, which must allow blocking I/O. The constructor may be used on a
// different sequence.
class HostProcessorInfoScanner {
 public:
  HostProcessorInfoScanner();
  ~HostProcessorInfoScanner();

  HostProcessorInfoScanner(const HostProcessorInfoScanner&) = delete;
  HostProcessorInfoScanner& operator=(const HostProcessorInfoScanner&) = delete;

  const std::vector<CoreTimes>& core_times() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return core_times_;
  }

  bool Update();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  std::vector<CoreTimes> core_times_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace device

#endif  // SERVICES_DEVICE_COMPUTE_PRESSURE_HOST_PROCESSOR_INFO_SCANNER_H_
