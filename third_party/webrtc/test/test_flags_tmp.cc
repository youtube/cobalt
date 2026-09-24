/*
 *  Copyright (c) 2022 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/test_flags_tmp.h"

#include <string>

#include "absl/flags/flag.h"

ABSL_FLAG(std::string,
          webrtc_test_metrics_output_path,
          "",
          "Path where the test perf metrics should be stored using "
          "api/test/metrics/metric.proto proto format. File will contain "
          "MetricsSet as a root proto. On iOS, this MUST be a file name "
          "and the file will be stored under NSDocumentDirectory.");
