// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/public/pref_change_registrar.h"

#include "base/logging.h"
#include "base/prefs/public/pref_service_base.h"

PrefChangeRegistrar::PrefChangeRegistrar() : service_(NULL) {}

PrefChangeRegistrar::~PrefChangeRegistrar() {
  // If you see an invalid memory access in this destructor, this
  // PrefChangeRegistrar might be subscribed to an OffTheRecordProfileImpl that
  // has been destroyed. This should not happen any more but be warned.
  // Feel free to contact battre@chromium.org in case this happens.
  RemoveAll();
}

void PrefChangeRegistrar::Init(PrefServiceBase* service) {
  DCHECK(IsEmpty() || service_ == service);
  service_ = service;
}

void PrefChangeRegistrar::Add(const char* path, PrefObserver* obs) {
  if (!service_) {
    NOTREACHED();
    return;
  }
  ObserverRegistration registration(path, obs);
  if (observers_.find(registration) != observers_.end()) {
    NOTREACHED();
    return;
  }
  observers_.insert(registration);
  service_->AddPrefObserver(path, obs);
}

void PrefChangeRegistrar::Remove(const char* path, PrefObserver* obs) {
  if (!service_) {
    NOTREACHED();
    return;
  }
  ObserverRegistration registration(path, obs);
  std::set<ObserverRegistration>::iterator it =
       observers_.find(registration);
  if (it == observers_.end()) {
    NOTREACHED();
    return;
  }
  service_->RemovePrefObserver(it->first.c_str(), it->second);
  observers_.erase(it);
}

void PrefChangeRegistrar::RemoveAll() {
  if (service_) {
    for (std::set<ObserverRegistration>::const_iterator it = observers_.begin();
         it != observers_.end(); ++it) {
      service_->RemovePrefObserver(it->first.c_str(), it->second);
    }
    observers_.clear();
  }
}

bool PrefChangeRegistrar::IsEmpty() const {
  return observers_.empty();
}

bool PrefChangeRegistrar::IsObserved(const std::string& pref) {
  for (std::set<ObserverRegistration>::const_iterator it = observers_.begin();
       it != observers_.end(); ++it) {
    if (it->first == pref)
      return true;
  }
  return false;
}

bool PrefChangeRegistrar::IsManaged() {
  for (std::set<ObserverRegistration>::const_iterator it = observers_.begin();
       it != observers_.end(); ++it) {
    const PrefServiceBase::Preference* pref =
        service_->FindPreference(it->first.c_str());
    if (pref && pref->IsManaged())
      return true;
  }
  return false;
}
