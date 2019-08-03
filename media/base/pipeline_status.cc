// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/pipeline_status.h"

#include "base/bind.h"
#include "base/metrics/histogram.h"

namespace media {

static void ReportAndRun(const std::string& name,
                         const PipelineStatusCB& cb,
                         PipelineStatus status) {
  UMA_HISTOGRAM_ENUMERATION(name, status, PIPELINE_STATUS_MAX);
  cb.Run(status);
}

PipelineStatusCB CreateUMAReportingPipelineCB(const std::string& name,
                                              const PipelineStatusCB& cb) {
  return base::Bind(&ReportAndRun, name, cb);
}

}  // namespace media
