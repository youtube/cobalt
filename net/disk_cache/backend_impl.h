// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See net/disk_cache/disk_cache.h for the public interface of the cache.

#ifndef NET_DISK_CACHE_BACKEND_IMPL_H_
#define NET_DISK_CACHE_BACKEND_IMPL_H_
#pragma once

#include "base/file_path.h"
#include "base/hash_tables.h"
#include "base/timer.h"
#include "net/disk_cache/block_files.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/eviction.h"
#include "net/disk_cache/in_flight_backend_io.h"
#include "net/disk_cache/rankings.h"
#include "net/disk_cache/stats.h"
#include "net/disk_cache/trace.h"

namespace net {
class NetLog;
}  // namespace net

namespace disk_cache {

enum BackendFlags {
  kNone = 0,
  kMask = 1,                    // A mask (for the index table) was specified.
  kMaxSize = 1 << 1,            // A maximum size was provided.
  kUnitTestMode = 1 << 2,       // We are modifying the behavior for testing.
  kUpgradeMode = 1 << 3,        // This is the upgrade tool (dump).
  kNewEviction = 1 << 4,        // Use of new eviction was specified.
  kNoRandom = 1 << 5,           // Don't add randomness to the behavior.
  kNoLoadProtection = 1 << 6,   // Don't act conservatively under load.
  kNoBuffering = 1 << 7         // Disable extended IO buffering.
};

// This class implements the Backend interface. An object of this
// class handles the operations of the cache for a particular profile.
class BackendImpl : public Backend {
  friend class Eviction;
 public:
  BackendImpl(const FilePath& path, base::MessageLoopProxy* cache_thread,
              net::NetLog* net_log);
  // mask can be used to limit the usable size of the hash table, for testing.
  BackendImpl(const FilePath& path, uint32 mask,
              base::MessageLoopProxy* cache_thread, net::NetLog* net_log);
  ~BackendImpl();

  // Returns a new backend with the desired flags. See the declaration of
  // CreateCacheBackend().
  static int CreateBackend(const FilePath& full_path, bool force,
                           int max_bytes, net::CacheType type,
                           uint32 flags, base::MessageLoopProxy* thread,
                           net::NetLog* net_log, Backend** backend,
                           CompletionCallback* callback);

  // Performs general initialization for this current instance of the cache.
  int Init(CompletionCallback* callback);

  // Performs the actual initialization and final cleanup on destruction.
  int SyncInit();
  void CleanupCache();

  // Same bahavior as OpenNextEntry but walks the list from back to front.
  int OpenPrevEntry(void** iter, Entry** prev_entry,
                    CompletionCallback* callback);

  // Synchronous implementation of the asynchronous interface.
  int SyncOpenEntry(const std::string& key, Entry** entry);
  int SyncCreateEntry(const std::string& key, Entry** entry);
  int SyncDoomEntry(const std::string& key);
  int SyncDoomAllEntries();
  int SyncDoomEntriesBetween(const base::Time initial_time,
                             const base::Time end_time);
  int SyncDoomEntriesSince(const base::Time initial_time);
  int SyncOpenNextEntry(void** iter, Entry** next_entry);
  int SyncOpenPrevEntry(void** iter, Entry** prev_entry);
  void SyncEndEnumeration(void* iter);

  // Open or create an entry for the given |key| or |iter|.
  EntryImpl* OpenEntryImpl(const std::string& key);
  EntryImpl* CreateEntryImpl(const std::string& key);
  EntryImpl* OpenNextEntryImpl(void** iter);
  EntryImpl* OpenPrevEntryImpl(void** iter);

  // Sets the maximum size for the total amount of data stored by this instance.
  bool SetMaxSize(int max_bytes);

  // Sets the cache type for this backend.
  void SetType(net::CacheType type);

  // Returns the full name for an external storage file.
  FilePath GetFileName(Addr address) const;

  // Returns the actual file used to store a given (non-external) address.
  MappedFile* File(Addr address);

  InFlightBackendIO* background_queue() {
    return &background_queue_;
  }

  // Creates an external storage file.
  bool CreateExternalFile(Addr* address);

  // Creates a new storage block of size block_count.
  bool CreateBlock(FileType block_type, int block_count,
                   Addr* block_address);

  // Deletes a given storage block. deep set to true can be used to zero-fill
  // the related storage in addition of releasing the related block.
  void DeleteBlock(Addr block_address, bool deep);

  // Retrieves a pointer to the lru-related data.
  LruData* GetLruData();

  // Updates the ranking information for an entry.
  void UpdateRank(EntryImpl* entry, bool modified);

  // A node was recovered from a crash, it may not be on the index, so this
  // method checks it and takes the appropriate action.
  void RecoveredEntry(CacheRankingsBlock* rankings);

  // Permanently deletes an entry, but still keeps track of it.
  void InternalDoomEntry(EntryImpl* entry);

