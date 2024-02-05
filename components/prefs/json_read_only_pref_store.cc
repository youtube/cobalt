// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/json_read_only_pref_store.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram.h"
#include "base/task/sequenced_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/prefs/pref_filter.h"

// Result returned from internal read tasks.
struct JsonReadOnlyPrefStore::ReadResult {
 public:
  ReadResult();
  ~ReadResult();

  std::unique_ptr<base::Value> value;
  PrefReadError error;
  bool no_dir;

 private:
  DISALLOW_COPY_AND_ASSIGN(ReadResult);
};

JsonReadOnlyPrefStore::ReadResult::ReadResult()
    : error(PersistentPrefStore::PREF_READ_ERROR_NONE), no_dir(false) {}

JsonReadOnlyPrefStore::ReadResult::~ReadResult() {}

namespace {

// Some extensions we'll tack on to copies of the Preferences files.
const base::FilePath::CharType kBadExtension[] = FILE_PATH_LITERAL("bad");

PersistentPrefStore::PrefReadError HandleReadErrors(
    const base::Value* value,
    const base::FilePath& path,
    int error_code,
    const std::string& error_msg) {
  if (!value) {
    DVLOG(1) << "Error while loading JSON file: " << error_msg
             << ", file: " << path.value();
    switch (error_code) {
      case JSONFileValueDeserializer::JSON_ACCESS_DENIED:
        return PersistentPrefStore::PREF_READ_ERROR_ACCESS_DENIED;
      case JSONFileValueDeserializer::JSON_CANNOT_READ_FILE:
        return PersistentPrefStore::PREF_READ_ERROR_FILE_OTHER;
      case JSONFileValueDeserializer::JSON_FILE_LOCKED:
        return PersistentPrefStore::PREF_READ_ERROR_FILE_LOCKED;
      case JSONFileValueDeserializer::JSON_NO_SUCH_FILE:
        return PersistentPrefStore::PREF_READ_ERROR_NO_FILE;
      default:
        // JSON errors indicate file corruption of some sort.
        // Since the file is corrupt, move it to the side and continue with
        // empty preferences.  This will result in them losing their settings.
        // We keep the old file for possible support and debugging assistance
        // as well as to detect if they're seeing these errors repeatedly.
        // TODO(erikkay) Instead, use the last known good file.
        base::FilePath bad = path.ReplaceExtension(kBadExtension);

        // If they've ever had a parse error before, put them in another bucket.
        // TODO(erikkay) if we keep this error checking for very long, we may
        // want to differentiate between recent and long ago errors.
        bool bad_existed = base::PathExists(bad);
        base::Move(path, bad);
        return bad_existed ? PersistentPrefStore::PREF_READ_ERROR_JSON_REPEAT
                           : PersistentPrefStore::PREF_READ_ERROR_JSON_PARSE;
    }
  }
  if (!value->is_dict())
    return PersistentPrefStore::PREF_READ_ERROR_JSON_TYPE;
  return PersistentPrefStore::PREF_READ_ERROR_NONE;
}

// Records a sample for |size| in the Settings.JsonDataReadSizeKilobytes
// histogram suffixed with the base name of the JSON file under |path|.
void RecordJsonDataSizeHistogram(const base::FilePath& path, size_t size) {
  std::string spaceless_basename;
  base::ReplaceChars(path.BaseName().MaybeAsASCII(), " ", "_",
                     &spaceless_basename);

  // The histogram below is an expansion of the UMA_HISTOGRAM_CUSTOM_COUNTS
  // macro adapted to allow for a dynamically suffixed histogram name.
  // Note: The factory creates and owns the histogram.
  // This histogram is expired but the code was intentionally left behind so
  // it can be re-enabled on Stable in a single config tweak if needed.
  base::HistogramBase* histogram = base::Histogram::FactoryGet(
      "Settings.JsonDataReadSizeKilobytes." + spaceless_basename, 1, 10000, 50,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  histogram->Add(static_cast<int>(size) / 1024);
}

std::unique_ptr<JsonReadOnlyPrefStore::ReadResult> ReadPrefsFromDisk(
    const base::FilePath& path) {
  int error_code;
  std::string error_msg;
  std::unique_ptr<JsonReadOnlyPrefStore::ReadResult> read_result(
      new JsonReadOnlyPrefStore::ReadResult);
  JSONFileValueDeserializer deserializer(path);
  read_result->value = deserializer.Deserialize(&error_code, &error_msg);
  read_result->error =
      HandleReadErrors(read_result->value.get(), path, error_code, error_msg);
  read_result->no_dir = !base::PathExists(path.DirName());

  if (read_result->error == PersistentPrefStore::PREF_READ_ERROR_NONE)
    RecordJsonDataSizeHistogram(path, deserializer.get_last_read_size());

  return read_result;
}

}  // namespace

JsonReadOnlyPrefStore::JsonReadOnlyPrefStore(
    const base::FilePath& pref_filename,
    std::unique_ptr<PrefFilter> pref_filter,
    scoped_refptr<base::SequencedTaskRunner> file_task_runner)
    : path_(pref_filename),
      file_task_runner_(std::move(file_task_runner)),
      pref_filter_(std::move(pref_filter)),
      initialized_(false),
      filtering_in_progress_(false),
      read_error_(PREF_READ_ERROR_NONE) {
  DCHECK(!path_.empty());
}

bool JsonReadOnlyPrefStore::GetValue(const std::string& key,
                                     const base::Value** result) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const base::Value* tmp = prefs_.FindByDottedPath(key);
  if (!tmp)
    return false;

  if (result)
    *result = tmp;
  return true;
}

