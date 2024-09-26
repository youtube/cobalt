// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CLUSTERS_CORE_LABEL_CLUSTER_FINALIZER_H_
#define COMPONENTS_HISTORY_CLUSTERS_CORE_LABEL_CLUSTER_FINALIZER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ref.h"
#include "components/history_clusters/core/cluster_finalizer.h"

namespace optimization_guide {
struct EntityMetadata;
}  // namespace optimization_guide

namespace history_clusters {

// A cluster finalizer that determines the label for a given cluster.
class LabelClusterFinalizer : public ClusterFinalizer {
 public:
  explicit LabelClusterFinalizer(
      base::flat_map<std::string, optimization_guide::EntityMetadata>*
          entity_metadata_map);
  ~LabelClusterFinalizer() override;

  // ClusterFinalizer:
  void FinalizeCluster(history::Cluster& cluster) override;

 private:
  // A map from entity id to the metadata associated with it.
  //
  // Not owned. Guaranteed to outlive `this` and be non-null.
  const raw_ref<base::flat_map<std::string, optimization_guide::EntityMetadata>>
      entity_metadata_map_;
};

}  // namespace history_clusters

#endif  // COMPONENTS_HISTORY_CLUSTERS_CORE_LABEL_CLUSTER_FINALIZER_H_
