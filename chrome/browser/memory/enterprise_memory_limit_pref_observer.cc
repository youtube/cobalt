// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/enterprise_memory_limit_pref_observer.h"

#include "base/functional/bind.h"
#include "base/memory/memory_pressure_monitor.h"
#include "build/build_config.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "chrome/common/pref_names.h"
#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_source.h"
#endif

namespace memory {

namespace {
const int kMinimalResidentSetLimitMb = 1024;
}  // namespace

EnterpriseMemoryLimitPrefObserver::EnterpriseMemoryLimitPrefObserver(
    PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service_);
  DCHECK(base::MemoryPressureMonitor::Get());
  evaluator_ = std::make_unique<EnterpriseMemoryLimitEvaluator>(
      static_cast<memory_pressure::MultiSourceMemoryPressureMonitor*>(
          base::MemoryPressureMonitor::Get())
          ->CreateVoter());

  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kTotalMemoryLimitMb,
      base::BindRepeating(&EnterpriseMemoryLimitPrefObserver::GetPref,
                          base::Unretained(this)));
  GetPref();
}

EnterpriseMemoryLimitPrefObserver::~EnterpriseMemoryLimitPrefObserver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (evaluator_->IsRunning()) {
#if !BUILDFLAG(IS_ANDROID)
    resource_coordinator::GetTabLifecycleUnitSource()
        ->SetMemoryLimitEnterprisePolicyFlag(false);
#endif
    evaluator_->Stop();
  }
}

bool EnterpriseMemoryLimitPrefObserver::PlatformIsSupported() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  return true;
#else
  return false;
#endif
}

// static
void EnterpriseMemoryLimitPrefObserver::RegisterPrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kTotalMemoryLimitMb,
                                kMinimalResidentSetLimitMb);
}

void EnterpriseMemoryLimitPrefObserver::GetPref() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const PrefService::Preference* pref =
      pref_service_->FindPreference(prefs::kTotalMemoryLimitMb);

  if (pref->IsManaged()) {
    if (!evaluator_->IsRunning())
      evaluator_->Start();
    evaluator_->SetResidentSetLimitMb(
        std::max(pref->GetValue()->GetInt(), kMinimalResidentSetLimitMb));
  } else if (evaluator_->IsRunning()) {
    evaluator_->Stop();
  }

#if !BUILDFLAG(IS_ANDROID)
  resource_coordinator::GetTabLifecycleUnitSource()
      ->SetMemoryLimitEnterprisePolicyFlag(pref->IsManaged());
#endif
}

}  // namespace memory
