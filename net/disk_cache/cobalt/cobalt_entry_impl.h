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

#ifndef NET_DISK_CACHE_COBALT_COBALT_ENTRY_IMPL_H_
#define NET_DISK_CACHE_COBALT_COBALT_ENTRY_IMPL_H_

#include "base/trace_event/memory_usage_estimator.h"
#include "net/disk_cache/disk_cache.h"

namespace disk_cache {

class CobaltBackendImpl;

class NET_EXPORT_PRIVATE CobaltEntryImpl final
    : public Entry,
      public base::LinkNode<CobaltEntryImpl> {
 public:
  enum EntryType {
    PARENT_ENTRY,
    CHILD_ENTRY,
  };

  // Constructor for parent entries.
  CobaltEntryImpl(base::WeakPtr<CobaltBackendImpl> backend,
                  const std::string& key,
                  net::NetLog* net_log);

  // Constructor for child entries.
  CobaltEntryImpl(base::WeakPtr<CobaltBackendImpl> backend,
                  int child_id,
                  CobaltEntryImpl* parent,
                  net::NetLog* net_log);

  EntryType type() const { return parent_ ? CHILD_ENTRY : PARENT_ENTRY; }

  // From disk_cache::Entry:
  void Doom() override;
  void Close() override;
  std::string GetKey() const override;
  base::Time GetLastUsed() const override;
  base::Time GetLastModified() const override;
  int32_t GetDataSize(int index) const override;
  int ReadData(int index,
               int offset,
               IOBuffer* buf,
               int buf_len,
               CompletionOnceCallback callback) override;
  int WriteData(int index,
                int offset,
                IOBuffer* buf,
                int buf_len,
                CompletionOnceCallback callback,
                bool truncate) override;
  int ReadSparseData(int64_t offset,
                     IOBuffer* buf,
                     int buf_len,
                     CompletionOnceCallback callback) override;
  int WriteSparseData(int64_t offset,
                      IOBuffer* buf,
                      int buf_len,
                      CompletionOnceCallback callback) override;
  int GetAvailableRange(int64_t offset,
                        int len,
                        int64_t* start,
                        CompletionOnceCallback callback) override;
  bool CouldBeSparse() const override;
  void CancelSparseIO() override {}
  net::Error ReadyForSparseIO(CompletionOnceCallback callback) override;
  void SetLastUsedTimeForTest(base::Time time) override;

 private:
  CobaltEntryImpl(base::WeakPtr<CobaltBackendImpl> backend,
                  const std::string& key,
                  int child_id,
                  CobaltEntryImpl* parent,
                  net::NetLog* net_log);

  using EntryMap = std::map<int, CobaltEntryImpl*>;

  std::string key_;

  // Pointer to the parent entry, or nullptr if this entry is a parent entry.
  CobaltEntryImpl* parent_ = nullptr;
  std::unique_ptr<EntryMap> children_ = nullptr;

  base::Time last_modified_;
  base::Time last_used_;

  DISALLOW_COPY_AND_ASSIGN(CobaltEntryImpl);
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_COBALT_COBALT_ENTRY_IMPL_H_
