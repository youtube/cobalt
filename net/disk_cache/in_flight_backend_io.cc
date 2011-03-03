// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/in_flight_backend_io.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/backend_impl.h"
#include "net/disk_cache/entry_impl.h"
#include "net/disk_cache/histogram_macros.h"

namespace disk_cache {

BackendIO::BackendIO(InFlightIO* controller, BackendImpl* backend,
                     net::CompletionCallback* callback)
    : BackgroundIO(controller), backend_(backend), callback_(callback),
      operation_(OP_NONE),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          my_callback_(this, &BackendIO::OnIOComplete)) {
  start_time_ = base::TimeTicks::Now();
}

// Runs on the background thread.
void BackendIO::ExecuteOperation() {
  if (IsEntryOperation())
    return ExecuteEntryOperation();

  ExecuteBackendOperation();
}

// Runs on the background thread.
void BackendIO::OnIOComplete(int result) {
  DCHECK(IsEntryOperation());
  DCHECK_NE(result, net::ERR_IO_PENDING);
  result_ = result;
  controller_->OnIOComplete(this);
}

bool BackendIO::IsEntryOperation() {
  return operation_ > OP_MAX_BACKEND;
}

// Runs on the background thread.
void BackendIO::ReferenceEntry() {
  entry_->AddRef();
}

base::TimeDelta BackendIO::ElapsedTime() const {
  return base::TimeTicks::Now() - start_time_;
}

void BackendIO::Init() {
  operation_ = OP_INIT;
}

void BackendIO::OpenEntry(const std::string& key, Entry** entry) {
  operation_ = OP_OPEN;
  key_ = key;
  entry_ptr_ = entry;
}

void BackendIO::CreateEntry(const std::string& key, Entry** entry) {
  operation_ = OP_CREATE;
  key_ = key;
  entry_ptr_ = entry;
}

void BackendIO::DoomEntry(const std::string& key) {
  operation_ = OP_DOOM;
  key_ = key;
}

void BackendIO::DoomAllEntries() {
  operation_ = OP_DOOM_ALL;
}

void BackendIO::DoomEntriesBetween(const base::Time initial_time,
                                   const base::Time end_time) {
  operation_ = OP_DOOM_BETWEEN;
  initial_time_ = initial_time;
  end_time_ = end_time;
}

void BackendIO::DoomEntriesSince(const base::Time initial_time) {
  operation_ = OP_DOOM_SINCE;
  initial_time_ = initial_time;
}

void BackendIO::OpenNextEntry(void** iter, Entry** next_entry) {
  operation_ = OP_OPEN_NEXT;
  iter_ptr_ = iter;
  entry_ptr_ = next_entry;
}

void BackendIO::OpenPrevEntry(void** iter, Entry** prev_entry) {
  operation_ = OP_OPEN_PREV;
  iter_ptr_ = iter;
  entry_ptr_ = prev_entry;
}

void BackendIO::EndEnumeration(void* iterator) {
  operation_ = OP_END_ENUMERATION;
  iter_ = iterator;
}

void BackendIO::CloseEntryImpl(EntryImpl* entry) {
  operation_ = OP_CLOSE_ENTRY;
  entry_ = entry;
}

void BackendIO::DoomEntryImpl(EntryImpl* entry) {
  operation_ = OP_DOOM_ENTRY;
  entry_ = entry;
}

void BackendIO::FlushQueue() {
  operation_ = OP_FLUSH_QUEUE;
}

void BackendIO::RunTask(Task* task) {
  operation_ = OP_RUN_TASK;
  task_ = task;
}

void BackendIO::ReadData(EntryImpl* entry, int index, int offset,
                         net::IOBuffer* buf, int buf_len) {
  operation_ = OP_READ;
  entry_ = entry;
  index_ = index;
  offset_ = offset;
  buf_ = buf;
  buf_len_ = buf_len;
}

