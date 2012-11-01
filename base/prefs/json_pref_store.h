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
class MessageLoopProxy;
class Value;
}

class FilePath;

// A writable PrefStore implementation that is used for user preferences.
class BASE_PREFS_EXPORT JsonPrefStore
    : public PersistentPrefStore,
      public base::ImportantFileWriter::DataSerializer {
 public:
  // |file_message_loop_proxy| is the MessageLoopProxy for a thread on which
  // file I/O can be done.
  JsonPrefStore(const FilePath& pref_filename,
                base::MessageLoopProxy* file_message_loop_proxy);

  // PrefStore overrides:
  virtual ReadResult GetValue(const std::string& key,
                              const base::Value** result) const OVERRIDE;
  virtual void AddObserver(PrefStore::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(PrefStore::Observer* observer) OVERRIDE;
  virtual size_t NumberOfObservers() const OVERRIDE;
  virtual bool IsInitializationComplete() const OVERRIDE;

  // PersistentPrefStore overrides:
  virtual ReadResult GetMutableValue(const std::string& key,
                                     base::Value** result) OVERRIDE;
  virtual void SetValue(const std::string& key, base::Value* value) OVERRIDE;
  virtual void SetValueSilently(const std::string& key,
                                base::Value* value) OVERRIDE;
  virtual void RemoveValue(const std::string& key) OVERRIDE;
  virtual void MarkNeedsEmptyValue(const std::string& key) OVERRIDE;
  virtual bool ReadOnly() const OVERRIDE;
  virtual PrefReadError GetReadError() const OVERRIDE;
  virtual PrefReadError ReadPrefs() OVERRIDE;
  virtual void ReadPrefsAsync(ReadErrorDelegate* error_delegate) OVERRIDE;
  virtual void CommitPendingWrite() OVERRIDE;
  virtual void ReportValueChanged(const std::string& key) OVERRIDE;

  // This method is called after JSON file has been read. Method takes
  // ownership of the |value| pointer. Note, this method is used with
  // asynchronous file reading, so class exposes it only for the internal needs.
  // (read: do not call it manually).
  void OnFileRead(base::Value* value_owned, PrefReadError error, bool no_dir);

 private:
  virtual ~JsonPrefStore();

  // ImportantFileWriter::DataSerializer overrides:
  virtual bool SerializeData(std::string* output) OVERRIDE;

  FilePath path_;
  scoped_refptr<base::MessageLoopProxy> file_message_loop_proxy_;

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
