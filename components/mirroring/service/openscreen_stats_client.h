// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MIRRORING_SERVICE_OPENSCREEN_STATS_CLIENT_H_
#define COMPONENTS_MIRRORING_SERVICE_OPENSCREEN_STATS_CLIENT_H_

#include "base/values.h"
#include "components/mirroring/service/session_logger.h"
#include "third_party/openscreen/src/cast/streaming/sender_session.h"
#include "third_party/openscreen/src/cast/streaming/statistics.h"

namespace mirroring {

// Handles statistics of an Openscreen mirroring session.
class COMPONENT_EXPORT(MIRRORING_SERVICE) OpenscreenStatsClient
    : public openscreen::cast::SenderStatsClient,
      public MirroringStatsProvider {
 public:
  OpenscreenStatsClient();
  ~OpenscreenStatsClient() override;

  // MirroringStatsProvider::GetStats() override;
  base::Value::Dict GetStats() const override;

  // openscreen::cast::SenderStatsClient::OnStatisticsUpdated() override;
  void OnStatisticsUpdated(
      const openscreen::cast::SenderStats& updated_stats) override;

 protected:
  base::Value::Dict ConvertSenderStatsToDict(
      const openscreen::cast::SenderStats& updated_stats) const;
  base::Value::Dict ConvertStatisticsListToDict(
      const openscreen::cast::SenderStats::StatisticsList& stats_list) const;
  base::Value::Dict ConvertHistogramsListToDict(
      const openscreen::cast::SenderStats::HistogramsList& histograms_list)
      const;
  base::Value::List ConvertOpenscreenHistogramToList(
      const openscreen::cast::SimpleHistogram& histogram) const;

  base::Value::Dict most_recent_stats_;
};

}  // namespace mirroring

#endif  // COMPONENTS_MIRRORING_SERVICE_OPENSCREEN_STATS_CLIENT_H_
