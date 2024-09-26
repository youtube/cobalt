// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_REQUEST_FEATURES_H_
#define COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_REQUEST_FEATURES_H_

#include "components/permissions/permission_request_enums.h"
#include "components/permissions/request_type.h"

namespace permissions {

struct PredictionRequestFeatures {
  struct ActionCounts {
    size_t grants = 0;
    size_t denies = 0;
    size_t dismissals = 0;
    size_t ignores = 0;
    size_t total() const { return grants + denies + dismissals + ignores; }
  };

  // Whether a gesture is present or not.
  PermissionRequestGestureType gesture;

  // Which permissions request type this is for.
  RequestType type;

  // The permission action counts for this specific permission type.
  ActionCounts requested_permission_counts;

  // The permission action counts for all permissions type.
  ActionCounts all_permission_counts;
};

}  // namespace permissions

#endif  // COMPONENTS_PERMISSIONS_PREDICTION_SERVICE_PREDICTION_REQUEST_FEATURES_H_
