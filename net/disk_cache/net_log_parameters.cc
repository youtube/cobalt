// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/net_log_parameters.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "net/base/net_errors.h"

namespace disk_cache {

EntryCreationParameters::EntryCreationParameters(
    const std::string& key, bool created)
    : key_(key), created_(created) {
}

Value* EntryCreationParameters::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("key", key_);
  dict->SetBoolean("created", created_);
  return dict;
}

ReadWriteDataParameters::ReadWriteDataParameters(
    int index, int offset, int buf_len, bool truncate)
    : index_(index), offset_(offset), buf_len_(buf_len), truncate_(truncate) {
}

Value* ReadWriteDataParameters::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger("index", index_);
  dict->SetInteger("offset", offset_);
  dict->SetInteger("buf_len", buf_len_);
  if (truncate_)
    dict->SetBoolean("truncate", truncate_);
  return dict;
}


// NetLog parameters logged when non-sparse reads and writes complete.
ReadWriteCompleteParameters::ReadWriteCompleteParameters(int bytes_copied)
    : bytes_copied_(bytes_copied) {
}

Value* ReadWriteCompleteParameters::ToValue() const {
  DCHECK_NE(bytes_copied_, net::ERR_IO_PENDING);
  DictionaryValue* dict = new DictionaryValue();
  if (bytes_copied_ < 0) {
    dict->SetInteger("net_error", bytes_copied_);
  } else {
    dict->SetInteger("bytes_copied", bytes_copied_);
  }
  return dict;
}

SparseOperationParameters::SparseOperationParameters(
    int64 offset, int buff_len)
    : offset_(offset), buff_len_(buff_len) {
}

Value* SparseOperationParameters::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  // Values can only be created with at most 32-bit integers.  Using a string
  // instead circumvents that restriction.
  dict->SetString("offset", base::Int64ToString(offset_));
  dict->SetInteger("buff_len", buff_len_);
  return dict;
}

SparseReadWriteParameters::SparseReadWriteParameters(
    const net::NetLog::Source& source, int child_len)
    : source_(source), child_len_(child_len) {
}

Value* SparseReadWriteParameters::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  dict->Set("source_dependency", source_.ToValue());
  dict->SetInteger("child_len", child_len_);
  return dict;
}

GetAvailableRangeResultParameters::GetAvailableRangeResultParameters(
    int64 start, int result)
    : start_(start), result_(result) {
}

Value* GetAvailableRangeResultParameters::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  if (result_ > 0) {
    dict->SetInteger("length", result_);
    dict->SetString("start",  base::Int64ToString(start_));
  } else {
    dict->SetInteger("net_error", result_);
  }
  return dict;
}

}  // namespace disk_cache
