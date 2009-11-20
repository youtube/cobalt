// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_cache.h"

#include <algorithm>

#include "base/compiler_specific.h"

#if defined(OS_POSIX)
#include <unistd.h>
#endif

#include "base/format_macros.h"
#include "base/message_loop.h"
#include "base/pickle.h"
#include "base/ref_counted.h"
#include "base/string_util.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache_transaction.h"
#include "net/http/http_network_layer.h"
#include "net/http/http_network_session.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"

namespace net {

// disk cache entry data indices.
enum {
  kResponseInfoIndex,
  kResponseContentIndex
};

//-----------------------------------------------------------------------------

HttpCache::ActiveEntry::ActiveEntry(disk_cache::Entry* e)
    : disk_entry(e),
      writer(NULL),
      will_process_pending_queue(false),
      doomed(false) {
}

HttpCache::ActiveEntry::~ActiveEntry() {
  if (disk_entry)
    disk_entry->Close();
}

//-----------------------------------------------------------------------------

HttpCache::HttpCache(HostResolver* host_resolver,
                     ProxyService* proxy_service,
                     SSLConfigService* ssl_config_service,
                     const FilePath& cache_dir,
                     int cache_size)
    : disk_cache_dir_(cache_dir),
      mode_(NORMAL),
      type_(DISK_CACHE),
      network_layer_(HttpNetworkLayer::CreateFactory(
          host_resolver, proxy_service, ssl_config_service)),
      ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)),
      enable_range_support_(true),
      cache_size_(cache_size) {
}

HttpCache::HttpCache(HttpNetworkSession* session,
                     const FilePath& cache_dir,
                     int cache_size)
    : disk_cache_dir_(cache_dir),
      mode_(NORMAL),
      type_(DISK_CACHE),
      network_layer_(HttpNetworkLayer::CreateFactory(session)),
      ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)),
      enable_range_support_(true),
      cache_size_(cache_size) {
}

HttpCache::HttpCache(HostResolver* host_resolver,
                     ProxyService* proxy_service,
                     SSLConfigService* ssl_config_service,
                     int cache_size)
    : mode_(NORMAL),
      type_(MEMORY_CACHE),
      network_layer_(HttpNetworkLayer::CreateFactory(
          host_resolver, proxy_service, ssl_config_service)),
      ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)),
      enable_range_support_(true),
      cache_size_(cache_size) {
}

HttpCache::HttpCache(HttpTransactionFactory* network_layer,
                     disk_cache::Backend* disk_cache)
    : mode_(NORMAL),
      type_(DISK_CACHE),
      network_layer_(network_layer),
      disk_cache_(disk_cache),
      ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)),
      enable_range_support_(true),
      cache_size_(0) {
}

HttpCache::~HttpCache() {
  // If we have any active entries remaining, then we need to deactivate them.
  // We may have some pending calls to OnProcessPendingQueue, but since those
  // won't run (due to our destruction), we can simply ignore the corresponding
  // will_process_pending_queue flag.
  while (!active_entries_.empty()) {
    ActiveEntry* entry = active_entries_.begin()->second;
    entry->will_process_pending_queue = false;
    entry->pending_queue.clear();
    entry->readers.clear();
    entry->writer = NULL;
    DeactivateEntry(entry);
  }

  ActiveEntriesSet::iterator it = doomed_entries_.begin();
  for (; it != doomed_entries_.end(); ++it)
    delete *it;
}

disk_cache::Backend* HttpCache::GetBackend() {
  if (disk_cache_.get())
    return disk_cache_.get();

  DCHECK_GE(cache_size_, 0);
  if (type_ == MEMORY_CACHE) {
    // We may end up with no folder name and no cache if the initialization
    // of the disk cache fails. We want to be sure that what we wanted to have
    // was an in-memory cache.
    disk_cache_.reset(disk_cache::CreateInMemoryCacheBackend(cache_size_));
  } else if (!disk_cache_dir_.empty()) {
    disk_cache_.reset(disk_cache::CreateCacheBackend(disk_cache_dir_, true,
        cache_size_, type_));
    disk_cache_dir_ = FilePath();  // Reclaim memory.
  }
  return disk_cache_.get();
}

int HttpCache::CreateTransaction(scoped_ptr<HttpTransaction>* trans) {
  // Do lazy initialization of disk cache if needed.
  GetBackend();
  trans->reset(new HttpCache::Transaction(this, enable_range_support_));
  return OK;
}

