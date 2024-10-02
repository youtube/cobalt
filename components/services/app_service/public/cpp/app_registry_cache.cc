// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_registry_cache.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/observer_list.h"
#include "build/chromeos_buildflags.h"

namespace apps {

AppRegistryCache::Observer::Observer(AppRegistryCache* cache) {
  Observe(cache);
}

AppRegistryCache::Observer::Observer() = default;

AppRegistryCache::Observer::~Observer() {
  if (cache_) {
    cache_->RemoveObserver(this);
  }
}

void AppRegistryCache::Observer::Observe(AppRegistryCache* cache) {
  if (cache == cache_) {
    // Early exit to avoid infinite loops if we're in the middle of a callback.
    return;
  }
  if (cache_) {
    cache_->RemoveObserver(this);
  }
  cache_ = cache;
  if (cache_) {
    cache_->AddObserver(this);
  }
}

AppRegistryCache::AppRegistryCache() : account_id_(EmptyAccountId()) {}

AppRegistryCache::~AppRegistryCache() {
  for (auto& obs : observers_) {
    obs.OnAppRegistryCacheWillBeDestroyed(this);
  }
  DCHECK(observers_.empty());
}

void AppRegistryCache::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void AppRegistryCache::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AppRegistryCache::OnApps(std::vector<AppPtr> deltas,
                              apps::AppType app_type,
                              bool should_notify_initialized) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  if (should_notify_initialized) {
    DCHECK_NE(apps::AppType::kUnknown, app_type);
    if (!IsAppTypeInitialized(app_type)) {
      in_progress_initialized_app_types_.insert(app_type);
    }
  }

  if (!deltas_in_progress_.empty()) {
    std::move(deltas.begin(), deltas.end(),
              std::back_inserter(deltas_pending_));
    return;
  }

  DoOnApps(std::move(deltas));
  while (!deltas_pending_.empty()) {
    std::vector<AppPtr> pending;
    pending.swap(deltas_pending_);
    DoOnApps(std::move(pending));
  }

  OnAppTypeInitialized();
}

void AppRegistryCache::DoOnApps(std::vector<AppPtr> deltas) {
  // Merge any deltas elements that have the same app_id. If an observer's
  // OnAppUpdate calls back into this AppRegistryCache then we can therefore
  // present a single delta for any given app_id.
  bool is_pending = false;
  for (auto& delta : deltas) {
    if (is_pending) {
      deltas_pending_.push_back(std::move(delta));
      continue;
    }
    if (delta->readiness == Readiness::kRemoved) {
      // As soon as we see a removed update, we need to defer future merges to
      // avoid cases where a removed update is merged with a future non-removed
      // update.
      //
      // For example, for below apps deltas:
      // 1. {App1: ready}
      // 2. {App1: uninstalled, App1: removed}
      // 3. {App1: disabled by policy, App2: ready}
      //
      // The expected result for OnAppUpdate is:
      // {App1: ready}
      // {App1: uninstalled}
      // Then App1 is removed in AppRegistry, GetAppType returns kUnknown.
      // {App1: is added as disabled by policy}
      // {App2: ready}
      is_pending = true;
    }

    auto d_iter = deltas_in_progress_.find(delta->app_id);
    if (d_iter != deltas_in_progress_.end()) {
      if (delta->readiness == Readiness::kRemoved) {
        // Ensure that removed deltas are *not* merged, so that the last update
        // before the merge is sent separately.
        deltas_pending_.push_back(std::move(delta));
      } else {
        AppUpdate::Merge(d_iter->second, delta.get());
      }
    } else {
      deltas_in_progress_[delta->app_id] = delta.get();
    }
  }

  // The remaining for loops range over the deltas_in_progress_ map, not
  // the deltas vector, so that OnAppUpdate is called only once per unique
  // app_id.

  // Notify the observers for every de-duplicated delta.
  for (const auto& d_iter : deltas_in_progress_) {
    // Do not update subscribers for removed apps.
    if (d_iter.second->readiness == Readiness::kRemoved) {
      continue;
    }
    auto s_iter = states_.find(d_iter.first);
    App* state = (s_iter != states_.end()) ? s_iter->second.get() : nullptr;
    App* delta = d_iter.second;

    for (auto& obs : observers_) {
      obs.OnAppUpdate(AppUpdate(state, delta, account_id_));
    }
  }

  // Update the states for every de-duplicated delta.
  for (const auto& d_iter : deltas_in_progress_) {
    auto s_iter = states_.find(d_iter.first);
    apps::App* state =
        (s_iter != states_.end()) ? s_iter->second.get() : nullptr;
    apps::App* delta = d_iter.second;

    if (delta->readiness != Readiness::kRemoved) {
      if (state) {
        AppUpdate::Merge(state, delta);
      } else {
        states_.insert(std::make_pair(delta->app_id, delta->Clone()));
      }
    } else {
      DCHECK(!state || state->readiness != Readiness::kReady);
      states_.erase(d_iter.first);
    }
  }
  deltas_in_progress_.clear();
}

