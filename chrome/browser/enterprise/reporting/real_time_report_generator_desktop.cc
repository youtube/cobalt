// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/real_time_report_generator_desktop.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "components/enterprise/browser/reporting/report_type.h"
#include "components/enterprise/common/proto/extensions_workflow_events.pb.h"

namespace enterprise_reporting {

RealTimeReportGeneratorDesktop::RealTimeReportGeneratorDesktop() = default;
RealTimeReportGeneratorDesktop::~RealTimeReportGeneratorDesktop() = default;

std::vector<std::unique_ptr<google::protobuf::MessageLite>>
RealTimeReportGeneratorDesktop::Generate(
    RealTimeReportGenerator::ReportType type,
    const RealTimeReportGenerator::Data& data) {
  std::vector<std::unique_ptr<google::protobuf::MessageLite>> reports;
  switch (type) {
    case RealTimeReportGenerator::ReportType::kExtensionRequest:
      for (auto& report : extension_request_report_generator_.Generate(data))
        reports.push_back(std::move(report));
  }
  return reports;
}

}  // namespace enterprise_reporting