HttpCache* HttpCache::GetCache() {
  return this;
}

HttpNetworkSession* HttpCache::GetSession() {
  net::HttpNetworkLayer* network =
      static_cast<net::HttpNetworkLayer*>(network_layer_.get());
  return network->GetSession();
}

void HttpCache::Suspend(bool suspend) {
  network_layer_->Suspend(suspend);
}

// static
bool HttpCache::ParseResponseInfo(const char* data, int len,
                                  HttpResponseInfo* response_info,
                                  bool* response_truncated) {
  Pickle pickle(data, len);
  return response_info->InitFromPickle(pickle, response_truncated);
}

// static
bool HttpCache::ReadResponseInfo(disk_cache::Entry* disk_entry,
                                 HttpResponseInfo* response_info,
                                 bool* response_truncated) {
  int size = disk_entry->GetDataSize(kResponseInfoIndex);

  scoped_refptr<IOBuffer> buffer = new IOBuffer(size);
  int rv = disk_entry->ReadData(kResponseInfoIndex, 0, buffer, size, NULL);
  if (rv != size) {
    DLOG(ERROR) << "ReadData failed: " << rv;
    return false;
  }

  return ParseResponseInfo(buffer->data(), size, response_info,
                           response_truncated);
}

// static
bool HttpCache::WriteResponseInfo(disk_cache::Entry* disk_entry,
                                  const HttpResponseInfo* response_info,
                                  bool skip_transient_headers,
                                  bool response_truncated) {
  Pickle pickle;
  response_info->Persist(
      &pickle, skip_transient_headers, response_truncated);

  scoped_refptr<WrappedIOBuffer> data = new WrappedIOBuffer(
      reinterpret_cast<const char*>(pickle.data()));
  int len = static_cast<int>(pickle.size());

  return disk_entry->WriteData(kResponseInfoIndex, 0, data, len, NULL,
                               true) == len;
}

// Generate a key that can be used inside the cache.
std::string HttpCache::GenerateCacheKey(const HttpRequestInfo* request) {
  // Strip out the reference, username, and password sections of the URL.
  std::string url = HttpUtil::SpecForRequest(request->url);

  DCHECK(mode_ != DISABLE);
  if (mode_ == NORMAL) {
    // No valid URL can begin with numerals, so we should not have to worry
    // about collisions with normal URLs.
    if (request->upload_data && request->upload_data->identifier()) {
      url.insert(0, StringPrintf("%" PRId64 "/",
                                 request->upload_data->identifier()));
    }
    return url;
  }

  // In playback and record mode, we cache everything.

  // Lazily initialize.
  if (playback_cache_map_ == NULL)
    playback_cache_map_.reset(new PlaybackCacheMap());

  // Each time we request an item from the cache, we tag it with a
  // generation number.  During playback, multiple fetches for the same
  // item will use the same generation number and pull the proper
  // instance of an URL from the cache.
  int generation = 0;
  DCHECK(playback_cache_map_ != NULL);
  if (playback_cache_map_->find(url) != playback_cache_map_->end())
    generation = (*playback_cache_map_)[url];
  (*playback_cache_map_)[url] = generation + 1;

  // The key into the cache is GENERATION # + METHOD + URL.
  std::string result = IntToString(generation);
  result.append(request->method);
  result.append(url);
  return result;
}

void HttpCache::DoomEntry(const std::string& key) {
  // Need to abandon the ActiveEntry, but any transaction attached to the entry
  // should not be impacted.  Dooming an entry only means that it will no
  // longer be returned by FindActiveEntry (and it will also be destroyed once
  // all consumers are finished with the entry).
  ActiveEntriesMap::iterator it = active_entries_.find(key);
  if (it == active_entries_.end()) {
    disk_cache_->DoomEntry(key);
  } else {
    ActiveEntry* entry = it->second;
    active_entries_.erase(it);

    // We keep track of doomed entries so that we can ensure that they are
    // cleaned up properly when the cache is destroyed.
    doomed_entries_.insert(entry);

    entry->disk_entry->Doom();
    entry->doomed = true;

    DCHECK(entry->writer || !entry->readers.empty());
  }
}

