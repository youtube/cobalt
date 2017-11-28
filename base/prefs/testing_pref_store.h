// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PREFS_TESTING_PREF_STORE_H_
#define BASE_PREFS_TESTING_PREF_STORE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "base/prefs/persistent_pref_store.h"
#include "base/prefs/pref_value_map.h"

// |TestingPrefStore| is a preference store implementation that allows tests to
// explicitly manipulate the contents of the store, triggering notifications
// where appropriate.
class TestingPrefStore : public PersistentPrefStore {
 public:
  TestingPrefStore();

  // Overriden from PrefStore.
  virtual bool GetValue(const std::string& key,
                        const base::Value** result) const override;
  virtual void AddObserver(PrefStore::Observer* observer) override;
  virtual void RemoveObserver(PrefStore::Observer* observer) override;
  virtual size_t NumberOfObservers() const override;
  virtual bool IsInitializationComplete() const override;

  // PersistentPrefStore overrides:
  virtual bool GetMutableValue(const std::string& key,
                               base::Value** result) override;
  virtual void ReportValueChanged(const std::string& key) override;
  virtual void SetValue(const std::string& key, base::Value* value) override;
  virtual void SetValueSilently(const std::string& key,
                                base::Value* value) override;
  virtual void RemoveValue(const std::string& key) override;
  virtual void MarkNeedsEmptyValue(const std::string& key) override;
  virtual bool ReadOnly() const override;
  virtual PrefReadError GetReadError() const override;
  virtual PersistentPrefStore::PrefReadError ReadPrefs() override;
  virtual void ReadPrefsAsync(ReadErrorDelegate* error_delegate) override;
  virtual void CommitPendingWrite() override {}

  // Marks the store as having completed initialization.
  void SetInitializationCompleted();

  // Used for tests to trigger notifications explicitly.
  void NotifyPrefValueChanged(const std::string& key);
  void NotifyInitializationCompleted();

  // Some convenience getters/setters.
  void SetString(const std::string& key, const std::string& value);
  void SetInteger(const std::string& key, int value);
  void SetBoolean(const std::string& key, bool value);

  bool GetString(const std::string& key, std::string* value) const;
  bool GetInteger(const std::string& key, int* value) const;
  bool GetBoolean(const std::string& key, bool* value) const;

  // Getter and Setter methods for setting and getting the state of the
  // |TestingPrefStore|.
  virtual void set_read_only(bool read_only);

 protected:
  virtual ~TestingPrefStore();

 private:
  // Stores the preference values.
  PrefValueMap prefs_;

  // Flag that indicates if the PrefStore is read-only
  bool read_only_;

  // Whether initialization has been completed.
  bool init_complete_;

  ObserverList<PrefStore::Observer, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(TestingPrefStore);
};

#endif  // BASE_PREFS_TESTING_PREF_STORE_H_
