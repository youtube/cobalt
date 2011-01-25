// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/entry_impl.h"

#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/values.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/backend_impl.h"
#include "net/disk_cache/bitmap.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/histogram_macros.h"
#include "net/disk_cache/sparse_control.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

namespace {

// Index for the file used to store the key, if any (files_[kKeyFileIndex]).
const int kKeyFileIndex = 3;

// NetLog parameters for the creation of an EntryImpl.  Contains an entry's name
// and whether it was created or opened.
class EntryCreationParameters : public net::NetLog::EventParameters {
 public:
  EntryCreationParameters(const std::string& key, bool created)
      : key_(key), created_(created) {
  }

  Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetString("key", key_);
    dict->SetBoolean("created", created_);
    return dict;
  }

 private:
  const std::string key_;
  const bool created_;

  DISALLOW_COPY_AND_ASSIGN(EntryCreationParameters);
};

// NetLog parameters for non-sparse reading and writing to an EntryImpl.
class ReadWriteDataParams : public net::NetLog::EventParameters {
 public:
  // For reads, |truncate| must be false.
  ReadWriteDataParams(int index, int offset, int buf_len, bool truncate)
      : index_(index), offset_(offset), buf_len_(buf_len), truncate_(truncate) {
  }

  Value* ToValue() const {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetInteger("index", index_);
    dict->SetInteger("offset", offset_);
    dict->SetInteger("buf_len", buf_len_);
    if (truncate_)
      dict->SetBoolean("truncate", truncate_);
    return dict;
  }

 private:
  const int index_;
  const int offset_;
  const int buf_len_;
  const bool truncate_;

  DISALLOW_COPY_AND_ASSIGN(ReadWriteDataParams);
};

// NetLog parameters logged when non-sparse reads and writes complete.
class FileIOCompleteParameters : public net::NetLog::EventParameters {
 public:
  // |bytes_copied| is either the number of bytes copied or a network error
  // code.  |bytes_copied| must not be ERR_IO_PENDING, as it's not a valid
  // result for an operation.
  explicit FileIOCompleteParameters(int bytes_copied)
      : bytes_copied_(bytes_copied) {
  }

  Value* ToValue() const {
    DCHECK_NE(bytes_copied_, net::ERR_IO_PENDING);
    DictionaryValue* dict = new DictionaryValue();
    if (bytes_copied_ < 0) {
      dict->SetInteger("net_error", bytes_copied_);
    } else {
      dict->SetInteger("bytes_copied", bytes_copied_);
    }
    return dict;
  }

 private:
  const int bytes_copied_;

  DISALLOW_COPY_AND_ASSIGN(FileIOCompleteParameters);
};

// This class implements FileIOCallback to buffer the callback from a file IO
// operation from the actual net class.
class SyncCallback: public disk_cache::FileIOCallback {
 public:
  // |end_event_type| is the event type to log on completion.  Logs nothing on
  // discard, or when the NetLog is not set to log all events.
  SyncCallback(disk_cache::EntryImpl* entry, net::IOBuffer* buffer,
               net::CompletionCallback* callback,
               net::NetLog::EventType end_event_type)
      : entry_(entry), callback_(callback), buf_(buffer),
        start_(TimeTicks::Now()), end_event_type_(end_event_type) {
    entry->AddRef();
    entry->IncrementIoCount();
  }
  ~SyncCallback() {}

  virtual void OnFileIOComplete(int bytes_copied);
  void Discard();

 private:
  disk_cache::EntryImpl* entry_;
  net::CompletionCallback* callback_;
  scoped_refptr<net::IOBuffer> buf_;
  TimeTicks start_;
  net::NetLog::EventType end_event_type_;

  DISALLOW_COPY_AND_ASSIGN(SyncCallback);
};

void SyncCallback::OnFileIOComplete(int bytes_copied) {
  entry_->DecrementIoCount();
  if (callback_) {
    if (entry_->net_log().IsLoggingAllEvents()) {
      entry_->net_log().EndEvent(
          end_event_type_,
          make_scoped_refptr(new FileIOCompleteParameters(bytes_copied)));
    }
    entry_->ReportIOTime(disk_cache::EntryImpl::kAsyncIO, start_);
    callback_->Run(bytes_copied);
  }
  entry_->Release();
  delete this;
}

void SyncCallback::Discard() {
  callback_ = NULL;
  buf_ = NULL;
  OnFileIOComplete(0);
}

const int kMaxBufferSize = 1024 * 1024;  // 1 MB.

}  // namespace

namespace disk_cache {

// This class handles individual memory buffers that store data before it is
// sent to disk. The buffer can start at any offset, but if we try to write to
// anywhere in the first 16KB of the file (kMaxBlockSize), we set the offset to
// zero. The buffer grows up to a size determined by the backend, to keep the
// total memory used under control.
class EntryImpl::UserBuffer {
 public:
  explicit UserBuffer(BackendImpl* backend)
      : backend_(backend->GetWeakPtr()), offset_(0), grow_allowed_(true) {
    buffer_.reserve(kMaxBlockSize);
  }
  ~UserBuffer() {
    if (backend_)
      backend_->BufferDeleted(capacity() - kMaxBlockSize);
  }

  // Returns true if we can handle writing |len| bytes to |offset|.
  bool PreWrite(int offset, int len);

  // Truncates the buffer to |offset| bytes.
  void Truncate(int offset);

  // Writes |len| bytes from |buf| at the given |offset|.
  void Write(int offset, net::IOBuffer* buf, int len);

  // Returns true if we can read |len| bytes from |offset|, given that the
  // actual file has |eof| bytes stored. Note that the number of bytes to read
  // may be modified by this method even though it returns false: that means we
  // should do a smaller read from disk.
  bool PreRead(int eof, int offset, int* len);

  // Read |len| bytes from |buf| at the given |offset|.
  int Read(int offset, net::IOBuffer* buf, int len);

  // Prepare this buffer for reuse.
  void Reset();

