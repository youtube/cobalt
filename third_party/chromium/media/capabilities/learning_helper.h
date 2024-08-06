// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_CHROMIUM_MEDIA_CAPABILITIES_LEARNING_HELPER_H_
#define THIRD_PARTY_CHROMIUM_MEDIA_CAPABILITIES_LEARNING_HELPER_H_

#include <memory>

#include "base/macros.h"
#include "base/threading/sequence_bound.h"
#include "third_party/chromium/media/base/media_export.h"
#include "third_party/chromium/media/capabilities/video_decode_stats_db.h"
#include "third_party/chromium/media/learning/impl/feature_provider.h"
#include "third_party/chromium/media/learning/impl/learning_session_impl.h"

namespace media_m96 {

// Helper class to allow MediaCapabilities to log training examples to a
// media_m96::learning LearningTask.
class MEDIA_EXPORT LearningHelper {
 public:
  // |feature_factory| lets us register FeatureProviders with those
  // LearningTasks that include standard features.
  LearningHelper(learning::FeatureProviderFactoryCB feature_factory);
  ~LearningHelper();

  // |origin| is sent in separately since it's somewhat hacky.  This should be
  // provided by the FeatureProvider, but the way LearningHelper is used, it
  // doesn't have access to the origin.  Per-frame LearningTaskControllers will
  // be able to do this much more easily.
  void AppendStats(const VideoDecodeStatsDB::VideoDescKey& video_key,
                   learning::FeatureValue origin,
                   const VideoDecodeStatsDB::DecodeStatsEntry& new_stats);

 private:
  // Convenience function to begin and complete an observation.
  void AddExample(learning::LearningTaskController* controller,
                  const learning::LabelledExample& example);

  // Learning session for our profile.  Normally, we'd not have one of these
  // directly, but would instead get one that's connected to a browser profile.
  // For now, however, we just instantiate one and assume that we'll be
  // destroyed when the profile changes / history is cleared.
  std::unique_ptr<learning::LearningSessionImpl> learning_session_;

  // Controllers for each task.
  std::unique_ptr<learning::LearningTaskController>
      base_unweighted_table_controller_;
  std::unique_ptr<learning::LearningTaskController>
      base_unweighted_tree_controller_;
  std::unique_ptr<learning::LearningTaskController>
      base_unweighted_tree_200_controller_;
  std::unique_ptr<learning::LearningTaskController>
      enhanced_unweighted_tree_200_controller_;
};

}  // namespace media_m96

#endif  // THIRD_PARTY_CHROMIUM_MEDIA_CAPABILITIES_LEARNING_HELPER_H_