void BackendIO::WriteData(EntryImpl* entry, int index, int offset,
                          net::IOBuffer* buf, int buf_len, bool truncate) {
  operation_ = OP_WRITE;
  entry_ = entry;
  index_ = index;
  offset_ = offset;
  buf_ = buf;
  buf_len_ = buf_len;
  truncate_ = truncate;
}

void BackendIO::ReadSparseData(EntryImpl* entry, int64 offset,
                               net::IOBuffer* buf, int buf_len) {
  operation_ = OP_READ_SPARSE;
  entry_ = entry;
  offset64_ = offset;
  buf_ = buf;
  buf_len_ = buf_len;
}

void BackendIO::WriteSparseData(EntryImpl* entry, int64 offset,
                                net::IOBuffer* buf, int buf_len) {
  operation_ = OP_WRITE_SPARSE;
  entry_ = entry;
  offset64_ = offset;
  buf_ = buf;
  buf_len_ = buf_len;
}

void BackendIO::GetAvailableRange(EntryImpl* entry, int64 offset, int len,
                                  int64* start) {
  operation_ = OP_GET_RANGE;
  entry_ = entry;
  offset64_ = offset;
  buf_len_ = len;
  start_ = start;
}

void BackendIO::CancelSparseIO(EntryImpl* entry) {
  operation_ = OP_CANCEL_IO;
  entry_ = entry;
}

void BackendIO::ReadyForSparseIO(EntryImpl* entry) {
  operation_ = OP_IS_READY;
  entry_ = entry;
}

BackendIO::~BackendIO() {}

// Runs on the background thread.
void BackendIO::ExecuteBackendOperation() {
  switch (operation_) {
    case OP_INIT:
      result_ = backend_->SyncInit();
      break;
    case OP_OPEN:
      result_ = backend_->SyncOpenEntry(key_, entry_ptr_);
      break;
    case OP_CREATE:
      result_ = backend_->SyncCreateEntry(key_, entry_ptr_);
      break;
    case OP_DOOM:
      result_ = backend_->SyncDoomEntry(key_);
      break;
    case OP_DOOM_ALL:
      result_ = backend_->SyncDoomAllEntries();
      break;
    case OP_DOOM_BETWEEN:
      result_ = backend_->SyncDoomEntriesBetween(initial_time_, end_time_);
      break;
    case OP_DOOM_SINCE:
      result_ = backend_->SyncDoomEntriesSince(initial_time_);
      break;
    case OP_OPEN_NEXT:
      result_ = backend_->SyncOpenNextEntry(iter_ptr_, entry_ptr_);
      break;
    case OP_OPEN_PREV:
      result_ = backend_->SyncOpenPrevEntry(iter_ptr_, entry_ptr_);
      break;
    case OP_END_ENUMERATION:
      backend_->SyncEndEnumeration(iter_);
      result_ = net::OK;
      break;
    case OP_CLOSE_ENTRY:
      entry_->Release();
      result_ = net::OK;
      break;
    case OP_DOOM_ENTRY:
      entry_->DoomImpl();
      result_ = net::OK;
      break;
    case OP_FLUSH_QUEUE:
      result_ = net::OK;
      break;
    case OP_RUN_TASK:
      task_->Run();
      delete task_;
      result_ = net::OK;
      break;
    default:
      NOTREACHED() << "Invalid Operation";
      result_ = net::ERR_UNEXPECTED;
  }
  DCHECK_NE(net::ERR_IO_PENDING, result_);
  controller_->OnIOComplete(this);
}

