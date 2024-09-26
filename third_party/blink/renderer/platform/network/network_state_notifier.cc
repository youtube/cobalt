/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/network/network_state_notifier.h"

#include <memory>

#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "services/network/public/cpp/client_hints.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom-blink.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

using mojom::blink::EffectiveConnectionType;

namespace {

// Typical HTTP RTT value corresponding to a given WebEffectiveConnectionType
// value. Taken from
// https://cs.chromium.org/chromium/src/net/nqe/network_quality_estimator_params.cc.
const base::TimeDelta kTypicalHttpRttEffectiveConnectionType
    [static_cast<size_t>(WebEffectiveConnectionType::kMaxValue) + 1] = {
        base::Milliseconds(0),    base::Milliseconds(0),
        base::Milliseconds(3600), base::Milliseconds(1800),
        base::Milliseconds(450),  base::Milliseconds(175)};

// Typical downlink throughput (in Mbps) value corresponding to a given
// WebEffectiveConnectionType value. Taken from
// https://cs.chromium.org/chromium/src/net/nqe/network_quality_estimator_params.cc.
const double kTypicalDownlinkMbpsEffectiveConnectionType
    [static_cast<size_t>(WebEffectiveConnectionType::kMaxValue) + 1] = {
        0, 0, 0.040, 0.075, 0.400, 1.600};

}  // namespace

NetworkStateNotifier& GetNetworkStateNotifier() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(NetworkStateNotifier, network_state_notifier,
                                  ());
  return network_state_notifier;
}

NetworkStateNotifier::ScopedNotifier::ScopedNotifier(
    NetworkStateNotifier& notifier)
    : notifier_(notifier) {
  DCHECK(IsMainThread());
  before_ = notifier_.has_override_ ? notifier_.override_ : notifier_.state_;
}

NetworkStateNotifier::ScopedNotifier::~ScopedNotifier() {
  DCHECK(IsMainThread());
  const NetworkState& after =
      notifier_.has_override_ ? notifier_.override_ : notifier_.state_;
  if ((after.type != before_.type ||
       after.max_bandwidth_mbps != before_.max_bandwidth_mbps ||
       after.effective_type != before_.effective_type ||
       after.http_rtt != before_.http_rtt ||
       after.transport_rtt != before_.transport_rtt ||
       after.downlink_throughput_mbps != before_.downlink_throughput_mbps ||
       after.save_data != before_.save_data) &&
      before_.connection_initialized) {
    notifier_.NotifyObservers(notifier_.connection_observers_,
                              ObserverType::kConnectionType, after);
  }
  if (after.on_line != before_.on_line && before_.on_line_initialized) {
    notifier_.NotifyObservers(notifier_.on_line_state_observers_,
                              ObserverType::kOnLineState, after);
  }
}

