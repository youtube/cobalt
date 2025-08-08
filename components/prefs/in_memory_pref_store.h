// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_IN_MEMORY_PREF_STORE_H_
#define COMPONENTS_PREFS_IN_MEMORY_PREF_STORE_H_

#include <stdint.h>
#include <string>

#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_value_map.h"

// A light-weight prefstore implementation that keeps preferences
// in a memory backed store. This is not a persistent prefstore -- we
// subclass the PersistentPrefStore here since it is needed by the
// PrefService, which in turn is needed by various components.
class COMPONENTS_PREFS_EXPORT InMemoryPrefStore : public PersistentPrefStore {
 public:
  InMemoryPrefStore();

  InMemoryPrefStore(const InMemoryPrefStore&) = delete;
  InMemoryPrefStore& operator=(const InMemoryPrefStore&) = delete;

  // PrefStore implementation.
  bool GetValue(base::StringPiece key,
                const base::Value** result) const override;
  base::Value::Dict GetValues() const override;
  void AddObserver(PrefStore::Observer* observer) override;
  void RemoveObserver(PrefStore::Observer* observer) override;
  bool HasObservers() const override;
  bool IsInitializationComplete() const override;

  // PersistentPrefStore implementation.
  bool GetMutableValue(const std::string& key, base::Value** result) override;
  void ReportValueChanged(const std::string& key, uint32_t flags) override;
  void SetValue(const std::string& key,
                base::Value value,
                uint32_t flags) override;
  void SetValueSilently(const std::string& key,
                        base::Value value,
                        uint32_t flags) override;
  void RemoveValue(const std::string& key, uint32_t flags) override;
  bool ReadOnly() const override;
  PrefReadError GetReadError() const override;
  PersistentPrefStore::PrefReadError ReadPrefs() override;
  void ReadPrefsAsync(ReadErrorDelegate* error_delegate) override {}
  void SchedulePendingLossyWrites() override {}
  void OnStoreDeletionFromDisk() override {}
  bool IsInMemoryPrefStore() const override;
  void RemoveValuesByPrefixSilently(const std::string& prefix) override;

 protected:
  ~InMemoryPrefStore() override;

 private:
  // Stores the preference values.
  PrefValueMap prefs_;

  base::ObserverList<PrefStore::Observer, true>::Unchecked observers_;
};

#endif  // COMPONENTS_PREFS_IN_MEMORY_PREF_STORE_H_
