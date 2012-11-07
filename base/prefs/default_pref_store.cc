// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/default_pref_store.h"
#include "base/logging.h"

using base::Value;

DefaultPrefStore::DefaultPrefStore() {}

PrefStore::ReadResult DefaultPrefStore::GetValue(
    const std::string& key,
    const base::Value** result) const {
  return prefs_.GetValue(key, result) ? READ_OK : READ_NO_VALUE;
}

void DefaultPrefStore::SetDefaultValue(const std::string& key, Value* value) {
  CHECK(GetValue(key, NULL) == READ_NO_VALUE);
  prefs_.SetValue(key, value);
}

void DefaultPrefStore::RemoveDefaultValue(const std::string& key) {
  CHECK(GetValue(key, NULL) == READ_OK);
  prefs_.RemoveValue(key);
}

base::Value::Type DefaultPrefStore::GetType(const std::string& key) const {
  const Value* value;
  return GetValue(key, &value) == READ_OK ? value->GetType() : Value::TYPE_NULL;
}

DefaultPrefStore::const_iterator DefaultPrefStore::begin() const {
  return prefs_.begin();
}

DefaultPrefStore::const_iterator DefaultPrefStore::end() const {
  return prefs_.end();
}

DefaultPrefStore::~DefaultPrefStore() {}