  // Removes all references to this entry.
  void RemoveEntry(EntryImpl* entry);

  // This method must be called when an entry is released for the last time, so
  // the entry should not be used anymore. |address| is the cache address of the
  // entry.
  void OnEntryDestroyBegin(Addr address);

  // This method must be called after all resources for an entry have been
  // released.
  void OnEntryDestroyEnd();

  // If the data stored by the provided |rankings| points to an open entry,
  // returns a pointer to that entry, otherwise returns NULL. Note that this
  // method does NOT increase the ref counter for the entry.
  EntryImpl* GetOpenEntry(CacheRankingsBlock* rankings) const;

  // Returns the id being used on this run of the cache.
  int32 GetCurrentEntryId() const;

  // Returns the maximum size for a file to reside on the cache.
  int MaxFileSize() const;

  // A user data block is being created, extended or truncated.
  void ModifyStorageSize(int32 old_size, int32 new_size);

  // Logs requests that are denied due to being too big.
  void TooMuchStorageRequested(int32 size);

  // Returns true if a temporary buffer is allowed to be extended.
  bool IsAllocAllowed(int current_size, int new_size);

  // Tracks the release of |size| bytes by an entry buffer.
  void BufferDeleted(int size);

  // Only intended for testing the two previous methods.
  int GetTotalBuffersSize() const {
    return buffer_bytes_;
  }

  // Returns true if this instance seems to be under heavy load.
  bool IsLoaded() const;

  // Returns the full histogram name, for the given base |name| and experiment,
  // and the current cache type. The name will be "DiskCache.t.name_e" where n
  // is the cache type and e the provided |experiment|.
  std::string HistogramName(const char* name, int experiment) const;

  net::CacheType cache_type() const {
    return cache_type_;
  }

  // Returns a weak pointer to this object.
  base::WeakPtr<BackendImpl> GetWeakPtr();

  // Returns the group for this client, based on the current cache size.
  int GetSizeGroup() const;

  // Returns true if we should send histograms for this user again. The caller
  // must call this function only once per run (because it returns always the
  // same thing on a given run).
  bool ShouldReportAgain();

  // Reports some data when we filled up the cache.
  void FirstEviction();

  // Reports a critical error (and disables the cache).
  void CriticalError(int error);

  // Reports an uncommon, recoverable error.
  void ReportError(int error);

  // Called when an interesting event should be logged (counted).
  void OnEvent(Stats::Counters an_event);

  // Keeps track of paylod access (doesn't include metadata).
  void OnRead(int bytes);
  void OnWrite(int bytes);

  // Timer callback to calculate usage statistics.
  void OnStatsTimer();

  // Handles the pending asynchronous IO count.
  void IncrementIoCount();
  void DecrementIoCount();

  // Sets internal parameters to enable unit testing mode.
  void SetUnitTestMode();

  // Sets internal parameters to enable upgrade mode (for internal tools).
  void SetUpgradeMode();

  // Sets the eviction algorithm to version 2.
  void SetNewEviction();

  // Sets an explicit set of BackendFlags.
  void SetFlags(uint32 flags);

  // Clears the counter of references to test handling of corruptions.
  void ClearRefCountForTest();

  // Sends a dummy operation through the operation queue, for unit tests.
  int FlushQueueForTest(CompletionCallback* callback);

  // Runs the provided task on the cache thread. The task will be automatically
  // deleted after it runs.
  int RunTaskForTest(Task* task, CompletionCallback* callback);

  // Trims an entry (all if |empty| is true) from the list of deleted
  // entries. This method should be called directly on the cache thread.
  void TrimForTest(bool empty);

  // Trims an entry (all if |empty| is true) from the list of deleted
  // entries. This method should be called directly on the cache thread.
  void TrimDeletedListForTest(bool empty);

  // Peforms a simple self-check, and returns the number of dirty items
  // or an error code (negative value).
  int SelfCheck();

  // Backend interface.
  virtual int32 GetEntryCount() const;
  virtual int OpenEntry(const std::string& key, Entry** entry,
                        CompletionCallback* callback);
  virtual int CreateEntry(const std::string& key, Entry** entry,
                          CompletionCallback* callback);
  virtual int DoomEntry(const std::string& key, CompletionCallback* callback);
  virtual int DoomAllEntries(CompletionCallback* callback);
  virtual int DoomEntriesBetween(const base::Time initial_time,
                                 const base::Time end_time,
                                 CompletionCallback* callback);
  virtual int DoomEntriesSince(const base::Time initial_time,
                               CompletionCallback* callback);
  virtual int OpenNextEntry(void** iter, Entry** next_entry,
                            CompletionCallback* callback);
  virtual void EndEnumeration(void** iter);
  virtual void GetStats(StatsItems* stats);

 private:
  typedef base::hash_map<CacheAddr, EntryImpl*> EntriesMap;