void HttpCache::FinalizeDoomedEntry(ActiveEntry* entry) {
  DCHECK(entry->doomed);
  DCHECK(!entry->writer);
  DCHECK(entry->readers.empty());
  DCHECK(entry->pending_queue.empty());

  ActiveEntriesSet::iterator it = doomed_entries_.find(entry);
  DCHECK(it != doomed_entries_.end());
  doomed_entries_.erase(it);

  delete entry;
}

HttpCache::ActiveEntry* HttpCache::FindActiveEntry(const std::string& key) {
  ActiveEntriesMap::const_iterator it = active_entries_.find(key);
  return it != active_entries_.end() ? it->second : NULL;
}

HttpCache::ActiveEntry* HttpCache::OpenEntry(const std::string& key) {
  DCHECK(!FindActiveEntry(key));

  disk_cache::Entry* disk_entry;
  if (!disk_cache_->OpenEntry(key, &disk_entry))
    return NULL;

  return ActivateEntry(key, disk_entry);
}

HttpCache::ActiveEntry* HttpCache::CreateEntry(const std::string& key) {
  DCHECK(!FindActiveEntry(key));

  disk_cache::Entry* disk_entry;
  if (!disk_cache_->CreateEntry(key, &disk_entry))
    return NULL;

  return ActivateEntry(key, disk_entry);
}

void HttpCache::DestroyEntry(ActiveEntry* entry) {
  if (entry->doomed) {
    FinalizeDoomedEntry(entry);
  } else {
    DeactivateEntry(entry);
  }
}

HttpCache::ActiveEntry* HttpCache::ActivateEntry(
    const std::string& key,
    disk_cache::Entry* disk_entry) {
  ActiveEntry* entry = new ActiveEntry(disk_entry);
  active_entries_[key] = entry;
  return entry;
}

void HttpCache::DeactivateEntry(ActiveEntry* entry) {
  DCHECK(!entry->will_process_pending_queue);
  DCHECK(!entry->doomed);
  DCHECK(!entry->writer);
  DCHECK(entry->readers.empty());
  DCHECK(entry->pending_queue.empty());

  std::string key = entry->disk_entry->GetKey();
  if (key.empty())
    return SlowDeactivateEntry(entry);

  ActiveEntriesMap::iterator it = active_entries_.find(key);
  DCHECK(it != active_entries_.end());
  DCHECK(it->second == entry);

  active_entries_.erase(it);
  delete entry;
}

// We don't know this entry's key so we have to find it without it.
void HttpCache::SlowDeactivateEntry(ActiveEntry* entry) {
  for (ActiveEntriesMap::iterator it = active_entries_.begin();
       it != active_entries_.end(); ++it) {
    if (it->second == entry) {
      active_entries_.erase(it);
      delete entry;
      break;
    }
  }
}

int HttpCache::AddTransactionToEntry(ActiveEntry* entry, Transaction* trans) {
  DCHECK(entry);

  // We implement a basic reader/writer lock for the disk cache entry.  If
  // there is already a writer, then everyone has to wait for the writer to
  // finish before they can access the cache entry.  There can be multiple
  // readers.
  //
  // NOTE: If the transaction can only write, then the entry should not be in
  // use (since any existing entry should have already been doomed).

  if (entry->writer || entry->will_process_pending_queue) {
    entry->pending_queue.push_back(trans);
    return ERR_IO_PENDING;
  }

  if (trans->mode() & Transaction::WRITE) {
    // transaction needs exclusive access to the entry
    if (entry->readers.empty()) {
      entry->writer = trans;
    } else {
      entry->pending_queue.push_back(trans);
      return ERR_IO_PENDING;
    }
  } else {
    // transaction needs read access to the entry
    entry->readers.push_back(trans);
  }

  // We do this before calling EntryAvailable to force any further calls to
  // AddTransactionToEntry to add their transaction to the pending queue, which
  // ensures FIFO ordering.
  if (!entry->writer && !entry->pending_queue.empty())
    ProcessPendingQueue(entry);

  return trans->EntryAvailable(entry);
}

