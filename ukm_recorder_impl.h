// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UKM_UKM_RECORDER_IMPL_H_
#define COMPONENTS_UKM_UKM_RECORDER_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "services/metrics/public/cpp/ukm_decode.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"

namespace metrics {
class UkmBrowserTestBase;
class UkmEGTestHelper;
}

namespace ukm {
class Report;
class UkmSource;
class UkmUtilsForTest;

namespace debug {
class UkmDebugDataExtractor;
}

class UkmRecorderImpl : public UkmRecorder {
  using IsWebstoreExtensionCallback =
      base::RepeatingCallback<bool(base::StringPiece id)>;

 public:
  UkmRecorderImpl();
  ~UkmRecorderImpl() override;

  // Unconditionally attempts to create a field trial to control client side
  // metrics/crash sampling to use as a fallback when one hasn't been
  // provided. This is expected to occur on first-run on platforms that don't
  // have first-run variations support. This should only be called when there is
  // no existing field trial controlling the sampling feature.
  static void CreateFallbackSamplingTrial(bool is_stable_channel,
                                          base::FeatureList* feature_list);

  // Enables/disables recording control if data is allowed to be collected. The
  // |extensions| flag separately controls recording of chrome-extension://
  // URLs; this flag should reflect the "sync extensions" user setting.
  void EnableRecording(bool extensions);
  void DisableRecording();

  // Disables sampling for testing purposes.
  void DisableSamplingForTesting() override;

  // Deletes stored recordings.
  void Purge();

  // Sets a callback for determining if an extension URL can be recorded.
  void SetIsWebstoreExtensionCallback(
      const IsWebstoreExtensionCallback& callback);

 protected:
  // Cache the list of whitelisted entries from the field trial parameter.
  void StoreWhitelistedEntries();

  // Writes recordings into a report proto, and clears recordings.
  void StoreRecordingsInReport(Report* report);

  const std::map<SourceId, std::unique_ptr<UkmSource>>& sources() const {
    return recordings_.sources;
  }

  const std::vector<mojom::UkmEntryPtr>& entries() const {
    return recordings_.entries;
  }

  // UkmRecorder:
  void UpdateSourceURL(SourceId source_id, const GURL& url) override;
  void UpdateAppURL(SourceId source_id, const GURL& url) override;
  void RecordNavigation(
      SourceId source_id,
      const UkmSource::NavigationData& navigation_data) override;
  using UkmRecorder::RecordOtherURL;

  virtual bool ShouldRestrictToWhitelistedSourceIds() const;

  virtual bool ShouldRestrictToWhitelistedEntries() const;

 private:
  friend ::metrics::UkmBrowserTestBase;
  friend ::metrics::UkmEGTestHelper;
  friend ::ukm::debug::UkmDebugDataExtractor;
  friend ::ukm::UkmUtilsForTest;

  struct MetricAggregate {
    uint64_t total_count = 0;
    double value_sum = 0;
    double value_square_sum = 0.0;
    uint64_t dropped_due_to_limits = 0;
    uint64_t dropped_due_to_sampling = 0;
    uint64_t dropped_due_to_whitelist = 0;
  };

  struct EventAggregate {
    EventAggregate();
    ~EventAggregate();

    base::flat_map<uint64_t, MetricAggregate> metrics;
    uint64_t total_count = 0;
    uint64_t dropped_due_to_limits = 0;
    uint64_t dropped_due_to_sampling = 0;
    uint64_t dropped_due_to_whitelist = 0;
  };

  using MetricAggregateMap = std::map<uint64_t, MetricAggregate>;

  // Returns true if |sanitized_url| should be recorded.
  bool ShouldRecordUrl(SourceId source_id, const GURL& sanitized_url) const;

  void RecordSource(std::unique_ptr<UkmSource> source);

  void AddEntry(mojom::UkmEntryPtr entry) override;

  // Load sampling configurations from field-trial information.
  void LoadExperimentSamplingInfo();

  // Whether recording new data is currently allowed.
  bool recording_enabled_ = false;

  // Indicates whether recording is enabled for extensions.
  bool extensions_enabled_ = false;

  // Indicates if sampling has been enabled.
  bool sampling_enabled_ = true;

  // Callback for checking extension IDs.
  IsWebstoreExtensionCallback is_webstore_extension_callback_;

  // Map from hashes to entry and metric names.
  ukm::builders::DecodeMap decode_map_;

  // Whitelisted Entry hashes, only the ones in this set will be recorded.
  std::set<uint64_t> whitelisted_entry_hashes_;

  // Sampling configurations, loaded from a field-trial.
  int default_sampling_rate_ = 0;
  base::flat_map<uint64_t, int> event_sampling_rates_;

  // Contains data from various recordings which periodically get serialized
  // and cleared by StoreRecordingsInReport() and may be Purged().
  struct Recordings {
    Recordings();
    Recordings& operator=(Recordings&&);
    ~Recordings();

    // Data captured by UpdateSourceUrl().
    std::map<SourceId, std::unique_ptr<UkmSource>> sources;

    // Data captured by AddEntry().
    std::vector<mojom::UkmEntryPtr> entries;

    // URLs of sources that matched a whitelist url, but were not included in
    // the report generated by the last log rotation because we haven't seen any
    // events for that source yet.
    std::unordered_set<std::string> carryover_urls_whitelist;

    // Aggregate information for collected event metrics.
    std::map<uint64_t, EventAggregate> event_aggregations;

    // Aggregated counters about Sources recorded in the current log.
    struct SourceCounts {
      // Count of URLs recorded for all sources.
      size_t observed = 0;
      // Count of URLs recorded for all SourceIdType::NAVIGATION_ID Sources.
      size_t navigation_sources = 0;
      // Sources carried over (not recorded) from a previous logging rotation.
      size_t carryover_sources = 0;

      // Resets all of the data.
      void Reset();
    };
    SourceCounts source_counts;

    // Resets all of the data.
    void Reset();
  };
  Recordings recordings_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace ukm

#endif  // COMPONENTS_UKM_UKM_RECORDER_IMPL_H_