AppType AppRegistryCache::GetAppType(const std::string& app_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  auto d_iter = deltas_in_progress_.find(app_id);
  if (d_iter != deltas_in_progress_.end()) {
    return d_iter->second->app_type;
  }
  auto s_iter = states_.find(app_id);
  return (s_iter != states_.end()) ? s_iter->second->app_type
                                   : AppType::kUnknown;
}

std::vector<AppPtr> AppRegistryCache::GetAllApps() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(my_sequence_checker_);

  std::vector<AppPtr> apps;
  for (const auto& s_iter : states_) {
    const App* state = s_iter.second.get();
    apps.push_back(state->Clone());

    auto d_iter = deltas_in_progress_.find(s_iter.first);
    const App* delta =
        (d_iter != deltas_in_progress_.end()) ? d_iter->second : nullptr;
    AppUpdate::Merge(apps[apps.size() - 1].get(), delta);
  }
  for (const auto& d_iter : deltas_in_progress_) {
    if (!base::Contains(states_, d_iter.first)) {
      apps.push_back(d_iter.second->Clone());
    }
  }
  return apps;
}

void AppRegistryCache::SetAccountId(const AccountId& account_id) {
  account_id_ = account_id;
}

const std::set<AppType>& AppRegistryCache::InitializedAppTypes() const {
  return initialized_app_types_;
}

bool AppRegistryCache::IsAppTypeInitialized(apps::AppType app_type) const {
  return base::Contains(initialized_app_types_, app_type);
}

void AppRegistryCache::ReinitializeForTesting() {
  states_.clear();
  deltas_in_progress_.clear();
  deltas_pending_.clear();
  in_progress_initialized_app_types_.clear();

  // On most platforms, we can't clear initialized_app_types_ here as observers
  // expect each type to be initialized only once.
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  initialized_app_types_.clear();
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

void AppRegistryCache::OnAppTypeInitialized() {
  if (in_progress_initialized_app_types_.empty()) {
    return;
  }

  // In observer's OnAppTypeInitialized callback, `OnApp` might be called to
  // update the app, then this OnAppTypeInitialized might be called again. So we
  // need to check the initialized `app_type` first, and remove it from
  // `in_progress_initialized_app_types_` to prevent the dead loop.
  std::set<AppType> in_progress_initialized_app_types;
  for (auto app_type : in_progress_initialized_app_types_) {
    in_progress_initialized_app_types.insert(app_type);
  }

  for (auto app_type : in_progress_initialized_app_types) {
    in_progress_initialized_app_types_.erase(app_type);
    initialized_app_types_.insert(app_type);
    for (auto& obs : observers_) {
      obs.OnAppTypeInitialized(app_type);
    }
  }
}

}  // namespace apps