  char* Data() { return buffer_.size() ? &buffer_[0] : NULL; }
  int Size() { return static_cast<int>(buffer_.size()); }
  int Start() { return offset_; }
  int End() { return offset_ + Size(); }

 private:
  int capacity() { return static_cast<int>(buffer_.capacity()); }
  bool GrowBuffer(int required, int limit);

  base::WeakPtr<BackendImpl> backend_;
  int offset_;
  std::vector<char> buffer_;
  bool grow_allowed_;
  DISALLOW_COPY_AND_ASSIGN(UserBuffer);
};

bool EntryImpl::UserBuffer::PreWrite(int offset, int len) {
  DCHECK_GE(offset, 0);
  DCHECK_GE(len, 0);
  DCHECK_GE(offset + len, 0);

  // We don't want to write before our current start.
  if (offset < offset_)
    return false;

  // Lets get the common case out of the way.
  if (offset + len <= capacity())
    return true;

  // If we are writing to the first 16K (kMaxBlockSize), we want to keep the
  // buffer offset_ at 0.
  if (!Size() && offset > kMaxBlockSize)
    return GrowBuffer(len, kMaxBufferSize);

  int required = offset - offset_ + len;
  return GrowBuffer(required, kMaxBufferSize * 6 / 5);
}

void EntryImpl::UserBuffer::Truncate(int offset) {
  DCHECK_GE(offset, 0);
  DCHECK_GE(offset, offset_);
  DVLOG(3) << "Buffer truncate at " << offset << " current " << offset_;

  offset -= offset_;
  if (Size() >= offset)
    buffer_.resize(offset);
}

void EntryImpl::UserBuffer::Write(int offset, net::IOBuffer* buf, int len) {
  DCHECK_GE(offset, 0);
  DCHECK_GE(len, 0);
  DCHECK_GE(offset + len, 0);
  DCHECK_GE(offset, offset_);
  DVLOG(3) << "Buffer write at " << offset << " current " << offset_;

  if (!Size() && offset > kMaxBlockSize)
    offset_ = offset;

  offset -= offset_;

  if (offset > Size())
    buffer_.resize(offset);

  if (!len)
    return;

  char* buffer = buf->data();
  int valid_len = Size() - offset;
  int copy_len = std::min(valid_len, len);
  if (copy_len) {
    memcpy(&buffer_[offset], buffer, copy_len);
    len -= copy_len;
    buffer += copy_len;
  }
  if (!len)
    return;

  buffer_.insert(buffer_.end(), buffer, buffer + len);
}

bool EntryImpl::UserBuffer::PreRead(int eof, int offset, int* len) {
  DCHECK_GE(offset, 0);
  DCHECK_GT(*len, 0);

  if (offset < offset_) {
    // We are reading before this buffer.
    if (offset >= eof)
      return true;

    // If the read overlaps with the buffer, change its length so that there is
    // no overlap.
    *len = std::min(*len, offset_ - offset);
    *len = std::min(*len, eof - offset);

    // We should read from disk.
    return false;
  }

  if (!Size())
    return false;

  // See if we can fulfill the first part of the operation.
  return (offset - offset_ < Size());
}

int EntryImpl::UserBuffer::Read(int offset, net::IOBuffer* buf, int len) {
  DCHECK_GE(offset, 0);
  DCHECK_GT(len, 0);
  DCHECK(Size() || offset < offset_);

  int clean_bytes = 0;
  if (offset < offset_) {
    // We don't have a file so lets fill the first part with 0.
    clean_bytes = std::min(offset_ - offset, len);
    memset(buf->data(), 0, clean_bytes);
    if (len == clean_bytes)
      return len;
    offset = offset_;
    len -= clean_bytes;
  }

  int start = offset - offset_;
  int available = Size() - start;
  DCHECK_GE(start, 0);
  DCHECK_GE(available, 0);
  len = std::min(len, available);
  memcpy(buf->data() + clean_bytes, &buffer_[start], len);
  return len + clean_bytes;
}

void EntryImpl::UserBuffer::Reset() {
  if (!grow_allowed_) {
    if (backend_)
      backend_->BufferDeleted(capacity() - kMaxBlockSize);
    grow_allowed_ = true;
    std::vector<char> tmp;
    buffer_.swap(tmp);
    buffer_.reserve(kMaxBlockSize);
  }
  offset_ = 0;
  buffer_.clear();
}

bool EntryImpl::UserBuffer::GrowBuffer(int required, int limit) {
  DCHECK_GE(required, 0);
  int current_size = capacity();
  if (required <= current_size)
    return true;

  if (required > limit)
    return false;

  if (!backend_)
    return false;

  int to_add = std::max(required - current_size, kMaxBlockSize * 4);
  to_add = std::max(current_size, to_add);
  required = std::min(current_size + to_add, limit);

  grow_allowed_ = backend_->IsAllocAllowed(current_size, required);
  if (!grow_allowed_)
    return false;

  DVLOG(3) << "Buffer grow to " << required;

  buffer_.reserve(required);
  return true;
}

// ------------------------------------------------------------------------

EntryImpl::EntryImpl(BackendImpl* backend, Addr address, bool read_only)
    : entry_(NULL, Addr(0)), node_(NULL, Addr(0)), read_only_(read_only) {
  entry_.LazyInit(backend->File(address), address);
  doomed_ = false;
  backend_ = backend;
  for (int i = 0; i < kNumStreams; i++) {
    unreported_size_[i] = 0;
  }
}

void EntryImpl::DoomImpl() {
  if (doomed_)
    return;

  SetPointerForInvalidEntry(backend_->GetCurrentEntryId());
  backend_->InternalDoomEntry(this);
}

int EntryImpl::ReadDataImpl(int index, int offset, net::IOBuffer* buf,
                            int buf_len, CompletionCallback* callback) {
  if (net_log_.IsLoggingAllEvents()) {
    net_log_.BeginEvent(
        net::NetLog::TYPE_DISK_CACHE_READ_DATA,
        make_scoped_refptr(
            new ReadWriteDataParams(index, offset, buf_len, false)));
  }

  int result = InternalReadData(index, offset, buf, buf_len, callback);

  if (result != net::ERR_IO_PENDING && net_log_.IsLoggingAllEvents()) {
    net_log_.EndEvent(
        net::NetLog::TYPE_DISK_CACHE_READ_DATA,
        make_scoped_refptr(new FileIOCompleteParameters(result)));
  }
  return result;
}

int EntryImpl::WriteDataImpl(int index, int offset, net::IOBuffer* buf,
                             int buf_len, CompletionCallback* callback,
                             bool truncate) {
  if (net_log_.IsLoggingAllEvents()) {
    net_log_.BeginEvent(
        net::NetLog::TYPE_DISK_CACHE_WRITE_DATA,
        make_scoped_refptr(
            new ReadWriteDataParams(index, offset, buf_len, truncate)));
  }

  int result = InternalWriteData(index, offset, buf, buf_len, callback,
                                 truncate);

  if (result != net::ERR_IO_PENDING && net_log_.IsLoggingAllEvents()) {
    net_log_.EndEvent(
        net::NetLog::TYPE_DISK_CACHE_WRITE_DATA,
        make_scoped_refptr(new FileIOCompleteParameters(result)));
  }
  return result;
}

int EntryImpl::ReadSparseDataImpl(int64 offset, net::IOBuffer* buf, int buf_len,
                                  CompletionCallback* callback) {
  DCHECK(node_.Data()->dirty || read_only_);
  int result = InitSparseData();
  if (net::OK != result)
    return result;

  TimeTicks start = TimeTicks::Now();
  result = sparse_->StartIO(SparseControl::kReadOperation, offset, buf, buf_len,
                            callback);
  ReportIOTime(kSparseRead, start);
  return result;
}

int EntryImpl::WriteSparseDataImpl(int64 offset, net::IOBuffer* buf,
                                   int buf_len, CompletionCallback* callback) {
  DCHECK(node_.Data()->dirty || read_only_);
  int result = InitSparseData();
  if (net::OK != result)
    return result;

  TimeTicks start = TimeTicks::Now();
  result = sparse_->StartIO(SparseControl::kWriteOperation, offset, buf,
                            buf_len, callback);
  ReportIOTime(kSparseWrite, start);
  return result;
}

int EntryImpl::GetAvailableRangeImpl(int64 offset, int len, int64* start) {
  int result = InitSparseData();
  if (net::OK != result)
    return result;

  return sparse_->GetAvailableRange(offset, len, start);
}

void EntryImpl::CancelSparseIOImpl() {
  if (!sparse_.get())
    return;

  sparse_->CancelIO();
}

int EntryImpl::ReadyForSparseIOImpl(CompletionCallback* callback) {
  DCHECK(sparse_.get());
  return sparse_->ReadyToUse(callback);
}

uint32 EntryImpl::GetHash() {
  return entry_.Data()->hash;
}

bool EntryImpl::CreateEntry(Addr node_address, const std::string& key,
                            uint32 hash) {
  Trace("Create entry In");
  EntryStore* entry_store = entry_.Data();
  RankingsNode* node = node_.Data();
  memset(entry_store, 0, sizeof(EntryStore) * entry_.address().num_blocks());
  memset(node, 0, sizeof(RankingsNode));
  if (!node_.LazyInit(backend_->File(node_address), node_address))
    return false;

  entry_store->rankings_node = node_address.value();
  node->contents = entry_.address().value();

  entry_store->hash = hash;
  entry_store->creation_time = Time::Now().ToInternalValue();
  entry_store->key_len = static_cast<int32>(key.size());
  if (entry_store->key_len > kMaxInternalKeyLength) {
    Addr address(0);
    if (!CreateBlock(entry_store->key_len + 1, &address))
      return false;

    entry_store->long_key = address.value();
    File* key_file = GetBackingFile(address, kKeyFileIndex);
    key_ = key;

    size_t offset = 0;
    if (address.is_block_file())
      offset = address.start_block() * address.BlockSize() + kBlockHeaderSize;

    if (!key_file || !key_file->Write(key.data(), key.size(), offset)) {
      DeleteData(address, kKeyFileIndex);
      return false;
    }

    if (address.is_separate_file())
      key_file->SetLength(key.size() + 1);
  } else {
    memcpy(entry_store->key, key.data(), key.size());
    entry_store->key[key.size()] = '\0';
  }
  backend_->ModifyStorageSize(0, static_cast<int32>(key.size()));
  CACHE_UMA(COUNTS, "KeySize", 0, static_cast<int32>(key.size()));
  node->dirty = backend_->GetCurrentEntryId();
  Log("Create Entry ");
  return true;
}

bool EntryImpl::IsSameEntry(const std::string& key, uint32 hash) {
  if (entry_.Data()->hash != hash ||
      static_cast<size_t>(entry_.Data()->key_len) != key.size())
    return false;

  std::string my_key = GetKey();
  return key.compare(my_key) ? false : true;
}

void EntryImpl::InternalDoom() {
  net_log_.AddEvent(net::NetLog::TYPE_DISK_CACHE_DOOM, NULL);
  DCHECK(node_.HasData());
  if (!node_.Data()->dirty) {
    node_.Data()->dirty = backend_->GetCurrentEntryId();
    node_.Store();
  }
  doomed_ = true;
}

void EntryImpl::DeleteEntryData(bool everything) {
  DCHECK(doomed_ || !everything);

  if (GetEntryFlags() & PARENT_ENTRY) {
    // We have some child entries that must go away.
    SparseControl::DeleteChildren(this);
  }

  if (GetDataSize(0))
    CACHE_UMA(COUNTS, "DeleteHeader", 0, GetDataSize(0));
  if (GetDataSize(1))
    CACHE_UMA(COUNTS, "DeleteData", 0, GetDataSize(1));
  for (int index = 0; index < kNumStreams; index++) {
    Addr address(entry_.Data()->data_addr[index]);
    if (address.is_initialized()) {
      backend_->ModifyStorageSize(entry_.Data()->data_size[index] -
                                      unreported_size_[index], 0);
      entry_.Data()->data_addr[index] = 0;
      entry_.Data()->data_size[index] = 0;
      entry_.Store();
      DeleteData(address, index);
    }
  }

  if (!everything)
    return;

  // Remove all traces of this entry.
  backend_->RemoveEntry(this);

  Addr address(entry_.Data()->long_key);
  DeleteData(address, kKeyFileIndex);
  backend_->ModifyStorageSize(entry_.Data()->key_len, 0);

  memset(node_.buffer(), 0, node_.size());
  memset(entry_.buffer(), 0, entry_.size());
  node_.Store();
  entry_.Store();

  backend_->DeleteBlock(node_.address(), false);
  backend_->DeleteBlock(entry_.address(), false);
}

CacheAddr EntryImpl::GetNextAddress() {
  return entry_.Data()->next;
}

void EntryImpl::SetNextAddress(Addr address) {
  DCHECK_NE(address.value(), entry_.address().value());
  entry_.Data()->next = address.value();
  bool success = entry_.Store();
  DCHECK(success);
}

bool EntryImpl::LoadNodeAddress() {
  Addr address(entry_.Data()->rankings_node);
  if (!node_.LazyInit(backend_->File(address), address))
    return false;
  return node_.Load();
}

bool EntryImpl::Update() {
  DCHECK(node_.HasData());

  if (read_only_)
    return true;

  RankingsNode* rankings = node_.Data();
  if (!rankings->dirty) {
    rankings->dirty = backend_->GetCurrentEntryId();
    if (!node_.Store())
      return false;
  }
  return true;
}

bool EntryImpl::IsDirty(int32 current_id) {
  DCHECK(node_.HasData());
  // We are checking if the entry is valid or not. If there is a pointer here,
  // we should not be checking the entry.
  if (node_.Data()->dummy)
    return true;

  return node_.Data()->dirty && current_id != node_.Data()->dirty;
}

void EntryImpl::ClearDirtyFlag() {
  node_.Data()->dirty = 0;
}

void EntryImpl::SetPointerForInvalidEntry(int32 new_id) {
  node_.Data()->dirty = new_id;
  node_.Data()->dummy = 0;
  node_.Store();
}

bool EntryImpl::SanityCheck() {
  if (!entry_.Data()->rankings_node || !entry_.Data()->key_len)
    return false;

  Addr rankings_addr(entry_.Data()->rankings_node);
  if (!rankings_addr.is_initialized() || rankings_addr.is_separate_file() ||
      rankings_addr.file_type() != RANKINGS)
    return false;

  Addr next_addr(entry_.Data()->next);
  if (next_addr.is_initialized() &&
      (next_addr.is_separate_file() || next_addr.file_type() != BLOCK_256))
    return false;

  return true;
}

void EntryImpl::IncrementIoCount() {
  backend_->IncrementIoCount();
}

void EntryImpl::DecrementIoCount() {
  backend_->DecrementIoCount();
}

void EntryImpl::SetTimes(base::Time last_used, base::Time last_modified) {
  node_.Data()->last_used = last_used.ToInternalValue();
  node_.Data()->last_modified = last_modified.ToInternalValue();
  node_.set_modified();
}

void EntryImpl::ReportIOTime(Operation op, const base::TimeTicks& start) {
  int group = backend_->GetSizeGroup();
  switch (op) {
    case kRead:
      CACHE_UMA(AGE_MS, "ReadTime", group, start);
      break;
    case kWrite:
      CACHE_UMA(AGE_MS, "WriteTime", group, start);
      break;
    case kSparseRead:
      CACHE_UMA(AGE_MS, "SparseReadTime", 0, start);
      break;
    case kSparseWrite:
      CACHE_UMA(AGE_MS, "SparseWriteTime", 0, start);
      break;
    case kAsyncIO:
      CACHE_UMA(AGE_MS, "AsyncIOTime", group, start);
      break;
    default:
      NOTREACHED();
  }
}

void EntryImpl::BeginLogging(net::NetLog* net_log, bool created) {
  DCHECK(!net_log_.net_log());
  net_log_ = net::BoundNetLog::Make(
      net_log, net::NetLog::SOURCE_DISK_CACHE_ENTRY);
  net_log_.BeginEvent(
      net::NetLog::TYPE_DISK_CACHE_ENTRY,
      make_scoped_refptr(new EntryCreationParameters(GetKey(), created)));
}

const net::BoundNetLog& EntryImpl::net_log() const {
  return net_log_;
}

void EntryImpl::Doom() {
  backend_->background_queue()->DoomEntryImpl(this);
}

void EntryImpl::Close() {
  backend_->background_queue()->CloseEntryImpl(this);
}

std::string EntryImpl::GetKey() const {
  CacheEntryBlock* entry = const_cast<CacheEntryBlock*>(&entry_);
  if (entry->Data()->key_len <= kMaxInternalKeyLength)
    return std::string(entry->Data()->key);

  // We keep a copy of the key so that we can always return it, even if the
  // backend is disabled.
  if (!key_.empty())
    return key_;

  Addr address(entry->Data()->long_key);
  DCHECK(address.is_initialized());
  size_t offset = 0;
  if (address.is_block_file())
    offset = address.start_block() * address.BlockSize() + kBlockHeaderSize;

  COMPILE_ASSERT(kNumStreams == kKeyFileIndex, invalid_key_index);
  File* key_file = const_cast<EntryImpl*>(this)->GetBackingFile(address,
                                                                kKeyFileIndex);

  if (!key_file ||
      !key_file->Read(WriteInto(&key_, entry->Data()->key_len + 1),
                      entry->Data()->key_len + 1, offset))
    key_.clear();
  return key_;
}

Time EntryImpl::GetLastUsed() const {
  CacheRankingsBlock* node = const_cast<CacheRankingsBlock*>(&node_);
  return Time::FromInternalValue(node->Data()->last_used);
}

Time EntryImpl::GetLastModified() const {
  CacheRankingsBlock* node = const_cast<CacheRankingsBlock*>(&node_);
  return Time::FromInternalValue(node->Data()->last_modified);
}

int32 EntryImpl::GetDataSize(int index) const {
  if (index < 0 || index >= kNumStreams)
    return 0;

  CacheEntryBlock* entry = const_cast<CacheEntryBlock*>(&entry_);
  return entry->Data()->data_size[index];
}

int EntryImpl::ReadData(int index, int offset, net::IOBuffer* buf, int buf_len,
                        net::CompletionCallback* callback) {
  if (!callback)
    return ReadDataImpl(index, offset, buf, buf_len, callback);

  DCHECK(node_.Data()->dirty || read_only_);
  if (index < 0 || index >= kNumStreams)
    return net::ERR_INVALID_ARGUMENT;

  int entry_size = entry_.Data()->data_size[index];
  if (offset >= entry_size || offset < 0 || !buf_len)
    return 0;

  if (buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;

  backend_->background_queue()->ReadData(this, index, offset, buf, buf_len,
                                         callback);
  return net::ERR_IO_PENDING;
}

int EntryImpl::WriteData(int index, int offset, net::IOBuffer* buf, int buf_len,
                         CompletionCallback* callback, bool truncate) {
  if (!callback)
    return WriteDataImpl(index, offset, buf, buf_len, callback, truncate);

  DCHECK(node_.Data()->dirty || read_only_);
  if (index < 0 || index >= kNumStreams)
    return net::ERR_INVALID_ARGUMENT;

  if (offset < 0 || buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;

  backend_->background_queue()->WriteData(this, index, offset, buf, buf_len,
                                          truncate, callback);
  return net::ERR_IO_PENDING;
}

int EntryImpl::ReadSparseData(int64 offset, net::IOBuffer* buf, int buf_len,
                              net::CompletionCallback* callback) {
  if (!callback)
    return ReadSparseDataImpl(offset, buf, buf_len, callback);

  backend_->background_queue()->ReadSparseData(this, offset, buf, buf_len,
                                               callback);
  return net::ERR_IO_PENDING;
}

int EntryImpl::WriteSparseData(int64 offset, net::IOBuffer* buf, int buf_len,
                               net::CompletionCallback* callback) {
  if (!callback)
    return WriteSparseDataImpl(offset, buf, buf_len, callback);

  backend_->background_queue()->WriteSparseData(this, offset, buf, buf_len,
                                                callback);
  return net::ERR_IO_PENDING;
}

int EntryImpl::GetAvailableRange(int64 offset, int len, int64* start,
                                 CompletionCallback* callback) {
  backend_->background_queue()->GetAvailableRange(this, offset, len, start,
                                                  callback);
  return net::ERR_IO_PENDING;
}

bool EntryImpl::CouldBeSparse() const {
  if (sparse_.get())
    return true;

  scoped_ptr<SparseControl> sparse;
  sparse.reset(new SparseControl(const_cast<EntryImpl*>(this)));
  return sparse->CouldBeSparse();
}

void EntryImpl::CancelSparseIO() {
  backend_->background_queue()->CancelSparseIO(this);
}

int EntryImpl::ReadyForSparseIO(net::CompletionCallback* callback) {
  if (!sparse_.get())
    return net::OK;

  backend_->background_queue()->ReadyForSparseIO(this, callback);
  return net::ERR_IO_PENDING;
}

// When an entry is deleted from the cache, we clean up all the data associated
// with it for two reasons: to simplify the reuse of the block (we know that any
// unused block is filled with zeros), and to simplify the handling of write /
// read partial information from an entry (don't have to worry about returning
// data related to a previous cache entry because the range was not fully
// written before).
EntryImpl::~EntryImpl() {
  Log("~EntryImpl in");

  // Save the sparse info to disk. This will generate IO for this entry and
  // maybe for a child entry, so it is important to do it before deleting this
  // entry.
  sparse_.reset();

  // Remove this entry from the list of open entries.
  backend_->OnEntryDestroyBegin(entry_.address());

  if (doomed_) {
    DeleteEntryData(true);
  } else {
    net_log_.AddEvent(net::NetLog::TYPE_DISK_CACHE_CLOSE, NULL);
    bool ret = true;
    for (int index = 0; index < kNumStreams; index++) {
      if (user_buffers_[index].get()) {
        if (!(ret = Flush(index, 0)))
          LOG(ERROR) << "Failed to save user data";
      }
      if (unreported_size_[index]) {
        backend_->ModifyStorageSize(
            entry_.Data()->data_size[index] - unreported_size_[index],
            entry_.Data()->data_size[index]);
      }
    }

    if (!ret) {
      // There was a failure writing the actual data. Mark the entry as dirty.
      int current_id = backend_->GetCurrentEntryId();
      node_.Data()->dirty = current_id == 1 ? -1 : current_id - 1;
      node_.Store();
    } else if (node_.HasData() && node_.Data()->dirty) {
      node_.Data()->dirty = 0;
      node_.Store();
    }
  }

  Trace("~EntryImpl out 0x%p", reinterpret_cast<void*>(this));
  net_log_.EndEvent(net::NetLog::TYPE_DISK_CACHE_ENTRY, NULL);
  backend_->OnEntryDestroyEnd();
}

// ------------------------------------------------------------------------

int EntryImpl::InternalReadData(int index, int offset, net::IOBuffer* buf,
                                int buf_len, CompletionCallback* callback) {
  DCHECK(node_.Data()->dirty || read_only_);
  DVLOG(2) << "Read from " << index << " at " << offset << " : " << buf_len;
  if (index < 0 || index >= kNumStreams)
    return net::ERR_INVALID_ARGUMENT;

  int entry_size = entry_.Data()->data_size[index];
  if (offset >= entry_size || offset < 0 || !buf_len)
    return 0;

  if (buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;

  TimeTicks start = TimeTicks::Now();

  if (offset + buf_len > entry_size)
    buf_len = entry_size - offset;

  UpdateRank(false);

  backend_->OnEvent(Stats::READ_DATA);
  backend_->OnRead(buf_len);

  Addr address(entry_.Data()->data_addr[index]);
  int eof = address.is_initialized() ? entry_size : 0;
  if (user_buffers_[index].get() &&
      user_buffers_[index]->PreRead(eof, offset, &buf_len)) {
    // Complete the operation locally.
    buf_len = user_buffers_[index]->Read(offset, buf, buf_len);
    ReportIOTime(kRead, start);
    return buf_len;
  }

  address.set_value(entry_.Data()->data_addr[index]);
  DCHECK(address.is_initialized());
  if (!address.is_initialized())
    return net::ERR_FAILED;

  File* file = GetBackingFile(address, index);
  if (!file)
    return net::ERR_FAILED;

  size_t file_offset = offset;
  if (address.is_block_file()) {
    DCHECK_LE(offset + buf_len, kMaxBlockSize);
    file_offset += address.start_block() * address.BlockSize() +
                   kBlockHeaderSize;
  }

  SyncCallback* io_callback = NULL;
  if (callback) {
    io_callback = new SyncCallback(this, buf, callback,
                                   net::NetLog::TYPE_DISK_CACHE_READ_DATA);
  }

  bool completed;
  if (!file->Read(buf->data(), buf_len, file_offset, io_callback, &completed)) {
    if (io_callback)
      io_callback->Discard();
    return net::ERR_FAILED;
  }

  if (io_callback && completed)
    io_callback->Discard();

  ReportIOTime(kRead, start);
  return (completed || !callback) ? buf_len : net::ERR_IO_PENDING;
}

int EntryImpl::InternalWriteData(int index, int offset, net::IOBuffer* buf,
                                 int buf_len, CompletionCallback* callback,
                                 bool truncate) {
  DCHECK(node_.Data()->dirty || read_only_);
  DVLOG(2) << "Write to " << index << " at " << offset << " : " << buf_len;
  if (index < 0 || index >= kNumStreams)
    return net::ERR_INVALID_ARGUMENT;

  if (offset < 0 || buf_len < 0)
    return net::ERR_INVALID_ARGUMENT;

  int max_file_size = backend_->MaxFileSize();

  // offset or buf_len could be negative numbers.
  if (offset > max_file_size || buf_len > max_file_size ||
      offset + buf_len > max_file_size) {
    int size = offset + buf_len;
    if (size <= max_file_size)
      size = kint32max;
    backend_->TooMuchStorageRequested(size);
    return net::ERR_FAILED;
  }

  TimeTicks start = TimeTicks::Now();

  // Read the size at this point (it may change inside prepare).
  int entry_size = entry_.Data()->data_size[index];
  bool extending = entry_size < offset + buf_len;
  truncate = truncate && entry_size > offset + buf_len;
  Trace("To PrepareTarget 0x%x", entry_.address().value());
  if (!PrepareTarget(index, offset, buf_len, truncate))
    return net::ERR_FAILED;

  Trace("From PrepareTarget 0x%x", entry_.address().value());
  if (extending || truncate)
    UpdateSize(index, entry_size, offset + buf_len);

  UpdateRank(true);

  backend_->OnEvent(Stats::WRITE_DATA);
  backend_->OnWrite(buf_len);

  if (user_buffers_[index].get()) {
    // Complete the operation locally.
    user_buffers_[index]->Write(offset, buf, buf_len);
    ReportIOTime(kWrite, start);
    return buf_len;
  }

  Addr address(entry_.Data()->data_addr[index]);
  if (offset + buf_len == 0) {
    if (truncate) {
      DCHECK(!address.is_initialized());
    }
    return 0;
  }

  File* file = GetBackingFile(address, index);
  if (!file)
    return net::ERR_FAILED;

  size_t file_offset = offset;
  if (address.is_block_file()) {
    DCHECK_LE(offset + buf_len, kMaxBlockSize);
    file_offset += address.start_block() * address.BlockSize() +
                   kBlockHeaderSize;
  } else if (truncate || (extending && !buf_len)) {
    if (!file->SetLength(offset + buf_len))
      return net::ERR_FAILED;
  }

  if (!buf_len)
    return 0;

  SyncCallback* io_callback = NULL;
  if (callback) {
    io_callback = new SyncCallback(this, buf, callback,
                                   net::NetLog::TYPE_DISK_CACHE_WRITE_DATA);
  }

  bool completed;
  if (!file->Write(buf->data(), buf_len, file_offset, io_callback,
                   &completed)) {
    if (io_callback)
      io_callback->Discard();
    return net::ERR_FAILED;
  }

  if (io_callback && completed)
    io_callback->Discard();

  ReportIOTime(kWrite, start);
  return (completed || !callback) ? buf_len : net::ERR_IO_PENDING;
}

// ------------------------------------------------------------------------

bool EntryImpl::CreateDataBlock(int index, int size) {
  DCHECK(index >= 0 && index < kNumStreams);

  Addr address(entry_.Data()->data_addr[index]);
  if (!CreateBlock(size, &address))
    return false;

  entry_.Data()->data_addr[index] = address.value();
  entry_.Store();
  return true;
}

bool EntryImpl::CreateBlock(int size, Addr* address) {
  DCHECK(!address->is_initialized());

  FileType file_type = Addr::RequiredFileType(size);
  if (EXTERNAL == file_type) {
    if (size > backend_->MaxFileSize())
      return false;
    if (!backend_->CreateExternalFile(address))
      return false;
  } else {
    int num_blocks = (size + Addr::BlockSizeForFileType(file_type) - 1) /
                     Addr::BlockSizeForFileType(file_type);

    if (!backend_->CreateBlock(file_type, num_blocks, address))
      return false;
  }
  return true;
}

// Note that this method may end up modifying a block file so upon return the
// involved block will be free, and could be reused for something else. If there
// is a crash after that point (and maybe before returning to the caller), the
// entry will be left dirty... and at some point it will be discarded; it is
// important that the entry doesn't keep a reference to this address, or we'll
// end up deleting the contents of |address| once again.
void EntryImpl::DeleteData(Addr address, int index) {
  if (!address.is_initialized())
    return;
  if (address.is_separate_file()) {
    int failure = !DeleteCacheFile(backend_->GetFileName(address));
    CACHE_UMA(COUNTS, "DeleteFailed", 0, failure);
    if (failure) {
      LOG(ERROR) << "Failed to delete " <<
          backend_->GetFileName(address).value() << " from the cache.";
    }
    if (files_[index])
      files_[index] = NULL;  // Releases the object.
  } else {
    backend_->DeleteBlock(address, true);
  }
}

void EntryImpl::UpdateRank(bool modified) {
  if (!doomed_) {
    // Everything is handled by the backend.
    backend_->UpdateRank(this, modified);
    return;
  }

  Time current = Time::Now();
  node_.Data()->last_used = current.ToInternalValue();

  if (modified)
    node_.Data()->last_modified = current.ToInternalValue();
}

File* EntryImpl::GetBackingFile(Addr address, int index) {
  File* file;
  if (address.is_separate_file())
    file = GetExternalFile(address, index);
  else
    file = backend_->File(address);
  return file;
}

File* EntryImpl::GetExternalFile(Addr address, int index) {
  DCHECK(index >= 0 && index <= kKeyFileIndex);
  if (!files_[index].get()) {
    // For a key file, use mixed mode IO.
    scoped_refptr<File> file(new File(kKeyFileIndex == index));
    if (file->Init(backend_->GetFileName(address)))
      files_[index].swap(file);
  }
  return files_[index].get();
}

// We keep a memory buffer for everything that ends up stored on a block file
// (because we don't know yet the final data size), and for some of the data
// that end up on external files. This function will initialize that memory
// buffer and / or the files needed to store the data.
//
// In general, a buffer may overlap data already stored on disk, and in that
// case, the contents of the buffer are the most accurate. It may also extend
// the file, but we don't want to read from disk just to keep the buffer up to
// date. This means that as soon as there is a chance to get confused about what
// is the most recent version of some part of a file, we'll flush the buffer and
// reuse it for the new data. Keep in mind that the normal use pattern is quite
// simple (write sequentially from the beginning), so we optimize for handling
// that case.
bool EntryImpl::PrepareTarget(int index, int offset, int buf_len,
                              bool truncate) {
  if (truncate)
    return HandleTruncation(index, offset, buf_len);

  if (!offset && !buf_len)
    return true;

  Addr address(entry_.Data()->data_addr[index]);
  if (address.is_initialized()) {
    if (address.is_block_file() && !MoveToLocalBuffer(index))
      return false;

    if (!user_buffers_[index].get() && offset < kMaxBlockSize) {
      // We are about to create a buffer for the first 16KB, make sure that we
      // preserve existing data.
      if (!CopyToLocalBuffer(index))
        return false;
    }
  }

  if (!user_buffers_[index].get())
    user_buffers_[index].reset(new UserBuffer(backend_));

  return PrepareBuffer(index, offset, buf_len);
}

// We get to this function with some data already stored. If there is a
// truncation that results on data stored internally, we'll explicitly
// handle the case here.
bool EntryImpl::HandleTruncation(int index, int offset, int buf_len) {
  Addr address(entry_.Data()->data_addr[index]);

  int current_size = entry_.Data()->data_size[index];
  int new_size = offset + buf_len;

  if (!new_size) {
    // This is by far the most common scenario.
    backend_->ModifyStorageSize(current_size - unreported_size_[index], 0);
    entry_.Data()->data_addr[index] = 0;
    entry_.Data()->data_size[index] = 0;
    unreported_size_[index] = 0;
    entry_.Store();
    DeleteData(address, index);

    user_buffers_[index].reset();
    return true;
  }

  // We never postpone truncating a file, if there is one, but we may postpone
  // telling the backend about the size reduction.
  if (user_buffers_[index].get()) {
    DCHECK_GE(current_size, user_buffers_[index]->Start());
    if (!address.is_initialized()) {
      // There is no overlap between the buffer and disk.
      if (new_size > user_buffers_[index]->Start()) {
        // Just truncate our buffer.
        DCHECK_LT(new_size, user_buffers_[index]->End());
        user_buffers_[index]->Truncate(new_size);
        return true;
      }

      // Just discard our buffer.
      user_buffers_[index]->Reset();
      return PrepareBuffer(index, offset, buf_len);
    }

    // There is some overlap or we need to extend the file before the
    // truncation.
    if (offset > user_buffers_[index]->Start())
      user_buffers_[index]->Truncate(new_size);
    UpdateSize(index, current_size, new_size);
    if (!Flush(index, 0))
      return false;
    user_buffers_[index].reset();
  }

  // We have data somewhere, and it is not in a buffer.
  DCHECK(!user_buffers_[index].get());
  DCHECK(address.is_initialized());

  if (new_size > kMaxBlockSize)
    return true;  // Let the operation go directly to disk.

  return ImportSeparateFile(index, offset + buf_len);
}

bool EntryImpl::CopyToLocalBuffer(int index) {
  Addr address(entry_.Data()->data_addr[index]);
  DCHECK(!user_buffers_[index].get());
  DCHECK(address.is_initialized());

  int len = std::min(entry_.Data()->data_size[index], kMaxBlockSize);
  user_buffers_[index].reset(new UserBuffer(backend_));
  user_buffers_[index]->Write(len, NULL, 0);

  File* file = GetBackingFile(address, index);
  int offset = 0;

  if (address.is_block_file())
    offset = address.start_block() * address.BlockSize() + kBlockHeaderSize;

  if (!file ||
      !file->Read(user_buffers_[index]->Data(), len, offset, NULL, NULL)) {
    user_buffers_[index].reset();
    return false;
  }
  return true;
}

bool EntryImpl::MoveToLocalBuffer(int index) {
  if (!CopyToLocalBuffer(index))
    return false;

  Addr address(entry_.Data()->data_addr[index]);
  entry_.Data()->data_addr[index] = 0;
  entry_.Store();
  DeleteData(address, index);

  // If we lose this entry we'll see it as zero sized.
  int len = entry_.Data()->data_size[index];
  backend_->ModifyStorageSize(len - unreported_size_[index], 0);
  unreported_size_[index] = len;
  return true;
}

bool EntryImpl::ImportSeparateFile(int index, int new_size) {
  if (entry_.Data()->data_size[index] > new_size)
    UpdateSize(index, entry_.Data()->data_size[index], new_size);

  return MoveToLocalBuffer(index);
}

bool EntryImpl::PrepareBuffer(int index, int offset, int buf_len) {
  DCHECK(user_buffers_[index].get());
  if ((user_buffers_[index]->End() && offset > user_buffers_[index]->End()) ||
      offset > entry_.Data()->data_size[index]) {
    // We are about to extend the buffer or the file (with zeros), so make sure
    // that we are not overwriting anything.
    Addr address(entry_.Data()->data_addr[index]);
    if (address.is_initialized() && address.is_separate_file()) {
      if (!Flush(index, 0))
        return false;
      // There is an actual file already, and we don't want to keep track of
      // its length so we let this operation go straight to disk.
      // The only case when a buffer is allowed to extend the file (as in fill
      // with zeros before the start) is when there is no file yet to extend.
      user_buffers_[index].reset();
      return true;
    }
  }

  if (!user_buffers_[index]->PreWrite(offset, buf_len)) {
    if (!Flush(index, offset + buf_len))
      return false;

    // Lets try again.
    if (offset > user_buffers_[index]->End() ||
        !user_buffers_[index]->PreWrite(offset, buf_len)) {
      // We cannot complete the operation with a buffer.
      DCHECK(!user_buffers_[index]->Size());
      DCHECK(!user_buffers_[index]->Start());
      user_buffers_[index].reset();
    }
  }
  return true;
}

bool EntryImpl::Flush(int index, int min_len) {
  Addr address(entry_.Data()->data_addr[index]);
  DCHECK(user_buffers_[index].get());
  DCHECK(!address.is_initialized() || address.is_separate_file());
  DVLOG(3) << "Flush";

  int size = std::max(entry_.Data()->data_size[index], min_len);
  if (size && !address.is_initialized() && !CreateDataBlock(index, size))
    return false;

  if (!entry_.Data()->data_size[index]) {
    DCHECK(!user_buffers_[index]->Size());
    return true;
  }

  address.set_value(entry_.Data()->data_addr[index]);

  int len = user_buffers_[index]->Size();
  int offset = user_buffers_[index]->Start();
  if (!len && !offset)
    return true;

  if (address.is_block_file()) {
    DCHECK_EQ(len, entry_.Data()->data_size[index]);
    DCHECK(!offset);
    offset = address.start_block() * address.BlockSize() + kBlockHeaderSize;
  }

  File* file = GetBackingFile(address, index);
  if (!file)
    return false;

  if (!file->Write(user_buffers_[index]->Data(), len, offset, NULL, NULL))
    return false;
  user_buffers_[index]->Reset();

  return true;
}

void EntryImpl::UpdateSize(int index, int old_size, int new_size) {
  if (entry_.Data()->data_size[index] == new_size)
    return;

  unreported_size_[index] += new_size - old_size;
  entry_.Data()->data_size[index] = new_size;
  entry_.set_modified();
}

int EntryImpl::InitSparseData() {
  if (sparse_.get())
    return net::OK;

  // Use a local variable so that sparse_ never goes from 'valid' to NULL.
  scoped_ptr<SparseControl> sparse(new SparseControl(this));
  int result = sparse->Init();
  if (net::OK == result)
    sparse_.swap(sparse);

  return result;
}

void EntryImpl::SetEntryFlags(uint32 flags) {
  entry_.Data()->flags |= flags;
  entry_.set_modified();
}

uint32 EntryImpl::GetEntryFlags() {
  return entry_.Data()->flags;
}

void EntryImpl::GetData(int index, char** buffer, Addr* address) {
  if (user_buffers_[index].get() && user_buffers_[index]->Size() &&
      !user_buffers_[index]->Start()) {
    // The data is already in memory, just copy it and we're done.
    int data_len = entry_.Data()->data_size[index];
    if (data_len <= user_buffers_[index]->Size()) {
      DCHECK(!user_buffers_[index]->Start());
      *buffer = new char[data_len];
      memcpy(*buffer, user_buffers_[index]->Data(), data_len);
      return;
    }
  }

  // Bad news: we'd have to read the info from disk so instead we'll just tell
  // the caller where to read from.
  *buffer = NULL;
  address->set_value(entry_.Data()->data_addr[index]);
  if (address->is_initialized()) {
    // Prevent us from deleting the block from the backing store.
    backend_->ModifyStorageSize(entry_.Data()->data_size[index] -
                                    unreported_size_[index], 0);
    entry_.Data()->data_addr[index] = 0;
    entry_.Data()->data_size[index] = 0;
  }
}

void EntryImpl::Log(const char* msg) {
  int dirty = 0;
  if (node_.HasData()) {
    dirty = node_.Data()->dirty;
  }

  Trace("%s 0x%p 0x%x 0x%x", msg, reinterpret_cast<void*>(this),
        entry_.address().value(), node_.address().value());

  Trace("  data: 0x%x 0x%x 0x%x", entry_.Data()->data_addr[0],
        entry_.Data()->data_addr[1], entry_.Data()->long_key);

  Trace("  doomed: %d 0x%x", doomed_, dirty);
}

}  // namespace disk_cache
