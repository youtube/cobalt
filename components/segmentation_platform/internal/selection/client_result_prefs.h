// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_PREDICTION_RESULT_PREFS_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_PREDICTION_RESULT_PREFS_H_

#include "base/base64.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/proto/client_results.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace segmentation_platform {
// Caching layer for client results. Uses prefs for persistent caching and fast
// lookup even before rest of the segmentation platform has been initialized.
// The cache consists of a single pref which consists of results for all the
// clients in a map. Each map entry consists of (1) Client key : The unique key
// for the client. (2) ClientResult : Consists of (a) The result of model
// evaluation. (b) The time when the  prediction result was written to the
// prefs. Used to enforce TTL determining if the result is expired.
class ClientResultPrefs {
 public:
  explicit ClientResultPrefs(PrefService* pref_service);
  virtual ~ClientResultPrefs() = default;

  // Disallow copy/assign.
  ClientResultPrefs(const ClientResultPrefs& other) = delete;
  ClientResultPrefs operator=(const ClientResultPrefs& other) = delete;

  // Writes the `ClientResult` containing prediction result from model
  // evaluation to prefs. If the result for client is already present, update it
  // with the new result.
  virtual void SaveClientResultToPrefs(
      const std::string& client_key,
      const absl::optional<proto::ClientResult>& client_result);

  // Reads the `ClientResult` from prefs, if present.
  virtual absl::optional<proto::ClientResult> ReadClientResultFromPrefs(
      const std::string& client_key);

 private:
  raw_ptr<PrefService> prefs_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_SELECTION_PREDICTION_RESULT_PREFS_H_
