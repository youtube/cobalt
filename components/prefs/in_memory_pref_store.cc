// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/in_memory_pref_store.h"

#include <memory>
#include <string>
#include <utility>

#include "base/observer_list.h"
#include "base/strings/string_piece.h"
#include "base/values.h"

InMemoryPrefStore::InMemoryPrefStore() {}

InMemoryPrefStore::~InMemoryPrefStore() {}

bool InMemoryPrefStore::GetValue(base::StringPiece key,
                                 const base::Value** value) const {
  return prefs_.GetValue(key, value);
}

base::Value::Dict InMemoryPrefStore::GetValues() const {
  return prefs_.AsDict();
}

bool InMemoryPrefStore::GetMutableValue(const std::string& key,
                                        base::Value** value) {
  return prefs_.GetValue(key, value);
}

void InMemoryPrefStore::AddObserver(PrefStore::Observer* observer) {
  observers_.AddObserver(observer);
}

void InMemoryPrefStore::RemoveObserver(PrefStore::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool InMemoryPrefStore::HasObservers() const {
  return !observers_.empty();
}

bool InMemoryPrefStore::IsInitializationComplete() const {
  return true;
}

void InMemoryPrefStore::SetValue(const std::string& key,
                                 base::Value value,
                                 uint32_t flags) {
  if (prefs_.SetValue(key, std::move(value)))
    ReportValueChanged(key, flags);
}

void InMemoryPrefStore::SetValueSilently(const std::string& key,
                                         base::Value value,
                                         uint32_t flags) {
  prefs_.SetValue(key, std::move(value));
}

void InMemoryPrefStore::RemoveValue(const std::string& key, uint32_t flags) {
  if (prefs_.RemoveValue(key))
    ReportValueChanged(key, flags);
}

void InMemoryPrefStore::RemoveValuesByPrefixSilently(
    const std::string& prefix) {
  prefs_.ClearWithPrefix(prefix);
}

bool InMemoryPrefStore::ReadOnly() const {
  return false;
}

PersistentPrefStore::PrefReadError InMemoryPrefStore::GetReadError() const {
  return PersistentPrefStore::PREF_READ_ERROR_NONE;
}

PersistentPrefStore::PrefReadError InMemoryPrefStore::ReadPrefs() {
  return PersistentPrefStore::PREF_READ_ERROR_NONE;
}

void InMemoryPrefStore::ReportValueChanged(const std::string& key,
                                           uint32_t flags) {
  for (Observer& observer : observers_)
    observer.OnPrefValueChanged(key);
}

bool InMemoryPrefStore::IsInMemoryPrefStore() const {
  return true;
}
