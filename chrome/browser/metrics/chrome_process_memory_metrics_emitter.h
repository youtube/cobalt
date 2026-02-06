// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_PROCESS_MEMORY_METRICS_EMITTER_H_
#define CHROME_BROWSER_METRICS_CHROME_PROCESS_MEMORY_METRICS_EMITTER_H_

#include "components/embedder_support/metrics/process_memory_metrics_emitter.h"

class ChromeProcessMemoryMetricsEmitter : public ProcessMemoryMetricsEmitter {
 public:
  ChromeProcessMemoryMetricsEmitter();
  explicit ChromeProcessMemoryMetricsEmitter(base::ProcessId pid_scope);

  ChromeProcessMemoryMetricsEmitter(const ChromeProcessMemoryMetricsEmitter&) = delete;
  ChromeProcessMemoryMetricsEmitter& operator=(const ChromeProcessMemoryMetricsEmitter&) =
      delete;

 protected:
  ~ChromeProcessMemoryMetricsEmitter() override;

  // ProcessMemoryMetricsEmitter:
  void FetchProcessInfos() override;
  int GetNumberOfExtensions(base::ProcessId pid) override;
};

#endif  // CHROME_BROWSER_METRICS_CHROME_PROCESS_MEMORY_METRICS_EMITTER_H_