// Runs on the background thread.
void BackendIO::ExecuteEntryOperation() {
  switch (operation_) {
    case OP_READ:
      result_ = entry_->ReadDataImpl(index_, offset_, buf_, buf_len_,
                                     &my_callback_);
      break;
    case OP_WRITE:
      result_ = entry_->WriteDataImpl(index_, offset_, buf_, buf_len_,
                                      &my_callback_, truncate_);
      break;
    case OP_READ_SPARSE:
      result_ = entry_->ReadSparseDataImpl(offset64_, buf_, buf_len_,
                                           &my_callback_);
      break;
    case OP_WRITE_SPARSE:
      result_ = entry_->WriteSparseDataImpl(offset64_, buf_, buf_len_,
                                            &my_callback_);
      break;
    case OP_GET_RANGE:
      result_ = entry_->GetAvailableRangeImpl(offset64_, buf_len_, start_);
      break;
    case OP_CANCEL_IO:
      entry_->CancelSparseIOImpl();
      result_ = net::OK;
      break;
    case OP_IS_READY:
      result_ = entry_->ReadyForSparseIOImpl(&my_callback_);
      break;
    default:
      NOTREACHED() << "Invalid Operation";
      result_ = net::ERR_UNEXPECTED;
  }
  if (result_ != net::ERR_IO_PENDING)
    controller_->OnIOComplete(this);
}

// ---------------------------------------------------------------------------

InFlightBackendIO::InFlightBackendIO(BackendImpl* backend,
                    base::MessageLoopProxy* background_thread)
    : backend_(backend),
      background_thread_(background_thread) {
}

InFlightBackendIO::~InFlightBackendIO() {
}

void InFlightBackendIO::Init(CompletionCallback* callback) {
  scoped_refptr<BackendIO> operation(new BackendIO(this, backend_, callback));
  operation->Init();
  PostOperation(operation);
}

void InFlightBackendIO::OpenEntry(const std::string& key, Entry** entry,
                                  CompletionCallback* callback) {
  scoped_refptr<BackendIO> operation(new BackendIO(this, backend_, callback));
  operation->OpenEntry(key, entry);
  PostOperation(operation);
}

void InFlightBackendIO::CreateEntry(const std::string& key, Entry** entry,
                                    CompletionCallback* callback) {
  scoped_refptr<BackendIO> operation(new BackendIO(this, backend_, callback));
  operation->CreateEntry(key, entry);
  PostOperation(operation);
}

void InFlightBackendIO::DoomEntry(const std::string& key,
                                  CompletionCallback* callback) {
  scoped_refptr<BackendIO> operation(new BackendIO(this, backend_, callback));
  operation->DoomEntry(key);
  PostOperation(operation);
}

void InFlightBackendIO::DoomAllEntries(CompletionCallback* callback) {
  scoped_refptr<BackendIO> operation(new BackendIO(this, backend_, callback));
  operation->DoomAllEntries();
  PostOperation(operation);
}

void InFlightBackendIO::DoomEntriesBetween(const base::Time initial_time,
                        const base::Time end_time,
                        CompletionCallback* callback) {
  scoped_refptr<BackendIO> operation(new BackendIO(this, backend_, callback));
  operation->DoomEntriesBetween(initial_time, end_time);
  PostOperation(operation);
}

void InFlightBackendIO::DoomEntriesSince(const base::Time initial_time,
                                         CompletionCallback* callback) {
  scoped_refptr<BackendIO> operation(new BackendIO(this, backend_, callback));
  operation->DoomEntriesSince(initial_time);
  PostOperation(operation);
}

void InFlightBackendIO::OpenNextEntry(void** iter, Entry** next_entry,
                                      CompletionCallback* callback) {
  scoped_refptr<BackendIO> operation(new BackendIO(this, backend_, callback));
  operation->OpenNextEntry(iter, next_entry);
  PostOperation(operation);
}

void InFlightBackendIO::OpenPrevEntry(void** iter, Entry** prev_entry,
                                      CompletionCallback* callback) {
  scoped_refptr<BackendIO> operation(new BackendIO(this, backend_, callback));
  operation->OpenPrevEntry(iter, prev_entry);
  PostOperation(operation);
}

void InFlightBackendIO::EndEnumeration(void* iterator) {
  scoped_refptr<BackendIO> operation(new BackendIO(this, backend_, NULL));
  operation->EndEnumeration(iterator);
  PostOperation(operation);
}

void InFlightBackendIO::CloseEntryImpl(EntryImpl* entry) {
  scoped_refptr<BackendIO> operation(new BackendIO(this, backend_, NULL));
  operation->CloseEntryImpl(entry);
  PostOperation(operation);
}

