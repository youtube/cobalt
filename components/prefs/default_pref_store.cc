// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/default_pref_store.h"

#include <utility>

#include "base/logging.h"

using base::Value;

DefaultPrefStore::DefaultPrefStore() {}

bool DefaultPrefStore::GetValue(const std::string& key,
                                const Value** result) const {
  return prefs_.GetValue(key, result);
}

base::Value::Dict DefaultPrefStore::GetValues() const {
  return prefs_.AsDict();
}

void DefaultPrefStore::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void DefaultPrefStore::RemoveObserver(PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool DefaultPrefStore::HasObservers() const {
  return !observers_.empty();
}

void DefaultPrefStore::SetDefaultValue(const std::string& key, Value value) {
  DCHECK(!GetValue(key, nullptr));
  prefs_.SetValue(key, std::move(value));
}

void DefaultPrefStore::ReplaceDefaultValue(const std::string& key,
                                           Value value) {
  DCHECK(GetValue(key, nullptr));
  bool notify = prefs_.SetValue(key, std::move(value));
  if (notify) {
    for (Observer& observer : observers_)
      observer.OnPrefValueChanged(key);
  }
}

DefaultPrefStore::const_iterator DefaultPrefStore::begin() const {
  return prefs_.begin();
}

DefaultPrefStore::const_iterator DefaultPrefStore::end() const {
  return prefs_.end();
}

DefaultPrefStore::~DefaultPrefStore() {}