NetworkStateNotifier::NetworkStateObserverHandle::NetworkStateObserverHandle(
    NetworkStateNotifier* notifier,
    NetworkStateNotifier::ObserverType type,
    NetworkStateNotifier::NetworkStateObserver* observer,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : notifier_(notifier),
      type_(type),
      observer_(observer),
      task_runner_(std::move(task_runner)) {}

NetworkStateNotifier::NetworkStateObserverHandle::
    ~NetworkStateObserverHandle() {
  notifier_->RemoveObserver(type_, observer_, std::move(task_runner_));
}

void NetworkStateNotifier::SetOnLine(bool on_line) {
  DCHECK(IsMainThread());
  ScopedNotifier notifier(*this);
  {
    base::AutoLock locker(lock_);
    state_.on_line_initialized = true;
    state_.on_line = on_line;
  }
}

void NetworkStateNotifier::SetWebConnection(WebConnectionType type,
                                            double max_bandwidth_mbps) {
  DCHECK(IsMainThread());
  ScopedNotifier notifier(*this);
  {
    base::AutoLock locker(lock_);
    state_.connection_initialized = true;
    state_.type = type;
    state_.max_bandwidth_mbps = max_bandwidth_mbps;
  }
}

void NetworkStateNotifier::SetNetworkQuality(WebEffectiveConnectionType type,
                                             base::TimeDelta http_rtt,
                                             base::TimeDelta transport_rtt,
                                             int downlink_throughput_kbps) {
  DCHECK(IsMainThread());
  ScopedNotifier notifier(*this);
  {
    base::AutoLock locker(lock_);

    state_.effective_type = type;
    state_.http_rtt = absl::nullopt;
    state_.transport_rtt = absl::nullopt;
    state_.downlink_throughput_mbps = absl::nullopt;

    if (http_rtt.InMilliseconds() >= 0)
      state_.http_rtt = http_rtt;

    if (transport_rtt.InMilliseconds() >= 0)
      state_.transport_rtt = transport_rtt;

    if (downlink_throughput_kbps >= 0) {
      state_.downlink_throughput_mbps =
          static_cast<double>(downlink_throughput_kbps) / 1000;
    }
  }
}

void NetworkStateNotifier::SetNetworkQualityWebHoldback(
    WebEffectiveConnectionType type) {
  DCHECK(IsMainThread());
  if (type == WebEffectiveConnectionType::kTypeUnknown)
    return;
  ScopedNotifier notifier(*this);
  {
    base::AutoLock locker(lock_);

    state_.network_quality_web_holdback = type;
  }
}

std::unique_ptr<NetworkStateNotifier::NetworkStateObserverHandle>
NetworkStateNotifier::AddConnectionObserver(
    NetworkStateObserver* observer,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  AddObserverToMap(connection_observers_, observer, task_runner);
  return std::make_unique<NetworkStateNotifier::NetworkStateObserverHandle>(
      this, ObserverType::kConnectionType, observer, task_runner);
}

void NetworkStateNotifier::SetSaveDataEnabled(bool enabled) {
  DCHECK(IsMainThread());
  ScopedNotifier notifier(*this);
  {
    base::AutoLock locker(lock_);
    state_.save_data = enabled;
  }
}

std::unique_ptr<NetworkStateNotifier::NetworkStateObserverHandle>
NetworkStateNotifier::AddOnLineObserver(
    NetworkStateObserver* observer,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  AddObserverToMap(on_line_state_observers_, observer, task_runner);
  return std::make_unique<NetworkStateNotifier::NetworkStateObserverHandle>(
      this, ObserverType::kOnLineState, observer, task_runner);
}

void NetworkStateNotifier::SetNetworkConnectionInfoOverride(
    bool on_line,
    WebConnectionType type,
    absl::optional<WebEffectiveConnectionType> effective_type,
    int64_t http_rtt_msec,
    double max_bandwidth_mbps) {
  DCHECK(IsMainThread());
  ScopedNotifier notifier(*this);
  {
    base::AutoLock locker(lock_);
    has_override_ = true;
    override_.on_line_initialized = true;
    override_.on_line = on_line;
    override_.connection_initialized = true;
    override_.type = type;
    override_.max_bandwidth_mbps = max_bandwidth_mbps;

    if (!effective_type && http_rtt_msec > 0) {
      base::TimeDelta http_rtt(base::Milliseconds(http_rtt_msec));
      // Threshold values taken from
      // net/nqe/network_quality_estimator_params.cc.
      if (http_rtt >=
          net::kHttpRttEffectiveConnectionTypeThresholds[static_cast<int>(
              EffectiveConnectionType::kEffectiveConnectionSlow2GType)]) {
        effective_type = WebEffectiveConnectionType::kTypeSlow2G;
      } else if (http_rtt >=
                 net::kHttpRttEffectiveConnectionTypeThresholds[static_cast<
                     int>(
                     EffectiveConnectionType::kEffectiveConnection2GType)]) {
        effective_type = WebEffectiveConnectionType::kType2G;
      } else if (http_rtt >=
                 net::kHttpRttEffectiveConnectionTypeThresholds[static_cast<
                     int>(
                     EffectiveConnectionType::kEffectiveConnection3GType)]) {
        effective_type = WebEffectiveConnectionType::kType3G;
      } else {
        effective_type = WebEffectiveConnectionType::kType4G;
      }
    }
    override_.effective_type = effective_type
                                   ? effective_type.value()
                                   : WebEffectiveConnectionType::kTypeUnknown;
    override_.http_rtt = base::Milliseconds(http_rtt_msec);
    override_.downlink_throughput_mbps = max_bandwidth_mbps;
  }
}

void NetworkStateNotifier::SetSaveDataEnabledOverride(bool enabled) {
  DCHECK(IsMainThread());
  ScopedNotifier notifier(*this);
  {
    base::AutoLock locker(lock_);
    has_override_ = true;
    override_.on_line_initialized = true;
    override_.connection_initialized = true;
    override_.save_data = enabled;
  }
}

void NetworkStateNotifier::ClearOverride() {
  DCHECK(IsMainThread());
  ScopedNotifier notifier(*this);
  {
    base::AutoLock locker(lock_);
    has_override_ = false;
  }
}

void NetworkStateNotifier::NotifyObservers(ObserverListMap& map,
                                           ObserverType type,
                                           const NetworkState& state) {
  DCHECK(IsMainThread());
  base::AutoLock locker(lock_);
  for (const auto& entry : map) {
    entry.value->PostTask(
        FROM_HERE,
        base::BindOnce(&NetworkStateNotifier::NotifyObserverOnTaskRunner,
                       base::Unretained(this), base::UnsafeDangling(entry.key),
                       type, state));
  }
}

void NetworkStateNotifier::NotifyObserverOnTaskRunner(
    MayBeDangling<NetworkStateObserver> observer,
    ObserverType type,
    const NetworkState& state) {
  {
    base::AutoLock locker(lock_);
    ObserverListMap& map = GetObserverMapFor(type);
    // It's safe to pass a MayBeDangling pointer to find().
    ObserverListMap::iterator it = map.find(observer);
    if (map.end() == it) {
      return;
    }
    DCHECK(it->value->RunsTasksInCurrentSequence());
  }

  switch (type) {
    case ObserverType::kOnLineState:
      observer->OnLineStateChange(state.on_line);
      return;
    case ObserverType::kConnectionType:
      observer->ConnectionChange(
          state.type, state.max_bandwidth_mbps, state.effective_type,
          state.http_rtt, state.transport_rtt, state.downlink_throughput_mbps,
          state.save_data);
      return;
    default:
      NOTREACHED();
  }
}

NetworkStateNotifier::ObserverListMap& NetworkStateNotifier::GetObserverMapFor(
    ObserverType type) {
  switch (type) {
    case ObserverType::kConnectionType:
      return connection_observers_;
    case ObserverType::kOnLineState:
      return on_line_state_observers_;
    default:
      NOTREACHED();
      return connection_observers_;
  }
}

void NetworkStateNotifier::AddObserverToMap(
    ObserverListMap& map,
    NetworkStateObserver* observer,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(task_runner->RunsTasksInCurrentSequence());
  DCHECK(observer);

  base::AutoLock locker(lock_);
  ObserverListMap::AddResult result =
      map.insert(observer, std::move(task_runner));
  DCHECK(result.is_new_entry);
}

void NetworkStateNotifier::RemoveObserver(
    ObserverType type,
    NetworkStateObserver* observer,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(task_runner->RunsTasksInCurrentSequence());
  DCHECK(observer);

  ObserverListMap& map = GetObserverMapFor(type);
  DCHECK_NE(map.end(), map.find(observer));
  map.erase(observer);
}

// static
String NetworkStateNotifier::EffectiveConnectionTypeToString(
    WebEffectiveConnectionType type) {
  DCHECK_GT(network::kWebEffectiveConnectionTypeMappingCount,
            static_cast<size_t>(type));
  return network::kWebEffectiveConnectionTypeMapping[static_cast<int>(type)];
}

double NetworkStateNotifier::GetRandomMultiplier(const String& host) const {
  // The random number should be a function of the hostname to reduce
  // cross-origin fingerprinting. The random number should also be a function
  // of randomized salt which is known only to the device. This prevents
  // origin from removing noise from the estimates.
  if (!host)
    return 1.0;

  unsigned hash = WTF::GetHash(host) + RandomizationSalt();
  double random_multiplier = 0.9 + static_cast<double>((hash % 21)) * 0.01;
  DCHECK_LE(0.90, random_multiplier);
  DCHECK_GE(1.10, random_multiplier);
  return random_multiplier;
}

uint32_t NetworkStateNotifier::RoundRtt(
    const String& host,
    const absl::optional<base::TimeDelta>& rtt) const {
  if (!rtt.has_value()) {
    // RTT is unavailable. So, return the fastest value.
    return 0;
  }

  // Limit the maximum reported value and the granularity to reduce
  // fingerprinting.
  constexpr auto kMaxRtt = base::Seconds(3);
  constexpr auto kGranularity = base::Milliseconds(50);

  const base::TimeDelta modified_rtt =
      std::min(rtt.value() * GetRandomMultiplier(host), kMaxRtt);
  DCHECK_GE(modified_rtt, base::TimeDelta());
  return static_cast<uint32_t>(
      modified_rtt.RoundToMultiple(kGranularity).InMilliseconds());
}

double NetworkStateNotifier::RoundMbps(
    const String& host,
    const absl::optional<double>& downlink_mbps) const {
  // Limit the size of the buckets and the maximum reported value to reduce
  // fingerprinting.
  static const size_t kBucketSize = 50;
  static const double kMaxDownlinkKbps = 10.0 * 1000;

  double downlink_kbps = 0;
  if (!downlink_mbps.has_value()) {
    // Throughput is unavailable. So, return the fastest value.
    downlink_kbps = kMaxDownlinkKbps;
  } else {
    downlink_kbps = downlink_mbps.value() * 1000;
  }
  downlink_kbps *= GetRandomMultiplier(host);

  downlink_kbps = std::min(downlink_kbps, kMaxDownlinkKbps);

  DCHECK_LE(0, downlink_kbps);
  DCHECK_GE(kMaxDownlinkKbps, downlink_kbps);
  // Round down to the nearest kBucketSize kbps value.
  double downlink_kbps_rounded =
      std::round(downlink_kbps / kBucketSize) * kBucketSize;

  // Convert from Kbps to Mbps.
  return downlink_kbps_rounded / 1000;
}

absl::optional<WebEffectiveConnectionType>
NetworkStateNotifier::GetWebHoldbackEffectiveType() const {
  base::AutoLock locker(lock_);

  const NetworkState& state = has_override_ ? override_ : state_;
  // TODO (tbansal): Add a DCHECK to check that |state.on_line_initialized| is
  // true once https://crbug.com/728771 is fixed.
  return state.network_quality_web_holdback;
}

absl::optional<base::TimeDelta> NetworkStateNotifier::GetWebHoldbackHttpRtt()
    const {
  absl::optional<WebEffectiveConnectionType> override_ect =
      GetWebHoldbackEffectiveType();

  if (override_ect) {
    return kTypicalHttpRttEffectiveConnectionType[static_cast<size_t>(
        override_ect.value())];
  }
  return absl::nullopt;
}

absl::optional<double>
NetworkStateNotifier::GetWebHoldbackDownlinkThroughputMbps() const {
  absl::optional<WebEffectiveConnectionType> override_ect =
      GetWebHoldbackEffectiveType();

  if (override_ect) {
    return kTypicalDownlinkMbpsEffectiveConnectionType[static_cast<size_t>(
        override_ect.value())];
  }
  return absl::nullopt;
}

void NetworkStateNotifier::GetMetricsWithWebHoldback(
    WebConnectionType* type,
    double* downlink_max_mbps,
    WebEffectiveConnectionType* effective_type,
    absl::optional<base::TimeDelta>* http_rtt,
    absl::optional<double>* downlink_mbps,
    bool* save_data) const {
  base::AutoLock locker(lock_);
  const NetworkState& state = has_override_ ? override_ : state_;

  *type = state.type;
  *downlink_max_mbps = state.max_bandwidth_mbps;

  absl::optional<WebEffectiveConnectionType> override_ect =
      state.network_quality_web_holdback;
  if (override_ect) {
    *effective_type = override_ect.value();
    *http_rtt = kTypicalHttpRttEffectiveConnectionType[static_cast<size_t>(
        override_ect.value())];
    *downlink_mbps =
        kTypicalDownlinkMbpsEffectiveConnectionType[static_cast<size_t>(
            override_ect.value())];
  } else {
    *effective_type = state.effective_type;
    *http_rtt = state.http_rtt;
    *downlink_mbps = state.downlink_throughput_mbps;
  }
  *save_data = state.save_data;
}

}  // namespace blink
