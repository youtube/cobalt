// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_NET_LOG_PARAMETERS_H_
#define NET_DISK_CACHE_NET_LOG_PARAMETERS_H_
#pragma once

#include <string>

#include "net/base/net_log.h"

// This file contains a set of NetLog::EventParameters shared by EntryImpls and
// MemEntryImpls.
namespace disk_cache {

// NetLog parameters for the creation of an Entry.  Contains the Entry's name
// and whether it was created or opened.
class EntryCreationParameters : public net::NetLog::EventParameters {
 public:
  EntryCreationParameters(const std::string& key, bool created);
  virtual Value* ToValue() const;

 private:
  const std::string key_;
  const bool created_;

  DISALLOW_COPY_AND_ASSIGN(EntryCreationParameters);
};

// NetLog parameters for non-sparse reading and writing to an Entry.
class ReadWriteDataParameters : public net::NetLog::EventParameters {
 public:
  // For reads, |truncate| must be false.
  ReadWriteDataParameters(int index, int offset, int buf_len, bool truncate);
  virtual Value* ToValue() const;

 private:
  const int index_;
  const int offset_;
  const int buf_len_;
  const bool truncate_;

  DISALLOW_COPY_AND_ASSIGN(ReadWriteDataParameters);
};

// NetLog parameters for when a non-sparse read or write completes.
class ReadWriteCompleteParameters : public net::NetLog::EventParameters {
 public:
  // |bytes_copied| is either the number of bytes copied or a network error
  // code.  |bytes_copied| must not be ERR_IO_PENDING, as it's not a valid
  // result for an operation.
  explicit ReadWriteCompleteParameters(int bytes_copied);
  virtual Value* ToValue() const;

 private:
  const int bytes_copied_;

  DISALLOW_COPY_AND_ASSIGN(ReadWriteCompleteParameters);
};

// NetLog parameters for when a sparse operation is started.
class SparseOperationParameters : public net::NetLog::EventParameters {
 public:
  SparseOperationParameters(int64 offset, int buff_len);
  virtual Value* ToValue() const;

 private:
  const int64 offset_;
  const int buff_len_;
};

// NetLog parameters for when a read or write for a sparse entry's child is
// started.
class SparseReadWriteParameters : public net::NetLog::EventParameters {
 public:
  SparseReadWriteParameters(const net::NetLog::Source& source, int child_len);
  virtual Value* ToValue() const;

 private:
  const net::NetLog::Source source_;
  const int child_len_;
};

// NetLog parameters for when a call to GetAvailableRange returns.
class GetAvailableRangeResultParameters : public net::NetLog::EventParameters {
 public:
  // |start| is ignored when |result| < 0.
  GetAvailableRangeResultParameters(int64 start, int result);
  virtual Value* ToValue() const;

 private:
  const int64 start_;
  const int result_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_NET_LOG_CACHE_PARAMETERS_H_
