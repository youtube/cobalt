// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/test_metrics_log_uploader.h"
#include "components/metrics/metrics_log_uploader.h"

namespace metrics {

TestMetricsLogUploader::TestMetricsLogUploader(
    const MetricsLogUploader::UploadCallback& on_upload_complete)
    : on_upload_complete_(on_upload_complete), is_uploading_(false) {}

TestMetricsLogUploader::~TestMetricsLogUploader() = default;

void TestMetricsLogUploader::CompleteUpload(int response_code) {
  DCHECK(is_uploading_);
  is_uploading_ = false;
  last_reporting_info_.Clear();
  on_upload_complete_.Run(response_code, 0, false);
}

void TestMetricsLogUploader::UploadLog(const std::string& compressed_log_data,
                                       const std::string& log_hash,
                                       const ReportingInfo& reporting_info) {
  DCHECK(!is_uploading_);
  is_uploading_ = true;
  last_reporting_info_ = reporting_info;
}

}  // namespace metrics