void InFlightBackendIO::DoomEntryImpl(EntryImpl* entry) {
  scoped_refptr<BackendIO> operation(new BackendIO(this, backend_, NULL));
  operation->DoomEntryImpl(entry);
  PostOperation(operation);
}

void InFlightBackendIO::FlushQueue(net::CompletionCallback* callback) {
  scoped_refptr<BackendIO> operation(new BackendIO(this, backend_, callback));
  operation->FlushQueue();
  PostOperation(operation);
}

void InFlightBackendIO::RunTask(Task* task, net::CompletionCallback* callback) {
  scoped_refptr<BackendIO> operation(new BackendIO(this, backend_, callback));
  operation->RunTask(task);
  PostOperation(operation);
}

void InFlightBackendIO::ReadData(EntryImpl* entry, int index, int offset,
                                 net::IOBuffer* buf, int buf_len,
                                 CompletionCallback* callback) {
  scoped_refptr<BackendIO> operation(new BackendIO(this, backend_, callback));
  operation->ReadData(entry, index, offset, buf, buf_len);
  PostOperation(operation);
}

void InFlightBackendIO::WriteData(EntryImpl* entry, int index, int offset,
                                  net::IOBuffer* buf, int buf_len,
                                  bool truncate,
                                  CompletionCallback* callback) {
  scoped_refptr<BackendIO> operation(new BackendIO(this, backend_, callback));
  operation->WriteData(entry, index, offset, buf, buf_len, truncate);
  PostOperation(operation);
}

void InFlightBackendIO::ReadSparseData(EntryImpl* entry, int64 offset,
                                       net::IOBuffer* buf, int buf_len,
                                       CompletionCallback* callback) {
  scoped_refptr<BackendIO> operation(new BackendIO(this, backend_, callback));
  operation->ReadSparseData(entry, offset, buf, buf_len);
  PostOperation(operation);
}

void InFlightBackendIO::WriteSparseData(EntryImpl* entry, int64 offset,
                                        net::IOBuffer* buf, int buf_len,
                                        CompletionCallback* callback) {
  scoped_refptr<BackendIO> operation(new BackendIO(this, backend_, callback));
  operation->WriteSparseData(entry, offset, buf, buf_len);
  PostOperation(operation);
}

void InFlightBackendIO::GetAvailableRange(EntryImpl* entry, int64 offset,
                                          int len, int64* start,
                                          CompletionCallback* callback) {
  scoped_refptr<BackendIO> operation(new BackendIO(this, backend_, callback));
  operation->GetAvailableRange(entry, offset, len, start);
  PostOperation(operation);
}

void InFlightBackendIO::CancelSparseIO(EntryImpl* entry) {
  scoped_refptr<BackendIO> operation(new BackendIO(this, backend_, NULL));
  operation->CancelSparseIO(entry);
  PostOperation(operation);
}

void InFlightBackendIO::ReadyForSparseIO(EntryImpl* entry,
                                         CompletionCallback* callback) {
  scoped_refptr<BackendIO> operation(new BackendIO(this, backend_, callback));
  operation->ReadyForSparseIO(entry);
  PostOperation(operation);
}

void InFlightBackendIO::WaitForPendingIO() {
  InFlightIO::WaitForPendingIO();
}

void InFlightBackendIO::OnOperationComplete(BackgroundIO* operation,
                                            bool cancel) {
  BackendIO* op = static_cast<BackendIO*>(operation);

  if (op->IsEntryOperation()) {
    CACHE_UMA(TIMES, "TotalIOTime", 0, op->ElapsedTime());
  }

  if (op->callback() && (!cancel || op->IsEntryOperation()))
    op->callback()->Run(op->result());
}

void InFlightBackendIO::PostOperation(BackendIO* operation) {
  background_thread_->PostTask(FROM_HERE,
      NewRunnableMethod(operation, &BackendIO::ExecuteOperation));
  OnOperationPosted(operation);
}

}  // namespace
