// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_WORKER_CONTEXT_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_WORKER_CONTEXT_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace performance_manager {
class WorkerNode;
}  // namespace performance_manager

namespace performance_manager::resource_attribution {

class WorkerContext {
 public:
  ~WorkerContext();

  WorkerContext(const WorkerContext& other);
  WorkerContext& operator=(const WorkerContext& other);
  WorkerContext(WorkerContext&& other);
  WorkerContext& operator=(WorkerContext&& other);

  // UI thread methods.

  // Returns the WorkerContext for `token`. Returns nullopt if the WorkerToken
  // is not registered with PerformanceManager.
  static absl::optional<WorkerContext> FromWorkerToken(
      const blink::WorkerToken& token);

  // Returns the WorkerToken for this context.
  blink::WorkerToken GetWorkerToken() const;

  // Returns the WorkerNode for this context, or a null WeakPtr if it no longer
  // exists.
  base::WeakPtr<WorkerNode> GetWeakWorkerNode() const;

  // PM sequence methods.

  // Returns the WorkerContext for `node`. Equivalent to
  // node->GetResourceContext().
  static WorkerContext FromWorkerNode(const WorkerNode* node);

  // Returns the WorkerContext for `node`, or nullopt if `node` is null.
  static absl::optional<WorkerContext> FromWeakWorkerNode(
      base::WeakPtr<WorkerNode> node);

  // Returns the WorkerNode for this context, or nullptr if it no longer exists.
  WorkerNode* GetWorkerNode() const;

  // Returns a string representation of the context for debugging. This matches
  // the interface of base::TokenType and base::UnguessableToken, for
  // convenience.
  std::string ToString() const;

 private:
  friend bool operator<(const WorkerContext&, const WorkerContext&);
  friend bool operator==(const WorkerContext&, const WorkerContext&);

  WorkerContext(const blink::WorkerToken& token,
                base::WeakPtr<WorkerNode> weak_node);

  blink::WorkerToken token_;
  base::WeakPtr<WorkerNode> weak_node_;
};

inline bool operator<(const WorkerContext& a, const WorkerContext& b) {
  return a.token_ < b.token_;
}

inline bool operator==(const WorkerContext& a, const WorkerContext& b) {
  return a.token_ == b.token_;
}

inline bool operator!=(const WorkerContext& a, const WorkerContext& b) {
  return !(a == b);
}

inline bool operator<=(const WorkerContext& a, const WorkerContext& b) {
  return !(b < a);
}

inline bool operator>(const WorkerContext& a, const WorkerContext& b) {
  return !(a < b || a == b);
}

inline bool operator>=(const WorkerContext& a, const WorkerContext& b) {
  return !(a > b);
}

}  // namespace performance_manager::resource_attribution

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PUBLIC_RESOURCE_ATTRIBUTION_WORKER_CONTEXT_H_
