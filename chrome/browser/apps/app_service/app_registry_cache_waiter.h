// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_REGISTRY_CACHE_WAITER_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_REGISTRY_CACHE_WAITER_H_

#include <string>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "url/gurl.h"

class Profile;

namespace apps {

class AppTypeInitializationWaiter : public apps::AppRegistryCache::Observer {
 public:
  AppTypeInitializationWaiter(Profile* profile, apps::AppType app_type);
  ~AppTypeInitializationWaiter() override;

  void Await(const base::Location& location = base::Location::Current());

 private:
  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppTypeInitialized(apps::AppType app_type) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  const apps::AppType app_type_;
  base::RunLoop run_loop_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};
};

class AppReadinessWaiter : public apps::AppRegistryCache::Observer {
 public:
  AppReadinessWaiter(
      Profile* profile,
      const std::string& app_id,
      base::RepeatingCallback<bool(apps::Readiness)> readiness_predicate);
  AppReadinessWaiter(Profile* profile,
                     const std::string& app_id,
                     apps::Readiness readiness = apps::Readiness::kReady);
  ~AppReadinessWaiter() override;

  void Await(const base::Location& location = base::Location::Current());

 private:
  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  const std::string app_id_;
  const base::RepeatingCallback<bool(apps::Readiness)> readiness_predicate_;
  base::RunLoop run_loop_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};
};

// Waits for the web app's scope in the App Service app cache to match the
// expected |scope|.
class WebAppScopeWaiter : public apps::AppRegistryCache::Observer {
 public:
  WebAppScopeWaiter(Profile* profile, const std::string& app_id, GURL scope);
  ~WebAppScopeWaiter() override;

  // Waits for the web app's scope in the App Service app cache to match the
  // expected scope. Returns immediately if the app already has the expected
  // scope.
  void Await(const base::Location& location = base::Location::Current());

 private:
  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  bool ContainsExpectedIntentFilter(const apps::AppUpdate& update) const;

  const std::string app_id_;
  const GURL scope_;
  base::RunLoop run_loop_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};
};

class AppUpdateWaiter : public apps::AppRegistryCache::Observer {
 public:
  AppUpdateWaiter(
      Profile* profile,
      const std::string& app_id,
      base::RepeatingCallback<bool(const apps::AppUpdate&)> condition =
          base::BindRepeating([](const apps::AppUpdate& update) {
            return true;
          }));
  ~AppUpdateWaiter() override;

  void Wait();

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;

  // apps::AppRegistryCache::Observer:
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

 private:
  std::string app_id_;
  base::OnceClosure callback_;
  base::RepeatingCallback<bool(const apps::AppUpdate&)> condition_;
  bool condition_met_ = false;
  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observation_{this};
};

// Waits for the app's window mode in the App Service app cache to match the
// expected |window_mode|.
class AppWindowModeWaiter : public apps::AppRegistryCache::Observer {
 public:
  AppWindowModeWaiter(Profile* profile,
                      const std::string& app_id,
                      apps::WindowMode window_mode);
  ~AppWindowModeWaiter() override;

  // Returns immediately if the app already has the expected window mode.
  void Await(const base::Location& location = base::Location::Current());

 private:
  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  bool HasExpectedWindowMode(const apps::AppUpdate& update) const;

  const std::string app_id_;
  const apps::WindowMode window_mode_;
  base::RunLoop run_loop_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_REGISTRY_CACHE_WAITER_H_
