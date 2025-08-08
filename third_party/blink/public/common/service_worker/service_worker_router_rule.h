// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_SERVICE_WORKER_SERVICE_WORKER_ROUTER_RULE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_SERVICE_WORKER_SERVICE_WORKER_ROUTER_RULE_H_

#include <vector>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/safe_url_pattern.h"

namespace network::mojom {

enum class RequestMode : int32_t;
enum class RequestDestination : int32_t;

}  // namespace network::mojom

namespace blink {

class ServiceWorkerRouterCondition;

// TODO(crbug.com/1490445): set this value by discussing in spec proposal.
static constexpr int kServiceWorkerRouterConditionMaxRecursionDepth = 10;

struct ServiceWorkerRouterRequestCondition {
  // https://fetch.spec.whatwg.org/#concept-request-method
  // Technically, it can be an arbitrary string, but Chromium would set
  // k*Method in net/http/http_request_headers.h
  absl::optional<std::string> method;
  // RequestMode in services/network/public/mojom/fetch_api.mojom
  absl::optional<network::mojom::RequestMode> mode;
  // RequestDestination in services/network/public/mojom/fetch_api.mojom
  absl::optional<network::mojom::RequestDestination> destination;

  bool operator==(const ServiceWorkerRouterRequestCondition& other) const;
};

struct ServiceWorkerRouterRunningStatusCondition {
  enum class RunningStatusEnum {
    kRunning = 0,
    // This includes kStarting and kStopping in addition to kStopped.
    // These states are consolidated to kNotRunning because they need to
    // wait for ServiceWorker set up to run the fetch handler.
    kNotRunning = 1,
  };

  RunningStatusEnum status;

  bool operator==(
      const ServiceWorkerRouterRunningStatusCondition& other) const {
    return status == other.status;
  }
};

struct ServiceWorkerRouterOrCondition {
  std::vector<ServiceWorkerRouterCondition> conditions;

  bool operator==(const ServiceWorkerRouterOrCondition& other) const;
};

// TODO(crbug.com/1371756): implement other conditions in the proposal.
class BLINK_COMMON_EXPORT ServiceWorkerRouterCondition {
  // We use aggregated getter/setter in this class in order to emit errors on
  // the callers when other conditions are added in the future
 public:
  using MemberRef =
      std::tuple<absl::optional<SafeUrlPattern>&,
                 absl::optional<ServiceWorkerRouterRequestCondition>&,
                 absl::optional<ServiceWorkerRouterRunningStatusCondition>&,
                 absl::optional<ServiceWorkerRouterOrCondition>&>;
  using MemberConstRef = std::tuple<
      const absl::optional<SafeUrlPattern>&,
      const absl::optional<ServiceWorkerRouterRequestCondition>&,
      const absl::optional<ServiceWorkerRouterRunningStatusCondition>&,
      const absl::optional<ServiceWorkerRouterOrCondition>&>;

  ServiceWorkerRouterCondition() = default;
  ServiceWorkerRouterCondition(const ServiceWorkerRouterCondition&) = default;
  ServiceWorkerRouterCondition& operator=(const ServiceWorkerRouterCondition&) =
      default;
  ServiceWorkerRouterCondition(ServiceWorkerRouterCondition&&) = default;
  ServiceWorkerRouterCondition& operator=(ServiceWorkerRouterCondition&&) =
      default;

  ServiceWorkerRouterCondition(
      const absl::optional<SafeUrlPattern>& url_pattern,
      const absl::optional<ServiceWorkerRouterRequestCondition>& request,
      const absl::optional<ServiceWorkerRouterRunningStatusCondition>&
          running_status,
      const absl::optional<ServiceWorkerRouterOrCondition>& or_condition)
      : url_pattern_(url_pattern),
        request_(request),
        running_status_(running_status),
        or_condition_(or_condition) {}

  // Returns tuple: suggest using structured bindings on the caller side.
  MemberRef get() {
    return {url_pattern_, request_, running_status_, or_condition_};
  }
  MemberConstRef get() const {
    return {url_pattern_, request_, running_status_, or_condition_};
  }

  bool IsEmpty() const {
    return !(url_pattern_ || request_ || running_status_ || or_condition_);
  }

  bool IsOrConditionExclusive() const {
    return or_condition_.has_value() !=
           (url_pattern_ || request_ || running_status_);
  }

  bool IsValid() const { return !IsEmpty() && IsOrConditionExclusive(); }

