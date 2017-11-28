// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PREFS_JSON_PREF_STORE_H_
#define BASE_PREFS_JSON_PREF_STORE_H_

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/files/important_file_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "base/observer_list.h"
#include "base/prefs/base_prefs_export.h"
#include "base/prefs/persistent_pref_store.h"

namespace base {
class DictionaryValue;
class SequencedWorkerPool;
class SequencedTaskRunner;
class Value;
}

class FilePath;

// A writable PrefStore implementation that is used for user preferences.
class BASE_PREFS_EXPORT JsonPrefStore
    : public PersistentPrefStore,
      public base::ImportantFileWriter::DataSerializer {
 public:
  // Returns instance of SequencedTaskRunner which guarantees that file
  // operations on the same file will be executed in sequenced order.
  static scoped_refptr<base::SequencedTaskRunner> GetTaskRunnerForFile(
      const FilePath& pref_filename,
      base::SequencedWorkerPool* worker_pool);

  // |sequenced_task_runner| is must be a shutdown-blocking task runner, ideally
  // created by GetTaskRunnerForFile() method above.
  JsonPrefStore(const FilePath& pref_filename,
                base::SequencedTaskRunner* sequenced_task_runner);

  // PrefStore overrides:
  virtual bool GetValue(const std::string& key,
                        const base::Value** result) const override;
  virtual void AddObserver(PrefStore::Observer* observer) override;
  virtual void RemoveObserver(PrefStore::Observer* observer) override;
  virtual size_t NumberOfObservers() const override;
  virtual bool IsInitializationComplete() const override;

  // PersistentPrefStore overrides:
  virtual bool GetMutableValue(const std::string& key,
                               base::Value** result) override;
  virtual void SetValue(const std::string& key, base::Value* value) override;
  virtual void SetValueSilently(const std::string& key,
                                base::Value* value) override;
  virtual void RemoveValue(const std::string& key) override;
  virtual void MarkNeedsEmptyValue(const std::string& key) override;
  virtual bool ReadOnly() const override;
  virtual PrefReadError GetReadError() const override;
  virtual PrefReadError ReadPrefs() override;
  virtual void ReadPrefsAsync(ReadErrorDelegate* error_delegate) override;
  virtual void CommitPendingWrite() override;
  virtual void ReportValueChanged(const std::string& key) override;

  // This method is called after JSON file has been read. Method takes
  // ownership of the |value| pointer. Note, this method is used with
  // asynchronous file reading, so class exposes it only for the internal needs.
  // (read: do not call it manually).
  void OnFileRead(base::Value* value_owned, PrefReadError error, bool no_dir);

 private:
  virtual ~JsonPrefStore();

  // ImportantFileWriter::DataSerializer overrides:
  virtual bool SerializeData(std::string* output) override;

  FilePath path_;
  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  scoped_ptr<base::DictionaryValue> prefs_;

  bool read_only_;

  // Helper for safely writing pref data.
  base::ImportantFileWriter writer_;

  ObserverList<PrefStore::Observer, true> observers_;

  scoped_ptr<ReadErrorDelegate> error_delegate_;

  bool initialized_;
  PrefReadError read_error_;

  std::set<std::string> keys_need_empty_value_;

  DISALLOW_COPY_AND_ASSIGN(JsonPrefStore);
};

#endif  // BASE_PREFS_JSON_PREF_STORE_H_
