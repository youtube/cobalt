// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_IN_FLIGHT_BACKEND_IO_H_
#define NET_DISK_CACHE_IN_FLIGHT_BACKEND_IO_H_
#pragma once

#include <list>
#include <string>

#include "base/message_loop_proxy.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache/entry_impl.h"
#include "net/disk_cache/in_flight_io.h"

namespace disk_cache {

class BackendImpl;

// This class represents a single asynchronous disk cache IO operation while it
// is being bounced between threads.
class BackendIO : public BackgroundIO {
 public:
  BackendIO(InFlightIO* controller, BackendImpl* backend,
            net::CompletionCallback* callback);

  // Runs the actual operation on the background thread.
  void ExecuteOperation();

  // Callback implementation.
  void OnIOComplete(int result);

  // Returns true if this operation is directed to an entry (vs. the backend).
  bool IsEntryOperation();

  net::CompletionCallback* callback() { return callback_; }

  // Grabs an extra reference of entry_.
  void ReferenceEntry();

  // Returns the time that has passed since the operation was created.
  base::TimeDelta ElapsedTime() const;

  // The operations we proxy:
  void Init();
  void OpenEntry(const std::string& key, Entry** entry);
  void CreateEntry(const std::string& key, Entry** entry);
  void DoomEntry(const std::string& key);
  void DoomAllEntries();
  void DoomEntriesBetween(const base::Time initial_time,
                          const base::Time end_time);
  void DoomEntriesSince(const base::Time initial_time);
  void OpenNextEntry(void** iter, Entry** next_entry);
  void OpenPrevEntry(void** iter, Entry** prev_entry);
  void EndEnumeration(void* iterator);
  void CloseEntryImpl(EntryImpl* entry);
  void DoomEntryImpl(EntryImpl* entry);
  void FlushQueue();  // Dummy operation.
  void RunTask(Task* task);
  void ReadData(EntryImpl* entry, int index, int offset, net::IOBuffer* buf,
                int buf_len);
  void WriteData(EntryImpl* entry, int index, int offset, net::IOBuffer* buf,
                 int buf_len, bool truncate);
  void ReadSparseData(EntryImpl* entry, int64 offset, net::IOBuffer* buf,
                      int buf_len);
  void WriteSparseData(EntryImpl* entry, int64 offset, net::IOBuffer* buf,
                       int buf_len);
  void GetAvailableRange(EntryImpl* entry, int64 offset, int len, int64* start);
  void CancelSparseIO(EntryImpl* entry);
  void ReadyForSparseIO(EntryImpl* entry);

 private:
  // There are two types of operations to proxy: regular backend operations are
  // executed sequentially (queued by the message loop). On the other hand,
  // operations targeted to a given entry can be long lived and support multiple
  // simultaneous users (multiple reads or writes to the same entry), and they
  // are subject to throttling, so we keep an explicit queue.
  enum Operation {
    OP_NONE = 0,
    OP_INIT,
    OP_OPEN,
    OP_CREATE,
    OP_DOOM,
    OP_DOOM_ALL,
    OP_DOOM_BETWEEN,
    OP_DOOM_SINCE,
    OP_OPEN_NEXT,
    OP_OPEN_PREV,
    OP_END_ENUMERATION,
    OP_CLOSE_ENTRY,
    OP_DOOM_ENTRY,
    OP_FLUSH_QUEUE,
    OP_RUN_TASK,
    OP_MAX_BACKEND,
    OP_READ,
    OP_WRITE,
    OP_READ_SPARSE,
    OP_WRITE_SPARSE,
    OP_GET_RANGE,
    OP_CANCEL_IO,
    OP_IS_READY
  };

  ~BackendIO();

  void ExecuteBackendOperation();
  void ExecuteEntryOperation();

  BackendImpl* backend_;
  net::CompletionCallback* callback_;
  Operation operation_;
  net::CompletionCallbackImpl<BackendIO> my_callback_;

  // The arguments of all the operations we proxy:
  std::string key_;
  Entry** entry_ptr_;
  base::Time initial_time_;
  base::Time end_time_;
  void** iter_ptr_;
  void* iter_;
  EntryImpl* entry_;
  int index_;
  int offset_;
  scoped_refptr<net::IOBuffer> buf_;
  int buf_len_;
  bool truncate_;
  int64 offset64_;
  int64* start_;
  base::TimeTicks start_time_;
  Task* task_;

  DISALLOW_COPY_AND_ASSIGN(BackendIO);
};

// The specialized controller that keeps track of current operations.
class InFlightBackendIO : public InFlightIO {
 public:
  InFlightBackendIO(BackendImpl* backend,
                    base::MessageLoopProxy* background_thread);
  ~InFlightBackendIO();

  // The operations we proxy:
  void Init(net::CompletionCallback* callback);
  void OpenEntry(const std::string& key, Entry** entry,
                 net::CompletionCallback* callback);
  void CreateEntry(const std::string& key, Entry** entry,
                   net::CompletionCallback* callback);
  void DoomEntry(const std::string& key, net::CompletionCallback* callback);
  void DoomAllEntries(net::CompletionCallback* callback);
  void DoomEntriesBetween(const base::Time initial_time,
                          const base::Time end_time,
                          net::CompletionCallback* callback);
  void DoomEntriesSince(const base::Time initial_time,
                        net::CompletionCallback* callback);
  void OpenNextEntry(void** iter, Entry** next_entry,
                     net::CompletionCallback* callback);
  void OpenPrevEntry(void** iter, Entry** prev_entry,
                     net::CompletionCallback* callback);
  void EndEnumeration(void* iterator);
  void CloseEntryImpl(EntryImpl* entry);
  void DoomEntryImpl(EntryImpl* entry);
  void FlushQueue(net::CompletionCallback* callback);
  void RunTask(Task* task, net::CompletionCallback* callback);
  void ReadData(EntryImpl* entry, int index, int offset, net::IOBuffer* buf,
                int buf_len, net::CompletionCallback* callback);
  void WriteData(EntryImpl* entry, int index, int offset, net::IOBuffer* buf,
                 int buf_len, bool truncate, net::CompletionCallback* callback);
  void ReadSparseData(EntryImpl* entry, int64 offset, net::IOBuffer* buf,
                      int buf_len, net::CompletionCallback* callback);
  void WriteSparseData(EntryImpl* entry, int64 offset, net::IOBuffer* buf,
                       int buf_len, net::CompletionCallback* callback);
  void GetAvailableRange(EntryImpl* entry, int64 offset, int len, int64* start,
                         net::CompletionCallback* callback);
  void CancelSparseIO(EntryImpl* entry);
  void ReadyForSparseIO(EntryImpl* entry, net::CompletionCallback* callback);

  // Blocks until all operations are cancelled or completed.
  void WaitForPendingIO();

  scoped_refptr<base::MessageLoopProxy> background_thread() {
    return background_thread_;
  }

  // Returns true if the current thread is the background thread.
  bool BackgroundIsCurrentThread() {
    return background_thread_->BelongsToCurrentThread();
  }

 protected:
  virtual void OnOperationComplete(BackgroundIO* operation, bool cancel);

 private:
  void PostOperation(BackendIO* operation);

  BackendImpl* backend_;
  scoped_refptr<base::MessageLoopProxy> background_thread_;

  DISALLOW_COPY_AND_ASSIGN(InFlightBackendIO);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_IN_FLIGHT_BACKEND_IO_H_
