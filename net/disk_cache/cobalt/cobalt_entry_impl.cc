// Copyright 2022 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "net/disk_cache/cobalt/cobalt_entry_impl.h"

using base::Time;

namespace disk_cache {

CobaltEntryImpl::CobaltEntryImpl(base::WeakPtr<CobaltBackendImpl> backend,
                                 const std::string& key,
                                 net::NetLog* net_log)
    : CobaltEntryImpl(backend,
                      key,
                      0,        // child_id
                      nullptr,  // parent
                      net_log) {}

CobaltEntryImpl::CobaltEntryImpl(base::WeakPtr<CobaltBackendImpl> backend,
                                 int child_id,
                                 CobaltEntryImpl* parent,
                                 net::NetLog* net_log)
    : CobaltEntryImpl(backend,
                      std::string(),  // key
                      child_id,
                      parent,
                      net_log) {
  (*parent_->children_)[child_id] = this;
}

CobaltEntryImpl::CobaltEntryImpl(base::WeakPtr<CobaltBackendImpl> backend,
                                 const ::std::string& key,
                                 int child_id,
                                 CobaltEntryImpl* parent,
                                 net::NetLog* net_log)
    : key_(key),
      parent_(parent),
      last_modified_(Time::Now()),
      last_used_(last_modified_) {}

void CobaltEntryImpl::Doom() {
  // TODO: Implement
}

void CobaltEntryImpl::Close() {
  // TODO: Implement
}

std::string CobaltEntryImpl::GetKey() const {
  // A child entry doesn't have key so this method should not be called.
  DCHECK_EQ(PARENT_ENTRY, type());
  return key_;
}

Time CobaltEntryImpl::GetLastUsed() const {
  return last_used_;
}

Time CobaltEntryImpl::GetLastModified() const {
  return last_modified_;
}

int32_t CobaltEntryImpl::GetDataSize(int index) const {
  // TODO: Implement
  return 0;
}

int CobaltEntryImpl::ReadData(int index,
                              int offset,
                              IOBuffer* buf,
                              int buf_len,
                              CompletionOnceCallback callback) {
  // TODO: Implement
  return net::ERR_FAILED;
}

int CobaltEntryImpl::WriteData(int index,
                               int offset,
                               IOBuffer* buf,
                               int buf_len,
                               CompletionOnceCallback callback,
                               bool truncate) {
  // TODO: Implement
  return net::ERR_FAILED;
}

int CobaltEntryImpl::ReadSparseData(int64_t offset,
                                    IOBuffer* buf,
                                    int buf_len,
                                    CompletionOnceCallback callback) {
  // TODO: Implement
  return net::ERR_FAILED;
}

int CobaltEntryImpl::WriteSparseData(int64_t offset,
                                     IOBuffer* buf,
                                     int buf_len,
                                     CompletionOnceCallback callback) {
  // TODO: Implement
  return net::ERR_FAILED;
}

int CobaltEntryImpl::GetAvailableRange(int64_t offset,
                                       int len,
                                       int64_t* start,
                                       CompletionOnceCallback callback) {
  // TODO: Implement
  return 0;
}

bool CobaltEntryImpl::CouldBeSparse() const {
  DCHECK_EQ(PARENT_ENTRY, type());
  return (children_.get() != nullptr);
}

net::Error CobaltEntryImpl::ReadyForSparseIO(CompletionOnceCallback callback) {
  return net::OK;
}

void CobaltEntryImpl::SetLastUsedTimeForTest(base::Time time) {
  last_used_ = time;
}
}  // namespace disk_cache
