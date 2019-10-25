// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/h5vcc/h5vcc_metrics.h"

#include "base/metrics/statistics_recorder.h"
#include "base/metrics/histogram.h"

namespace cobalt {
namespace h5vcc {

script::Sequence<std::string> H5vccMetrics::GetHistograms(const std::string& query) const {
  base::StatisticsRecorder::ImportProvidedHistograms();
  //HistogramSynchronizer::FetchHistograms();

  script::Sequence<std::string> histograms_list;
  for (base::HistogramBase* histogram :
       base::StatisticsRecorder::Sort(base::StatisticsRecorder::WithName(
           base::StatisticsRecorder::GetHistograms(), query))) {
    std::string ascii_output;
    histogram->WriteHTMLGraph(&ascii_output);
    histograms_list.push_back(ascii_output);
  }
  return histograms_list;
}

}  // namespace h5vcc
}  // namespace cobalt
