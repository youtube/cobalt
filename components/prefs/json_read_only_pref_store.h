// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_JSON_READ_ONLY_PREF_STORE_H_
#define COMPONENTS_PREFS_JSON_READ_ONLY_PREF_STORE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/thread_pool.h"
#include "base/values.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_filter.h"
#include "components/prefs/prefs_export.h"

class PrefFilter;

namespace base {
class DictionaryValue;
class FilePath;
class JsonPrefStoreCallbackTest;
class SequencedTaskRunner;
class Value;
}  // namespace base

// A readonly JSONPrefStore implementation that is used for
// reading user preferences.
class COMPONENTS_PREFS_EXPORT JsonReadOnlyPrefStore
    : public PersistentPrefStore,
      public base::SupportsWeakPtr<JsonReadOnlyPrefStore> {
 public:
  struct ReadResult;

  JsonReadOnlyPrefStore(
      const base::FilePath& pref_filename,
      std::unique_ptr<PrefFilter> pref_filter = nullptr,
      scoped_refptr<base::SequencedTaskRunner> file_task_runner =
        base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
                base::TaskShutdownBehavior::BLOCK_SHUTDOWN}));

  // PrefStore overrides:
  bool GetValue(const std::string& key,
                const base::Value** result) const override;
  base::Value::Dict GetValues() const override;
  void AddObserver(PrefStore::Observer* observer) override;
  void RemoveObserver(PrefStore::Observer* observer) override;
  bool HasObservers() const override;
  bool IsInitializationComplete() const override;

  // PersistentPrefStore overrides:
  bool GetMutableValue(const std::string& key, base::Value** result) override;
  void SetValue(const std::string& key,
                base::Value value,
                uint32_t flags) override;
  void SetValueSilently(const std::string& key,
                        base::Value value,
                        uint32_t flags) override;
  void RemoveValue(const std::string& key, uint32_t flags) override;
  bool ReadOnly() const override;
  PrefReadError GetReadError() const override;
  // Note this method may be asynchronous if this instance has a |pref_filter_|
  // in which case it will return PREF_READ_ERROR_ASYNCHRONOUS_TASK_INCOMPLETE.
  // See details in pref_filter.h.
  PrefReadError ReadPrefs() override;
  void ReadPrefsAsync(ReadErrorDelegate* error_delegate) override;
  void CommitPendingWrite(
      base::OnceClosure reply_callback = base::OnceClosure(),
      base::OnceClosure synchronous_done_callback =
          base::OnceClosure()) override;
  void SchedulePendingLossyWrites() override;
  void ReportValueChanged(const std::string& key, uint32_t flags) override;

  void ClearMutableValues() override;

  void OnStoreDeletionFromDisk() override;

 private:
  friend class base::JsonPrefStoreCallbackTest;

  // This method is called after the JSON file has been read.  It then hands
  // |value| (or an empty dictionary in some read error cases) to the
  // |pref_filter| if one is set. It also gives a callback pointing at
  // FinalizeFileRead() to that |pref_filter_| which is then responsible for
  // invoking it when done. If there is no |pref_filter_|, FinalizeFileRead()
  // is invoked directly.
  void OnFileRead(std::unique_ptr<ReadResult> read_result);

  // This method is called after the JSON file has been read and the result has
  // potentially been intercepted and modified by |pref_filter_|.
  // |initialization_successful| is pre-determined by OnFileRead() and should
  // be used when reporting OnInitializationCompleted().
  // |schedule_write| indicates whether a write should be immediately scheduled
  // (typically because the |pref_filter_| has already altered the |prefs|) --
  // this will be ignored if this store is read-only.
  void FinalizeFileRead(bool initialization_successful,
                        base::Value::Dict prefs,
                        bool schedule_write);

  const base::FilePath path_;
  const scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  base::Value::Dict prefs_;

  std::unique_ptr<PrefFilter> pref_filter_;
  base::ObserverList<PrefStore::Observer, true>::Unchecked observers_;

  std::unique_ptr<ReadErrorDelegate> error_delegate_;

  bool initialized_;
  bool filtering_in_progress_;
  PrefReadError read_error_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(JsonReadOnlyPrefStore);
};

#endif  // COMPONENTS_PREFS_JSON_READ_ONLY_PREF_STORE_H_