  bool operator==(const ServiceWorkerRouterCondition& other) const;

  static ServiceWorkerRouterCondition WithUrlPattern(
      const SafeUrlPattern& url_pattern) {
    return {url_pattern, absl::nullopt, absl::nullopt, absl::nullopt};
  }
  static ServiceWorkerRouterCondition WithRequest(
      const ServiceWorkerRouterRequestCondition& request) {
    return {absl::nullopt, request, absl::nullopt, absl::nullopt};
  }
  static ServiceWorkerRouterCondition WithRunningStatus(
      const ServiceWorkerRouterRunningStatusCondition& running_status) {
    return {absl::nullopt, absl::nullopt, running_status, absl::nullopt};
  }
  static ServiceWorkerRouterCondition WithOrCondition(
      const ServiceWorkerRouterOrCondition& or_condition) {
    return {absl::nullopt, absl::nullopt, absl::nullopt, or_condition};
  }

 private:
  // URLPattern to be used for matching.
  absl::optional<SafeUrlPattern> url_pattern_;

  // Request to be used for matching.
  absl::optional<ServiceWorkerRouterRequestCondition> request_;

  // Running status to be used for matching.
  absl::optional<ServiceWorkerRouterRunningStatusCondition> running_status_;

  // `Or` condition to be used for matching
  // We need `_condition` suffix to avoid conflict with reserved keywords in C++
  absl::optional<ServiceWorkerRouterOrCondition> or_condition_;
};

// Network source structure.
// TODO(crbug.com/1371756): implement fields in the proposal.
struct BLINK_COMMON_EXPORT ServiceWorkerRouterNetworkSource {
  bool operator==(const ServiceWorkerRouterNetworkSource& other) const {
    return true;
  }
};

// Race network and fetch handler source.
struct BLINK_COMMON_EXPORT ServiceWorkerRouterRaceSource {
  bool operator==(const ServiceWorkerRouterRaceSource& other) const {
    return true;
  }
};

// Fetch handler source structure.
struct BLINK_COMMON_EXPORT ServiceWorkerRouterFetchEventSource {
  bool operator==(const ServiceWorkerRouterFetchEventSource& other) const {
    return true;
  }
};

// Cache source structure.
struct BLINK_COMMON_EXPORT ServiceWorkerRouterCacheSource {
  // A name of the Cache object.
  // If the field is not set, any of the Cache objects that the CacheStorage
  // tracks are used for matching as if CacheStorage.match().
  absl::optional<std::string> cache_name;

  bool operator==(const ServiceWorkerRouterCacheSource& other) const;
};

// This represents a source of the router rule.
// TODO(crbug.com/1371756): implement other sources in the proposal.
struct BLINK_COMMON_EXPORT ServiceWorkerRouterSource {
  // Type of sources.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class Type {
    // Network is used as a source.
    kNetwork = 0,
    // Race network and fetch handler.
    kRace = 1,
    // Fetch Event is used as a source.
    kFetchEvent = 2,
    // Cache is used as a source.
    kCache = 3,

    kMaxValue = kCache,
  };
  Type type;

  absl::optional<ServiceWorkerRouterNetworkSource> network_source;
  absl::optional<ServiceWorkerRouterRaceSource> race_source;
  absl::optional<ServiceWorkerRouterFetchEventSource> fetch_event_source;
  absl::optional<ServiceWorkerRouterCacheSource> cache_source;

  bool operator==(const ServiceWorkerRouterSource& other) const;
};

// This represents a ServiceWorker static routing API's router rule.
// It represents each route.
struct BLINK_COMMON_EXPORT ServiceWorkerRouterRule {
  // A rule can have one condition object. A condition object may have several
  // different conditions.
  ServiceWorkerRouterCondition condition;
  // There can be a list of sources, and expected to be routed from
  // front to back.
  std::vector<ServiceWorkerRouterSource> sources;

  bool operator==(const ServiceWorkerRouterRule& other) const {
    return condition == other.condition && sources == other.sources;
  }
};

// This represents a condition of the router rule.
// This represents a list of ServiceWorker static routing API's router rules.
struct BLINK_COMMON_EXPORT ServiceWorkerRouterRules {
  std::vector<ServiceWorkerRouterRule> rules;

  bool operator==(const ServiceWorkerRouterRules& other) const {
    return rules == other.rules;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_SERVICE_WORKER_SERVICE_WORKER_ROUTER_RULE_H_