base::Value::Dict JsonReadOnlyPrefStore::GetValues()
    const {
  return prefs_.Clone();
}

void JsonReadOnlyPrefStore::AddObserver(PrefStore::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.AddObserver(observer);
}

void JsonReadOnlyPrefStore::RemoveObserver(PrefStore::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  observers_.RemoveObserver(observer);
}

bool JsonReadOnlyPrefStore::HasObservers() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return !observers_.empty();
}

bool JsonReadOnlyPrefStore::IsInitializationComplete() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return initialized_;
}

bool JsonReadOnlyPrefStore::GetMutableValue(const std::string& key,
                                            base::Value** result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::Value* tmp = prefs_.FindByDottedPath(key);
  if (!tmp)
    return false;

  if (result)
    *result = tmp;
  return true;
}

void JsonReadOnlyPrefStore::SetValue(const std::string& key,
                                     base::Value value,
                                     uint32_t flags) {
  NOTIMPLEMENTED();
}

void JsonReadOnlyPrefStore::SetValueSilently(const std::string& key,
                                             base::Value value,
                                             uint32_t flags) {
  NOTIMPLEMENTED();
}

void JsonReadOnlyPrefStore::RemoveValue(const std::string& key,
                                        uint32_t flags) {
  NOTIMPLEMENTED();
}

bool JsonReadOnlyPrefStore::ReadOnly() const {
  return true;
}

PersistentPrefStore::PrefReadError JsonReadOnlyPrefStore::GetReadError() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return read_error_;
}

PersistentPrefStore::PrefReadError JsonReadOnlyPrefStore::ReadPrefs() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  OnFileRead(ReadPrefsFromDisk(path_));
  return filtering_in_progress_ ? PREF_READ_ERROR_ASYNCHRONOUS_TASK_INCOMPLETE
                                : read_error_;
}

void JsonReadOnlyPrefStore::ReadPrefsAsync(ReadErrorDelegate* error_delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  initialized_ = false;
  error_delegate_.reset(error_delegate);

  // Weakly binds the read task so that it doesn't kick in during shutdown.
  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&ReadPrefsFromDisk, path_),
      base::BindOnce(&JsonReadOnlyPrefStore::OnFileRead, AsWeakPtr()));
}

void JsonReadOnlyPrefStore::CommitPendingWrite(
    base::OnceClosure reply_callback,
    base::OnceClosure synchronous_done_callback) {
  NOTIMPLEMENTED();
}

void JsonReadOnlyPrefStore::SchedulePendingLossyWrites() {
  NOTIMPLEMENTED();
}

void JsonReadOnlyPrefStore::ReportValueChanged(const std::string& key,
                                               uint32_t flags) {
  NOTIMPLEMENTED();
}

void JsonReadOnlyPrefStore::ClearMutableValues() {
  NOTIMPLEMENTED();
}

void JsonReadOnlyPrefStore::OnStoreDeletionFromDisk() {
  if (pref_filter_)
    pref_filter_->OnStoreDeletionFromDisk();
}

void JsonReadOnlyPrefStore::OnFileRead(
    std::unique_ptr<ReadResult> read_result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK(read_result);

  base::Value::Dict unfiltered_prefs;

  read_error_ = read_result->error;

  bool initialization_successful = !read_result->no_dir;

  if (initialization_successful) {
    switch (read_error_) {
      case PREF_READ_ERROR_ACCESS_DENIED:
      case PREF_READ_ERROR_FILE_OTHER:
      case PREF_READ_ERROR_FILE_LOCKED:
      case PREF_READ_ERROR_JSON_TYPE:
      case PREF_READ_ERROR_FILE_NOT_SPECIFIED:
        break;
      case PREF_READ_ERROR_NONE:
        DCHECK(read_result->value);
        DCHECK(read_result->value->is_dict());
        unfiltered_prefs = std::move(*read_result->value).TakeDict();
        break;
      case PREF_READ_ERROR_NO_FILE:

      // If the file just doesn't exist, maybe this is first run.  In any case
      // there's no harm in writing out default prefs in this case.
      case PREF_READ_ERROR_JSON_PARSE:
      case PREF_READ_ERROR_JSON_REPEAT:
        break;
      case PREF_READ_ERROR_ASYNCHRONOUS_TASK_INCOMPLETE:
      // This is a special error code to be returned by ReadPrefs when it
      // can't complete synchronously, it should never be returned by the read
      // operation itself.
      case PREF_READ_ERROR_MAX_ENUM:
        NOTREACHED();
        break;
    }
  }

  if (pref_filter_) {
    filtering_in_progress_ = true;
    PrefFilter::PostFilterOnLoadCallback post_filter_on_load_callback(
        base::BindOnce(&JsonReadOnlyPrefStore::FinalizeFileRead, AsWeakPtr(),
                       initialization_successful));
    pref_filter_->FilterOnLoad(std::move(post_filter_on_load_callback),
                               std::move(unfiltered_prefs));
  } else {
    FinalizeFileRead(initialization_successful, std::move(unfiltered_prefs),
                     false);
  }
}

void JsonReadOnlyPrefStore::FinalizeFileRead(
    bool initialization_successful,
    base::Value::Dict prefs,
    bool schedule_write) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  filtering_in_progress_ = false;

  if (!initialization_successful) {
    for (PrefStore::Observer& observer : observers_)
      observer.OnInitializationCompleted(false);
    return;
  }

  prefs_ = std::move(prefs);

  initialized_ = true;

  if (error_delegate_ && read_error_ != PREF_READ_ERROR_NONE)
    error_delegate_->OnError(read_error_);

  for (PrefStore::Observer& observer : observers_)
    observer.OnInitializationCompleted(true);

  return;
}