  // Creates a new backing file for the cache index.
  bool CreateBackingStore(disk_cache::File* file);
  bool InitBackingStore(bool* file_created);
  void AdjustMaxCacheSize(int table_len);

  // Deletes the cache and starts again.
  void RestartCache(bool failure);
  void PrepareForRestart();

  // Creates a new entry object. Returns zero on success, or a disk_cache error
  // on failure.
  int NewEntry(Addr address, EntryImpl** entry);

  // Returns a given entry from the cache. The entry to match is determined by
  // key and hash, and the returned entry may be the matched one or it's parent
  // on the list of entries with the same hash (or bucket). To look for a parent
  // of a given entry, |entry_addr| should be grabbed from that entry, so that
  // if it doesn't match the entry on the index, we know that it was replaced
  // with a new entry; in this case |*match_error| will be set to true and the
  // return value will be NULL.
  EntryImpl* MatchEntry(const std::string& key, uint32 hash, bool find_parent,
                        Addr entry_addr, bool* match_error);

  // Opens the next or previous entry on a cache iteration.
  EntryImpl* OpenFollowingEntry(bool forward, void** iter);

  // Opens the next or previous entry on a single list. If successfull,
  // |from_entry| will be updated to point to the new entry, otherwise it will
  // be set to NULL; in other words, it is used as an explicit iterator.
  bool OpenFollowingEntryFromList(bool forward, Rankings::List list,
                                  CacheRankingsBlock** from_entry,
                                  EntryImpl** next_entry);

  // Returns the entry that is pointed by |next|.
  EntryImpl* GetEnumeratedEntry(CacheRankingsBlock* next);

  // Re-opens an entry that was previously deleted.
  EntryImpl* ResurrectEntry(EntryImpl* deleted_entry);

  void DestroyInvalidEntry(EntryImpl* entry);

  // Handles the used storage count.
  void AddStorageSize(int32 bytes);
  void SubstractStorageSize(int32 bytes);

  // Update the number of referenced cache entries.
  void IncreaseNumRefs();
  void DecreaseNumRefs();
  void IncreaseNumEntries();
  void DecreaseNumEntries();

  // Dumps current cache statistics to the log.
  void LogStats();

  // Send UMA stats.
  void ReportStats();

  // Upgrades the index file to version 2.1.
  void UpgradeTo2_1();

  // Performs basic checks on the index file. Returns false on failure.
  bool CheckIndex();

  // Part of the selt test. Returns the number or dirty entries, or an error.
  int CheckAllEntries();

  // Part of the self test. Returns false if the entry is corrupt.
  bool CheckEntry(EntryImpl* cache_entry);

  // Returns the maximum total memory for the memory buffers.
  int MaxBuffersSize();

  InFlightBackendIO background_queue_;  // The controller of pending operations.
  scoped_refptr<MappedFile> index_;  // The main cache index.
  FilePath path_;  // Path to the folder used as backing storage.
  Index* data_;  // Pointer to the index data.
  BlockFiles block_files_;  // Set of files used to store all data.
  Rankings rankings_;  // Rankings to be able to trim the cache.
  uint32 mask_;  // Binary mask to map a hash to the hash table.
  int32 max_size_;  // Maximum data size for this instance.
  Eviction eviction_;  // Handler of the eviction algorithm.
  EntriesMap open_entries_;  // Map of open entries.
  int num_refs_;  // Number of referenced cache entries.
  int max_refs_;  // Max number of referenced cache entries.
  int num_pending_io_;  // Number of pending IO operations.
  int entry_count_;  // Number of entries accessed lately.
  int byte_count_;  // Number of bytes read/written lately.
  int buffer_bytes_;  // Total size of the temporary entries' buffers.
  int io_delay_;  // Average time (ms) required to complete some IO operations.
  net::CacheType cache_type_;
  int uma_report_;  // Controls transmision of UMA data.
  uint32 user_flags_;  // Flags set by the user.
  bool init_;  // controls the initialization of the system.
  bool restarted_;
  bool unit_test_;
  bool read_only_;  // Prevents updates of the rankings data (used by tools).
  bool disabled_;
  bool new_eviction_;  // What eviction algorithm should be used.
  bool first_timer_;  // True if the timer has not been called.
  bool throttle_requests_;

  net::NetLog* net_log_;

  Stats stats_;  // Usage statistcs.
  base::RepeatingTimer<BackendImpl> timer_;  // Usage timer.
  base::WaitableEvent done_;  // Signals the end of background work.
  scoped_refptr<TraceObject> trace_object_;  // Inits internal tracing.
  ScopedRunnableMethodFactory<BackendImpl> factory_;
  base::WeakPtrFactory<BackendImpl> ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BackendImpl);
};

// Returns the prefered max cache size given the available disk space.
int PreferedCacheSize(int64 available);

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BACKEND_IMPL_H_
