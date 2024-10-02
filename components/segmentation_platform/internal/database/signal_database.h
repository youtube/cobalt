// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SIGNAL_DATABASE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SIGNAL_DATABASE_H_

#include <cstdint>
#include <utility>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/signal_key.h"
#include "components/segmentation_platform/public/proto/types.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {

// Responsible for storing histogram signals and user action events in a
// database. The signal samples are lazily bucketed into daily buckets for
// efficient storage and retrieval. A periodic job is responsible for running
// the compaction and deletion of old entries.
class SignalDatabase {
 public:
  using SuccessCallback = base::OnceCallback<void(bool)>;
  using Sample = std::pair<base::Time, int32_t>;
  using SamplesCallback = base::OnceCallback<void(std::vector<Sample>)>;

  virtual ~SignalDatabase() = default;

  // Called to initialize the database. Must be called before other methods.
  virtual void Initialize(SuccessCallback callback) = 0;

  // Called to write UMA events to the database. Sample timestamps are converted
  // to delta from UTC midnight for efficient storage.
  virtual void WriteSample(proto::SignalType signal_type,
                           uint64_t name_hash,
                           absl::optional<int32_t> value,
                           SuccessCallback callback) = 0;

  // Called to get signals collected between any two timestamps (including both
  // ends). The samples are returned in the |callback| as a list of pairs
  // containing signal timestamp and and an optional value.
  virtual void GetSamples(proto::SignalType signal_type,
                          uint64_t name_hash,
                          base::Time start_time,
                          base::Time end_time,
                          SamplesCallback callback) = 0;

  // Called to delete database entries having end time earlier than |end_time|.
  virtual void DeleteSamples(proto::SignalType signal_type,
                             uint64_t name_hash,
                             base::Time end_time,
                             SuccessCallback callback) = 0;

  // Called to compact the signals collected for the given day. Do not run this
  // for the current day as it might lead to read/write race condition. Meant to
  // be used for compacting the entries for the previous day from a background
  // job. Nevertheless, the database will work correctly without the need for
  // any compaction. |time| is used for finding the associated day.
  virtual void CompactSamplesForDay(proto::SignalType signal_type,
                                    uint64_t name_hash,
                                    base::Time time,
                                    SuccessCallback callback) = 0;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_SIGNAL_DATABASE_H_
