// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file declares a HttpTransactionFactory implementation that can be
// layered on top of another HttpTransactionFactory to add HTTP caching.  The
// caching logic follows RFC 2616 (any exceptions are called out in the code).
//
// The HttpCache takes a disk_cache::Backend as a parameter, and uses that for
// the cache storage.
//
// See HttpTransactionFactory and HttpTransaction for more details.

#ifndef NET_HTTP_HTTP_CACHE_H_
#define NET_HTTP_HTTP_CACHE_H_
#pragma once

#include <list>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/hash_tables.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/threading/non_thread_safe.h"
#include "base/weak_ptr.h"
#include "net/base/cache_type.h"
#include "net/base/completion_callback.h"
#include "net/base/load_states.h"
#include "net/http/http_transaction_factory.h"

class GURL;

namespace disk_cache {
class Backend;
class Entry;
}

namespace net {

class CertVerifier;
class DnsCertProvenanceChecker;
class DnsRRResolver;
class HostResolver;
class HttpAuthHandlerFactory;
class HttpNetworkDelegate;
class HttpNetworkSession;
struct HttpRequestInfo;
class HttpResponseInfo;
class IOBuffer;
class NetLog;
class ProxyService;
class SSLConfigService;
class ViewCacheHelper;

class HttpCache : public HttpTransactionFactory,
                  public base::SupportsWeakPtr<HttpCache>,
                  public base::NonThreadSafe {
 public:
  // The cache mode of operation.
  enum Mode {
    // Normal mode just behaves like a standard web cache.
    NORMAL = 0,
    // Record mode caches everything for purposes of offline playback.
    RECORD,
    // Playback mode replays from a cache without considering any
    // standard invalidations.
    PLAYBACK,
    // Disables reads and writes from the cache.
    // Equivalent to setting LOAD_DISABLE_CACHE on every request.
    DISABLE
  };

  // A BackendFactory creates a backend object to be used by the HttpCache.
  class BackendFactory {
   public:
    virtual ~BackendFactory() {}

    // The actual method to build the backend. Returns a net error code. If
    // ERR_IO_PENDING is returned, the |callback| will be notified when the
    // operation completes, and |backend| must remain valid until the
    // notification arrives.
    // The implementation must not access the factory object after invoking the
    // |callback| because the object can be deleted from within the callback.
    virtual int CreateBackend(NetLog* net_log,
                              disk_cache::Backend** backend,
                              CompletionCallback* callback) = 0;
  };

  // A default backend factory for the common use cases.
  class DefaultBackend : public BackendFactory {
   public:
    // |path| is the destination for any files used by the backend, and
    // |cache_thread| is the thread where disk operations should take place. If
    // |max_bytes| is  zero, a default value will be calculated automatically.
    DefaultBackend(CacheType type, const FilePath& path, int max_bytes,
                   base::MessageLoopProxy* thread);
    virtual ~DefaultBackend();

    // Returns a factory for an in-memory cache.
    static BackendFactory* InMemory(int max_bytes);

    // BackendFactory implementation.
    virtual int CreateBackend(NetLog* net_log,
                              disk_cache::Backend** backend,
                              CompletionCallback* callback);

   private:
    CacheType type_;
    const FilePath path_;
    int max_bytes_;
    scoped_refptr<base::MessageLoopProxy> thread_;
  };

  // The disk cache is initialized lazily (by CreateTransaction) in this case.
  // The HttpCache takes ownership of the |backend_factory|.
  HttpCache(HostResolver* host_resolver,
            CertVerifier* cert_verifier,
            DnsRRResolver* dnsrr_resolver,
            DnsCertProvenanceChecker* dns_cert_checker,
            ProxyService* proxy_service,
            SSLConfigService* ssl_config_service,
            HttpAuthHandlerFactory* http_auth_handler_factory,
            HttpNetworkDelegate* network_delegate,
            NetLog* net_log,
            BackendFactory* backend_factory);

  // The disk cache is initialized lazily (by CreateTransaction) in  this case.
  // Provide an existing HttpNetworkSession, the cache can construct a
  // network layer with a shared HttpNetworkSession in order for multiple
  // network layers to share information (e.g. authentication data). The
  // HttpCache takes ownership of the |backend_factory|.
  HttpCache(HttpNetworkSession* session, BackendFactory* backend_factory);

  // Initialize the cache from its component parts, which is useful for
  // testing.  The lifetime of the network_layer and backend_factory are managed
  // by the HttpCache and will be destroyed using |delete| when the HttpCache is
  // destroyed.
  HttpCache(HttpTransactionFactory* network_layer,
            NetLog* net_log,
            BackendFactory* backend_factory);

  ~HttpCache();

  HttpTransactionFactory* network_layer() { return network_layer_.get(); }

  // Retrieves the cache backend for this HttpCache instance. If the backend
  // is not initialized yet, this method will initialize it. The return value is
  // a network error code, and it could be ERR_IO_PENDING, in which case the
  // |callback| will be notified when the operation completes. The pointer that
  // receives the |backend| must remain valid until the operation completes.
  int GetBackend(disk_cache::Backend** backend, CompletionCallback* callback);

  // Returns the current backend (can be NULL).
  disk_cache::Backend* GetCurrentBackend();

  // Given a header data blob, convert it to a response info object.
  static bool ParseResponseInfo(const char* data, int len,
                                HttpResponseInfo* response_info,
                                bool* response_truncated);

  // Writes |buf_len| bytes of metadata stored in |buf| to the cache entry
  // referenced by |url|, as long as the entry's |expected_response_time| has
  // not changed. This method returns without blocking, and the operation will
  // be performed asynchronously without any completion notification.
  void WriteMetadata(const GURL& url, base::Time expected_response_time,
                     IOBuffer* buf, int buf_len);

  // Get/Set the cache's mode.
  void set_mode(Mode value) { mode_ = value; }
  Mode mode() { return mode_; }

  // Close currently active sockets so that fresh page loads will not use any
  // recycled connections.  For sockets currently in use, they may not close
  // immediately, but they will not be reusable. This is for debugging.
  void CloseCurrentConnections();

  // HttpTransactionFactory implementation:
  virtual int CreateTransaction(scoped_ptr<HttpTransaction>* trans);
  virtual HttpCache* GetCache();
  virtual HttpNetworkSession* GetSession();
  virtual void Suspend(bool suspend);

 protected:
  // Disk cache entry data indices.
  enum {
    kResponseInfoIndex = 0,
    kResponseContentIndex,
    kMetadataIndex,

    // Must remain at the end of the enum.
    kNumCacheEntryDataIndices
  };
  friend class ViewCacheHelper;

 private:
  // Types --------------------------------------------------------------------

  class BackendCallback;
  class MetadataWriter;
  class SSLHostInfoFactoryAdaptor;
  class Transaction;
  class WorkItem;
  friend class Transaction;
  struct PendingOp;  // Info for an entry under construction.

  typedef std::list<Transaction*> TransactionList;
  typedef std::list<WorkItem*> WorkItemList;

  struct ActiveEntry {
    explicit ActiveEntry(disk_cache::Entry* entry);
    ~ActiveEntry();

    disk_cache::Entry* disk_entry;
    Transaction*       writer;
    TransactionList    readers;
    TransactionList    pending_queue;
    bool               will_process_pending_queue;
    bool               doomed;
  };

  typedef base::hash_map<std::string, ActiveEntry*> ActiveEntriesMap;
  typedef base::hash_map<std::string, PendingOp*> PendingOpsMap;
  typedef std::set<ActiveEntry*> ActiveEntriesSet;
  typedef base::hash_map<std::string, int> PlaybackCacheMap;

  // Methods ------------------------------------------------------------------

  // Creates the |backend| object and notifies the |callback| when the operation
  // completes. Returns an error code.
  int CreateBackend(disk_cache::Backend** backend,
                    CompletionCallback* callback);

  // Makes sure that the backend creation is complete before allowing the
  // provided transaction to use the object. Returns an error code.  |trans|
  // will be notified via its IO callback if this method returns ERR_IO_PENDING.
  // The transaction is free to use the backend directly at any time after
  // receiving the notification.
  int GetBackendForTransaction(Transaction* trans);

  // Generates the cache key for this request.
  std::string GenerateCacheKey(const HttpRequestInfo*);

  // Dooms the entry selected by |key|. |trans| will be notified via its IO
  // callback if this method returns ERR_IO_PENDING. The entry can be
  // currently in use or not.
  int DoomEntry(const std::string& key, Transaction* trans);

  // Dooms the entry selected by |key|. |trans| will be notified via its IO
  // callback if this method returns ERR_IO_PENDING. The entry should not
  // be currently in use.
  int AsyncDoomEntry(const std::string& key, Transaction* trans);

  // Closes a previously doomed entry.
  void FinalizeDoomedEntry(ActiveEntry* entry);

  // Returns an entry that is currently in use and not doomed, or NULL.
  ActiveEntry* FindActiveEntry(const std::string& key);

  // Creates a new ActiveEntry and starts tracking it. |disk_entry| is the disk
  // cache entry.
  ActiveEntry* ActivateEntry(disk_cache::Entry* disk_entry);

  // Deletes an ActiveEntry.
  void DeactivateEntry(ActiveEntry* entry);

  // Deletes an ActiveEntry using an exhaustive search.
  void SlowDeactivateEntry(ActiveEntry* entry);

  // Returns the PendingOp for the desired |key|. If an entry is not under
  // construction already, a new PendingOp structure is created.
  PendingOp* GetPendingOp(const std::string& key);

  // Deletes a PendingOp.
  void DeletePendingOp(PendingOp* pending_op);

  // Opens the disk cache entry associated with |key|, returning an ActiveEntry
  // in |*entry|. |trans| will be notified via its IO callback if this method
  // returns ERR_IO_PENDING.
  int OpenEntry(const std::string& key, ActiveEntry** entry,
                Transaction* trans);

  // Creates the disk cache entry associated with |key|, returning an
  // ActiveEntry in |*entry|. |trans| will be notified via its IO callback if
  // this method returns ERR_IO_PENDING.
  int CreateEntry(const std::string& key, ActiveEntry** entry,
                  Transaction* trans);

  // Destroys an ActiveEntry (active or doomed).
  void DestroyEntry(ActiveEntry* entry);

  // Adds a transaction to an ActiveEntry. If this method returns ERR_IO_PENDING
  // the transaction will be notified about completion via its IO callback. This
  // method returns ERR_CACHE_RACE to signal the transaction that it cannot be
  // added to the provided entry, and it should retry the process with another
  // one (in this case, the entry is no longer valid).
  int AddTransactionToEntry(ActiveEntry* entry, Transaction* trans);

  // Called when the transaction has finished working with this entry. |cancel|
  // is true if the operation was cancelled by the caller instead of running
  // to completion.
  void DoneWithEntry(ActiveEntry* entry, Transaction* trans, bool cancel);

  // Called when the transaction has finished writting to this entry. |success|
  // is false if the cache entry should be deleted.
  void DoneWritingToEntry(ActiveEntry* entry, bool success);

  // Called when the transaction has finished reading from this entry.
  void DoneReadingFromEntry(ActiveEntry* entry, Transaction* trans);

  // Convers the active writter transaction to a reader so that other
  // transactions can start reading from this entry.
  void ConvertWriterToReader(ActiveEntry* entry);

  // Returns the LoadState of the provided pending transaction.
  LoadState GetLoadStateForPendingTransaction(const Transaction* trans);

  // Removes the transaction |trans|, from the pending list of an entry
  // (PendingOp, active or doomed entry).
  void RemovePendingTransaction(Transaction* trans);

  // Removes the transaction |trans|, from the pending list of |entry|.
  bool RemovePendingTransactionFromEntry(ActiveEntry* entry,
                                         Transaction* trans);

  // Removes the transaction |trans|, from the pending list of |pending_op|.
  bool RemovePendingTransactionFromPendingOp(PendingOp* pending_op,
                                             Transaction* trans);

  // Resumes processing the pending list of |entry|.
  void ProcessPendingQueue(ActiveEntry* entry);

  // Events (called via PostTask) ---------------------------------------------

  void OnProcessPendingQueue(ActiveEntry* entry);

  // Callbacks ----------------------------------------------------------------

  // Processes BackendCallback notifications.
  void OnIOComplete(int result, PendingOp* entry);

  // Processes the backend creation notification.
  void OnBackendCreated(int result, PendingOp* pending_op);


  // Variables ----------------------------------------------------------------

  NetLog* net_log_;

  // Used when lazily constructing the disk_cache_.
  scoped_ptr<BackendFactory> backend_factory_;
  bool building_backend_;

  Mode mode_;

  const scoped_ptr<SSLHostInfoFactoryAdaptor> ssl_host_info_factory_;

  const scoped_ptr<HttpTransactionFactory> network_layer_;
  scoped_ptr<disk_cache::Backend> disk_cache_;

  // The set of active entries indexed by cache key.
  ActiveEntriesMap active_entries_;

  // The set of doomed entries.
  ActiveEntriesSet doomed_entries_;

  // The set of entries "under construction".
  PendingOpsMap pending_ops_;

  ScopedRunnableMethodFactory<HttpCache> task_factory_;

  scoped_ptr<PlaybackCacheMap> playback_cache_map_;

  DISALLOW_COPY_AND_ASSIGN(HttpCache);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_CACHE_H_
