// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/pref_value_map.h"

#include <limits.h>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/strings/string_piece.h"
#include "base/values.h"

PrefValueMap::PrefValueMap() {}

PrefValueMap::~PrefValueMap() {}

bool PrefValueMap::GetValue(base::StringPiece key,
                            const base::Value** value) const {
  auto it = prefs_.find(key);
  if (it == prefs_.end())
    return false;

  if (value)
    *value = &it->second;

  return true;
}

bool PrefValueMap::GetValue(base::StringPiece key, base::Value** value) {
  auto it = prefs_.find(key);
  if (it == prefs_.end())
    return false;

  if (value)
    *value = &it->second;

  return true;
}

bool PrefValueMap::SetValue(const std::string& key, base::Value value) {
  base::Value& existing_value = prefs_[key];
  if (value == existing_value)
    return false;

  existing_value = std::move(value);
  return true;
}

bool PrefValueMap::RemoveValue(const std::string& key) {
  return prefs_.erase(key) != 0;
}

void PrefValueMap::Clear() {
  prefs_.clear();
}

void PrefValueMap::ClearWithPrefix(const std::string& prefix) {
  Map::iterator low = prefs_.lower_bound(prefix);
  // Appending maximum possible character so that there will be no string with
  // prefix |prefix| that we may miss.
  Map::iterator high = prefs_.upper_bound(prefix + char(CHAR_MAX));
  prefs_.erase(low, high);
}

void PrefValueMap::Swap(PrefValueMap* other) {
  prefs_.swap(other->prefs_);
}

PrefValueMap::iterator PrefValueMap::begin() {
  return prefs_.begin();
}

PrefValueMap::iterator PrefValueMap::end() {
  return prefs_.end();
}

PrefValueMap::const_iterator PrefValueMap::begin() const {
  return prefs_.begin();
}

PrefValueMap::const_iterator PrefValueMap::end() const {
  return prefs_.end();
}

bool PrefValueMap::empty() const {
  return prefs_.empty();
}

bool PrefValueMap::GetBoolean(const std::string& key, bool* value) const {
  const base::Value* stored_value = nullptr;
  if (GetValue(key, &stored_value) && stored_value->is_bool()) {
    *value = stored_value->GetBool();
    return true;
  }
  return false;
}

void PrefValueMap::SetBoolean(const std::string& key, bool value) {
  SetValue(key, base::Value(value));
}

bool PrefValueMap::GetString(const std::string& key, std::string* value) const {
  const base::Value* stored_value = nullptr;
  if (GetValue(key, &stored_value) && stored_value->is_string()) {
    *value = stored_value->GetString();
    return true;
  }
  return false;
}

void PrefValueMap::SetString(const std::string& key, const std::string& value) {
  SetValue(key, base::Value(value));
}

bool PrefValueMap::GetInteger(const std::string& key, int* value) const {
  const base::Value* stored_value = nullptr;
  if (GetValue(key, &stored_value) && stored_value->is_int()) {
    *value = stored_value->GetInt();
    return true;
  }
  return false;
}

void PrefValueMap::SetInteger(const std::string& key, const int value) {
  SetValue(key, base::Value(value));
}

void PrefValueMap::SetDouble(const std::string& key, const double value) {
  SetValue(key, base::Value(value));
}

void PrefValueMap::GetDifferingKeys(
    const PrefValueMap* other,
    std::vector<std::string>* differing_keys) const {
  differing_keys->clear();

  // Put everything into ordered maps.
  std::map<std::string, const base::Value*> this_prefs;
  std::map<std::string, const base::Value*> other_prefs;
  for (const auto& pair : prefs_)
    this_prefs.emplace(pair.first, &pair.second);
  for (const auto& pair : other->prefs_)
    other_prefs.emplace(pair.first, &pair.second);

  // Walk over the maps in lockstep, adding everything that is different.
  auto this_pref = this_prefs.begin();
  auto other_pref = other_prefs.begin();
  while (this_pref != this_prefs.end() && other_pref != other_prefs.end()) {
    const int diff = this_pref->first.compare(other_pref->first);
    if (diff == 0) {
      if (*this_pref->second != *other_pref->second)
        differing_keys->push_back(this_pref->first);
      ++this_pref;
      ++other_pref;
    } else if (diff < 0) {
      differing_keys->push_back(this_pref->first);
      ++this_pref;
    } else if (diff > 0) {
      differing_keys->push_back(other_pref->first);
      ++other_pref;
    }
  }

  // Add the remaining entries.
  for (; this_pref != this_prefs.end(); ++this_pref)
    differing_keys->push_back(this_pref->first);
  for (; other_pref != other_prefs.end(); ++other_pref)
    differing_keys->push_back(other_pref->first);
}

base::Value::Dict PrefValueMap::AsDict() const {
  base::Value::Dict dictionary;
  for (const auto& value : prefs_)
    dictionary.SetByDottedPath(value.first, value.second.Clone());

  return dictionary;
}