void HttpCache::DoneWithEntry(ActiveEntry* entry, Transaction* trans,
                              bool cancel) {
  // If we already posted a task to move on to the next transaction and this was
  // the writer, there is nothing to cancel.
  if (entry->will_process_pending_queue && entry->readers.empty())
    return;

  if (entry->writer) {
    DCHECK(trans == entry->writer);

    // Assume there was a failure.
    bool success = false;
    if (cancel) {
      DCHECK(entry->disk_entry);
      // This is a successful operation in the sense that we want to keep the
      // entry.
      success = trans->AddTruncatedFlag();
    }
    DoneWritingToEntry(entry, success);
  } else {
    DoneReadingFromEntry(entry, trans);
  }
}

void HttpCache::DoneWritingToEntry(ActiveEntry* entry, bool success) {
  DCHECK(entry->readers.empty());

  entry->writer = NULL;

  if (success) {
    ProcessPendingQueue(entry);
  } else {
    DCHECK(!entry->will_process_pending_queue);

    // We failed to create this entry.
    TransactionList pending_queue;
    pending_queue.swap(entry->pending_queue);

    entry->disk_entry->Doom();
    DestroyEntry(entry);

    // We need to do something about these pending entries, which now need to
    // be added to a new entry.
    while (!pending_queue.empty()) {
      pending_queue.front()->AddToEntry();
      pending_queue.pop_front();
    }
  }
}

void HttpCache::DoneReadingFromEntry(ActiveEntry* entry, Transaction* trans) {
  DCHECK(!entry->writer);

  TransactionList::iterator it =
      std::find(entry->readers.begin(), entry->readers.end(), trans);
  DCHECK(it != entry->readers.end());

  entry->readers.erase(it);

  ProcessPendingQueue(entry);
}

void HttpCache::ConvertWriterToReader(ActiveEntry* entry) {
  DCHECK(entry->writer);
  DCHECK(entry->writer->mode() == Transaction::READ_WRITE);
  DCHECK(entry->readers.empty());

  Transaction* trans = entry->writer;

  entry->writer = NULL;
  entry->readers.push_back(trans);

  ProcessPendingQueue(entry);
}

void HttpCache::RemovePendingTransaction(Transaction* trans) {
  ActiveEntriesMap::const_iterator i = active_entries_.find(trans->key());
  bool found = false;
  if (i != active_entries_.end())
    found = RemovePendingTransactionFromEntry(i->second, trans);

  if (found)
    return;

  ActiveEntriesSet::iterator it = doomed_entries_.begin();
  for (; it != doomed_entries_.end() && !found; ++it)
    found = RemovePendingTransactionFromEntry(*it, trans);
}

bool HttpCache::RemovePendingTransactionFromEntry(ActiveEntry* entry,
                                                  Transaction* trans) {
  TransactionList& pending_queue = entry->pending_queue;

  TransactionList::iterator j =
      find(pending_queue.begin(), pending_queue.end(), trans);
  if (j == pending_queue.end())
    return false;

  pending_queue.erase(j);
  return true;
}

void HttpCache::ProcessPendingQueue(ActiveEntry* entry) {
  // Multiple readers may finish with an entry at once, so we want to batch up
  // calls to OnProcessPendingQueue.  This flag also tells us that we should
  // not delete the entry before OnProcessPendingQueue runs.
  if (entry->will_process_pending_queue)
    return;
  entry->will_process_pending_queue = true;

  MessageLoop::current()->PostTask(FROM_HERE,
      task_factory_.NewRunnableMethod(&HttpCache::OnProcessPendingQueue,
                                      entry));
}

void HttpCache::OnProcessPendingQueue(ActiveEntry* entry) {
  entry->will_process_pending_queue = false;
  DCHECK(!entry->writer);

  // If no one is interested in this entry, then we can de-activate it.
  if (entry->pending_queue.empty()) {
    if (entry->readers.empty())
      DestroyEntry(entry);
    return;
  }

  // Promote next transaction from the pending queue.
  Transaction* next = entry->pending_queue.front();
  if ((next->mode() & Transaction::WRITE) && !entry->readers.empty())
    return;  // Have to wait.

  entry->pending_queue.erase(entry->pending_queue.begin());

  AddTransactionToEntry(entry, next);
}

void HttpCache::CloseIdleConnections() {
  net::HttpNetworkLayer* network =
      static_cast<net::HttpNetworkLayer*>(network_layer_.get());
  HttpNetworkSession* session = network->GetSession();
  if (session) {
    session->tcp_socket_pool()->CloseIdleSockets();
  }
}

//-----------------------------------------------------------------------------

}  // namespace net
