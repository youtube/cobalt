// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/backend_impl.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/stats_counters.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/sys_info.h"
#include "base/threading/worker_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "base/timer.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/entry_impl.h"
#include "net/disk_cache/errors.h"
#include "net/disk_cache/experiments.h"
#include "net/disk_cache/file.h"
#include "net/disk_cache/hash.h"
#include "net/disk_cache/mem_backend_impl.h"

// This has to be defined before including histogram_macros.h from this file.
#define NET_DISK_CACHE_BACKEND_IMPL_CC_
#include "net/disk_cache/histogram_macros.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

namespace {

const char* kIndexName = "index";
const int kMaxOldFolders = 100;

// Seems like ~240 MB correspond to less than 50k entries for 99% of the people.
// Note that the actual target is to keep the index table load factor under 55%
// for most users.
const int k64kEntriesStore = 240 * 1000 * 1000;
const int kBaseTableLen = 64 * 1024;
const int kDefaultCacheSize = 80 * 1024 * 1024;

int DesiredIndexTableLen(int32 storage_size) {
  if (storage_size <= k64kEntriesStore)
    return kBaseTableLen;
  if (storage_size <= k64kEntriesStore * 2)
    return kBaseTableLen * 2;
  if (storage_size <= k64kEntriesStore * 4)
    return kBaseTableLen * 4;
  if (storage_size <= k64kEntriesStore * 8)
    return kBaseTableLen * 8;

  // The biggest storage_size for int32 requires a 4 MB table.
  return kBaseTableLen * 16;
}

int MaxStorageSizeForTable(int table_len) {
  return table_len * (k64kEntriesStore / kBaseTableLen);
}

size_t GetIndexSize(int table_len) {
  size_t table_size = sizeof(disk_cache::CacheAddr) * table_len;
  return sizeof(disk_cache::IndexHeader) + table_size;
}

// ------------------------------------------------------------------------

// Returns a fully qualified name from path and name, using a given name prefix
// and index number. For instance, if the arguments are "/foo", "bar" and 5, it
// will return "/foo/old_bar_005".
FilePath GetPrefixedName(const FilePath& path, const std::string& name,
                         int index) {
  std::string tmp = base::StringPrintf("%s%s_%03d", "old_",
                                       name.c_str(), index);
  return path.AppendASCII(tmp);
}

// This is a simple Task to cleanup old caches.
class CleanupTask : public Task {
 public:
  CleanupTask(const FilePath& path, const std::string& name)
      : path_(path), name_(name) {}

  virtual void Run();

 private:
  FilePath path_;
  std::string name_;
  DISALLOW_COPY_AND_ASSIGN(CleanupTask);
};

void CleanupTask::Run() {
  for (int i = 0; i < kMaxOldFolders; i++) {
    FilePath to_delete = GetPrefixedName(path_, name_, i);
    disk_cache::DeleteCache(to_delete, true);
  }
}

// Returns a full path to rename the current cache, in order to delete it. path
// is the current folder location, and name is the current folder name.
FilePath GetTempCacheName(const FilePath& path, const std::string& name) {
  // We'll attempt to have up to kMaxOldFolders folders for deletion.
  for (int i = 0; i < kMaxOldFolders; i++) {
    FilePath to_delete = GetPrefixedName(path, name, i);
    if (!file_util::PathExists(to_delete))
      return to_delete;
  }
  return FilePath();
}

// Moves the cache files to a new folder and creates a task to delete them.
bool DelayedCacheCleanup(const FilePath& full_path) {
  // GetTempCacheName() and MoveCache() use synchronous file
  // operations.
  base::ThreadRestrictions::ScopedAllowIO allow_io;

  FilePath current_path = full_path.StripTrailingSeparators();

  FilePath path = current_path.DirName();
  FilePath name = current_path.BaseName();
#if defined(OS_POSIX)
  std::string name_str = name.value();
#elif defined(OS_WIN)
  // We created this file so it should only contain ASCII.
  std::string name_str = WideToASCII(name.value());
#endif

  FilePath to_delete = GetTempCacheName(path, name_str);
  if (to_delete.empty()) {
    LOG(ERROR) << "Unable to get another cache folder";
    return false;
  }

  if (!disk_cache::MoveCache(full_path, to_delete)) {
    LOG(ERROR) << "Unable to move cache folder";
    return false;
  }

  base::WorkerPool::PostTask(FROM_HERE, new CleanupTask(path, name_str), true);
  return true;
}

// Sets group for the current experiment. Returns false if the files should be
// discarded.
bool InitExperiment(disk_cache::IndexHeader* header) {
  if (header->experiment == disk_cache::EXPERIMENT_OLD_FILE1 ||
      header->experiment == disk_cache::EXPERIMENT_OLD_FILE2) {
    // Discard current cache.
    return false;
  }

  // See if we already defined the group for this profile.
  if (header->experiment >= disk_cache::EXPERIMENT_DELETED_LIST_OUT)
    return true;

  // The experiment is closed.
  header->experiment = disk_cache::EXPERIMENT_DELETED_LIST_OUT;
  return true;
}

// Initializes the field trial structures to allow performance measurements
// for the current cache configuration.
void SetFieldTrialInfo(int size_group) {
  static bool first = true;
  if (!first)
    return;

  // Field trials involve static objects so we have to do this only once.
  first = false;
  std::string group1 = base::StringPrintf("CacheSizeGroup_%d", size_group);
  int totalProbability = 10;
  scoped_refptr<base::FieldTrial> trial1(
      new base::FieldTrial("CacheSize", totalProbability, group1, 2011, 6, 30));
  trial1->AppendGroup(group1, totalProbability);
}

// ------------------------------------------------------------------------

// This class takes care of building an instance of the backend.
class CacheCreator {
 public:
  CacheCreator(const FilePath& path, bool force, int max_bytes,
               net::CacheType type, uint32 flags,
               base::MessageLoopProxy* thread, net::NetLog* net_log,
               disk_cache::Backend** backend,
               net::CompletionCallback* callback)
      : path_(path), force_(force), retry_(false), max_bytes_(max_bytes),
        type_(type), flags_(flags), thread_(thread), backend_(backend),
        callback_(callback), cache_(NULL), net_log_(net_log),
        ALLOW_THIS_IN_INITIALIZER_LIST(
            my_callback_(this, &CacheCreator::OnIOComplete)) {
  }
  ~CacheCreator() {}

  // Creates the backend.
  int Run();

  // Callback implementation.
  void OnIOComplete(int result);

 private:
  void DoCallback(int result);

  const FilePath& path_;
  bool force_;
  bool retry_;
  int max_bytes_;
  net::CacheType type_;
  uint32 flags_;
  scoped_refptr<base::MessageLoopProxy> thread_;
  disk_cache::Backend** backend_;
  net::CompletionCallback* callback_;
  disk_cache::BackendImpl* cache_;
  net::NetLog* net_log_;
  net::CompletionCallbackImpl<CacheCreator> my_callback_;

  DISALLOW_COPY_AND_ASSIGN(CacheCreator);
};

int CacheCreator::Run() {
  cache_ = new disk_cache::BackendImpl(path_, thread_, net_log_);
  cache_->SetMaxSize(max_bytes_);
  cache_->SetType(type_);
  cache_->SetFlags(flags_);
  int rv = cache_->Init(&my_callback_);
  DCHECK_EQ(net::ERR_IO_PENDING, rv);
  return rv;
}

void CacheCreator::OnIOComplete(int result) {
  if (result == net::OK || !force_ || retry_)
    return DoCallback(result);

  // This is a failure and we are supposed to try again, so delete the object,
  // delete all the files, and try again.
  retry_ = true;
  delete cache_;
  cache_ = NULL;
  if (!DelayedCacheCleanup(path_))
    return DoCallback(result);

  // The worker thread will start deleting files soon, but the original folder
  // is not there anymore... let's create a new set of files.
  int rv = Run();
  DCHECK_EQ(net::ERR_IO_PENDING, rv);
}

void CacheCreator::DoCallback(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  if (result == net::OK) {
    *backend_ = cache_;
  } else {
    LOG(ERROR) << "Unable to create cache";
    *backend_ = NULL;
    delete cache_;
  }
  callback_->Run(result);
  delete this;
}

// ------------------------------------------------------------------------

// A task to perform final cleanup on the background thread.
class FinalCleanup : public Task {
 public:
  explicit FinalCleanup(disk_cache::BackendImpl* backend) : backend_(backend) {}
  ~FinalCleanup() {}

  virtual void Run();
 private:
  disk_cache::BackendImpl* backend_;
  DISALLOW_EVIL_CONSTRUCTORS(FinalCleanup);
};

void FinalCleanup::Run() {
  backend_->CleanupCache();
}

}  // namespace

// ------------------------------------------------------------------------

namespace disk_cache {

int CreateCacheBackend(net::CacheType type, const FilePath& path, int max_bytes,
                       bool force, base::MessageLoopProxy* thread,
                       net::NetLog* net_log, Backend** backend,
                       CompletionCallback* callback) {
  DCHECK(callback);
  if (type == net::MEMORY_CACHE) {
    *backend = MemBackendImpl::CreateBackend(max_bytes);
    return *backend ? net::OK : net::ERR_FAILED;
  }
  DCHECK(thread);

  return BackendImpl::CreateBackend(path, force, max_bytes, type, kNone, thread,
                                    net_log, backend, callback);
}

// Returns the preferred maximum number of bytes for the cache given the
// number of available bytes.
int PreferedCacheSize(int64 available) {
  // Return 80% of the available space if there is not enough space to use
  // kDefaultCacheSize.
  if (available < kDefaultCacheSize * 10 / 8)
    return static_cast<int32>(available * 8 / 10);

  // Return kDefaultCacheSize if it uses 80% to 10% of the available space.
  if (available < kDefaultCacheSize * 10)
    return kDefaultCacheSize;

  // Return 10% of the available space if the target size
  // (2.5 * kDefaultCacheSize) is more than 10%.
  if (available < static_cast<int64>(kDefaultCacheSize) * 25)
    return static_cast<int32>(available / 10);

  // Return the target size (2.5 * kDefaultCacheSize) if it uses 10% to 1%
  // of the available space.
  if (available < static_cast<int64>(kDefaultCacheSize) * 250)
    return kDefaultCacheSize * 5 / 2;

  // Return 1% of the available space if it does not exceed kint32max.
  if (available < static_cast<int64>(kint32max) * 100)
    return static_cast<int32>(available / 100);

  return kint32max;
}

// ------------------------------------------------------------------------

BackendImpl::BackendImpl(const FilePath& path,
                         base::MessageLoopProxy* cache_thread,
                         net::NetLog* net_log)
    : ALLOW_THIS_IN_INITIALIZER_LIST(background_queue_(this, cache_thread)),
      path_(path),
      block_files_(path),
      mask_(0),
      max_size_(0),
      io_delay_(0),
      cache_type_(net::DISK_CACHE),
      uma_report_(0),
      user_flags_(0),
      init_(false),
      restarted_(false),
      unit_test_(false),
      read_only_(false),
      disabled_(false),
      new_eviction_(false),
      first_timer_(true),
      net_log_(net_log),
      done_(true, false),
      ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(ptr_factory_(this)) {
}

BackendImpl::BackendImpl(const FilePath& path,
                         uint32 mask,
                         base::MessageLoopProxy* cache_thread,
                         net::NetLog* net_log)
    : ALLOW_THIS_IN_INITIALIZER_LIST(background_queue_(this, cache_thread)),
      path_(path),
      block_files_(path),
      mask_(mask),
      max_size_(0),
      io_delay_(0),
      cache_type_(net::DISK_CACHE),
      uma_report_(0),
      user_flags_(kMask),
      init_(false),
      restarted_(false),
      unit_test_(false),
      read_only_(false),
      disabled_(false),
      new_eviction_(false),
      first_timer_(true),
      net_log_(net_log),
      done_(true, false),
      ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(ptr_factory_(this)) {
}

BackendImpl::~BackendImpl() {
  background_queue_.WaitForPendingIO();

  if (background_queue_.BackgroundIsCurrentThread()) {
    // Unit tests may use the same thread for everything.
    CleanupCache();
  } else {
    background_queue_.background_thread()->PostTask(FROM_HERE,
                                                    new FinalCleanup(this));
    done_.Wait();
  }
}

// If the initialization of the cache fails, and force is true, we will discard
// the whole cache and create a new one. In order to process a potentially large
// number of files, we'll rename the cache folder to old_ + original_name +
// number, (located on the same parent folder), and spawn a worker thread to
// delete all the files on all the stale cache folders. The whole process can
// still fail if we are not able to rename the cache folder (for instance due to
// a sharing violation), and in that case a cache for this profile (on the
// desired path) cannot be created.
//
// Static.
int BackendImpl::CreateBackend(const FilePath& full_path, bool force,
                               int max_bytes, net::CacheType type,
                               uint32 flags, base::MessageLoopProxy* thread,
                               net::NetLog* net_log, Backend** backend,
                               CompletionCallback* callback) {
  DCHECK(callback);
  CacheCreator* creator = new CacheCreator(full_path, force, max_bytes, type,
                                           flags, thread, net_log, backend,
                                           callback);
  // This object will self-destroy when finished.
  return creator->Run();
}

int BackendImpl::Init(CompletionCallback* callback) {
  background_queue_.Init(callback);
  return net::ERR_IO_PENDING;
}

int BackendImpl::SyncInit() {
  DCHECK(!init_);
  if (init_)
    return net::ERR_FAILED;

  bool create_files = false;
  if (!InitBackingStore(&create_files)) {
    ReportError(ERR_STORAGE_ERROR);
    return net::ERR_FAILED;
  }

  num_refs_ = num_pending_io_ = max_refs_ = 0;
  entry_count_ = byte_count_ = 0;

  if (!restarted_) {
    buffer_bytes_ = 0;
    trace_object_ = TraceObject::GetTraceObject();
    // Create a recurrent timer of 30 secs.
    int timer_delay = unit_test_ ? 1000 : 30000;
    timer_.Start(TimeDelta::FromMilliseconds(timer_delay), this,
                 &BackendImpl::OnStatsTimer);
  }

  init_ = true;

  if (data_->header.experiment != NO_EXPERIMENT &&
      cache_type_ != net::DISK_CACHE) {
    // No experiment for other caches.
    return net::ERR_FAILED;
  }

  if (!(user_flags_ & disk_cache::kNoRandom)) {
    // The unit test controls directly what to test.
    new_eviction_ = (cache_type_ == net::DISK_CACHE);
  }

  if (!CheckIndex()) {
    ReportError(ERR_INIT_FAILED);
    return net::ERR_FAILED;
  }

  if (!(user_flags_ & disk_cache::kNoRandom) &&
      cache_type_ == net::DISK_CACHE &&
      !InitExperiment(&data_->header))
    return net::ERR_FAILED;

  // We don't care if the value overflows. The only thing we care about is that
  // the id cannot be zero, because that value is used as "not dirty".
  // Increasing the value once per second gives us many years before we start
  // having collisions.
  data_->header.this_id++;
  if (!data_->header.this_id)
    data_->header.this_id++;

  if (data_->header.crash) {
    ReportError(ERR_PREVIOUS_CRASH);
  } else {
    ReportError(0);
    data_->header.crash = 1;
  }

  if (!block_files_.Init(create_files))
    return net::ERR_FAILED;

  // We want to minimize the changes to cache for an AppCache.
  if (cache_type() == net::APP_CACHE) {
    DCHECK(!new_eviction_);
    read_only_ = true;
  }

  // Setup load-time data only for the main cache.
  if (cache_type() == net::DISK_CACHE)
    SetFieldTrialInfo(GetSizeGroup());

  eviction_.Init(this);

  // stats_ and rankings_ may end up calling back to us so we better be enabled.
  disabled_ = false;
  if (!stats_.Init(this, &data_->header.stats))
    return net::ERR_FAILED;

  disabled_ = !rankings_.Init(this, new_eviction_);

  return disabled_ ? net::ERR_FAILED : net::OK;
}

void BackendImpl::CleanupCache() {
  Trace("Backend Cleanup");
  eviction_.Stop();
  timer_.Stop();

  if (init_) {
    stats_.Store();
    if (data_)
      data_->header.crash = 0;

    File::WaitForPendingIO(&num_pending_io_);
    if (user_flags_ & kNoRandom) {
      // This is a net_unittest, verify that we are not 'leaking' entries.
      DCHECK(!num_refs_);
    }
  }
  block_files_.CloseFiles();
  factory_.RevokeAll();
  ptr_factory_.InvalidateWeakPtrs();
  done_.Signal();
}

// ------------------------------------------------------------------------

int BackendImpl::OpenPrevEntry(void** iter, Entry** prev_entry,
                               CompletionCallback* callback) {
  DCHECK(callback);
  background_queue_.OpenPrevEntry(iter, prev_entry, callback);
  return net::ERR_IO_PENDING;
}

int BackendImpl::SyncOpenEntry(const std::string& key, Entry** entry) {
  DCHECK(entry);
  *entry = OpenEntryImpl(key);
  return (*entry) ? net::OK : net::ERR_FAILED;
}

int BackendImpl::SyncCreateEntry(const std::string& key, Entry** entry) {
  DCHECK(entry);
  *entry = CreateEntryImpl(key);
  return (*entry) ? net::OK : net::ERR_FAILED;
}

int BackendImpl::SyncDoomEntry(const std::string& key) {
  if (disabled_)
    return net::ERR_FAILED;

  EntryImpl* entry = OpenEntryImpl(key);
  if (!entry)
    return net::ERR_FAILED;

  entry->DoomImpl();
  entry->Release();
  return net::OK;
}

int BackendImpl::SyncDoomAllEntries() {
  // This is not really an error, but it is an interesting condition.
  ReportError(ERR_CACHE_DOOMED);
  stats_.OnEvent(Stats::DOOM_CACHE);
  if (!num_refs_) {
    RestartCache(false);
    return disabled_ ? net::ERR_FAILED : net::OK;
  } else {
    if (disabled_)
      return net::ERR_FAILED;

    eviction_.TrimCache(true);
    return net::OK;
  }
}

int BackendImpl::SyncDoomEntriesBetween(const base::Time initial_time,
                                        const base::Time end_time) {
  DCHECK_NE(net::APP_CACHE, cache_type_);
  if (end_time.is_null())
    return SyncDoomEntriesSince(initial_time);

  DCHECK(end_time >= initial_time);

  if (disabled_)
    return net::ERR_FAILED;

  EntryImpl* node;
  void* iter = NULL;
  EntryImpl* next = OpenNextEntryImpl(&iter);
  if (!next)
    return net::OK;

  while (next) {
    node = next;
    next = OpenNextEntryImpl(&iter);

    if (node->GetLastUsed() >= initial_time &&
        node->GetLastUsed() < end_time) {
      node->DoomImpl();
    } else if (node->GetLastUsed() < initial_time) {
      if (next)
        next->Release();
      next = NULL;
      SyncEndEnumeration(iter);
    }

    node->Release();
  }

  return net::OK;
}

// We use OpenNextEntryImpl to retrieve elements from the cache, until we get
// entries that are too old.
int BackendImpl::SyncDoomEntriesSince(const base::Time initial_time) {
  DCHECK_NE(net::APP_CACHE, cache_type_);
  if (disabled_)
    return net::ERR_FAILED;

  stats_.OnEvent(Stats::DOOM_RECENT);
  for (;;) {
    void* iter = NULL;
    EntryImpl* entry = OpenNextEntryImpl(&iter);
    if (!entry)
      return net::OK;

    if (initial_time > entry->GetLastUsed()) {
      entry->Release();
      SyncEndEnumeration(iter);
      return net::OK;
    }

    entry->DoomImpl();
    entry->Release();
    SyncEndEnumeration(iter);  // Dooming the entry invalidates the iterator.
  }
}

int BackendImpl::SyncOpenNextEntry(void** iter, Entry** next_entry) {
  *next_entry = OpenNextEntryImpl(iter);
  return (*next_entry) ? net::OK : net::ERR_FAILED;
}

int BackendImpl::SyncOpenPrevEntry(void** iter, Entry** prev_entry) {
  *prev_entry = OpenPrevEntryImpl(iter);
  return (*prev_entry) ? net::OK : net::ERR_FAILED;
}

void BackendImpl::SyncEndEnumeration(void* iter) {
  scoped_ptr<Rankings::Iterator> iterator(
      reinterpret_cast<Rankings::Iterator*>(iter));
}

EntryImpl* BackendImpl::OpenEntryImpl(const std::string& key) {
  if (disabled_)
    return NULL;

  TimeTicks start = TimeTicks::Now();
  uint32 hash = Hash(key);
  Trace("Open hash 0x%x", hash);

  bool error;
  EntryImpl* cache_entry = MatchEntry(key, hash, false, Addr(), &error);
  if (!cache_entry) {
    stats_.OnEvent(Stats::OPEN_MISS);
    return NULL;
  }

  if (ENTRY_NORMAL != cache_entry->entry()->Data()->state) {
    // The entry was already evicted.
    cache_entry->Release();
    stats_.OnEvent(Stats::OPEN_MISS);
    return NULL;
  }

  eviction_.OnOpenEntry(cache_entry);
  entry_count_++;

  CACHE_UMA(AGE_MS, "OpenTime", GetSizeGroup(), start);
  stats_.OnEvent(Stats::OPEN_HIT);
  SIMPLE_STATS_COUNTER("disk_cache.hit");
  return cache_entry;
}

EntryImpl* BackendImpl::CreateEntryImpl(const std::string& key) {
  if (disabled_ || key.empty())
    return NULL;

  TimeTicks start = TimeTicks::Now();
  uint32 hash = Hash(key);
  Trace("Create hash 0x%x", hash);

  scoped_refptr<EntryImpl> parent;
  Addr entry_address(data_->table[hash & mask_]);
  if (entry_address.is_initialized()) {
    // We have an entry already. It could be the one we are looking for, or just
    // a hash conflict.
    bool error;
    EntryImpl* old_entry = MatchEntry(key, hash, false, Addr(), &error);
    if (old_entry)
      return ResurrectEntry(old_entry);

    EntryImpl* parent_entry = MatchEntry(key, hash, true, Addr(), &error);
    DCHECK(!error);
    if (parent_entry) {
      parent.swap(&parent_entry);
    } else if (data_->table[hash & mask_]) {
      // We should have corrected the problem.
      NOTREACHED();
      return NULL;
    }
  }

  int num_blocks;
  size_t key1_len = sizeof(EntryStore) - offsetof(EntryStore, key);
  if (key.size() < key1_len ||
      key.size() > static_cast<size_t>(kMaxInternalKeyLength))
    num_blocks = 1;
  else
    num_blocks = static_cast<int>((key.size() - key1_len) / 256 + 2);

  if (!block_files_.CreateBlock(BLOCK_256, num_blocks, &entry_address)) {
    LOG(ERROR) << "Create entry failed " << key.c_str();
    stats_.OnEvent(Stats::CREATE_ERROR);
    return NULL;
  }

  Addr node_address(0);
  if (!block_files_.CreateBlock(RANKINGS, 1, &node_address)) {
    block_files_.DeleteBlock(entry_address, false);
    LOG(ERROR) << "Create entry failed " << key.c_str();
    stats_.OnEvent(Stats::CREATE_ERROR);
    return NULL;
  }

  scoped_refptr<EntryImpl> cache_entry(
      new EntryImpl(this, entry_address, false));
  IncreaseNumRefs();

  if (!cache_entry->CreateEntry(node_address, key, hash)) {
    block_files_.DeleteBlock(entry_address, false);
    block_files_.DeleteBlock(node_address, false);
    LOG(ERROR) << "Create entry failed " << key.c_str();
    stats_.OnEvent(Stats::CREATE_ERROR);
    return NULL;
  }

  cache_entry->BeginLogging(net_log_, true);

  // We are not failing the operation; let's add this to the map.
  open_entries_[entry_address.value()] = cache_entry;

  if (parent.get())
    parent->SetNextAddress(entry_address);

  block_files_.GetFile(entry_address)->Store(cache_entry->entry());
  block_files_.GetFile(node_address)->Store(cache_entry->rankings());

  IncreaseNumEntries();
  eviction_.OnCreateEntry(cache_entry);
  entry_count_++;
  if (!parent.get())
    data_->table[hash & mask_] = entry_address.value();

  CACHE_UMA(AGE_MS, "CreateTime", GetSizeGroup(), start);
  stats_.OnEvent(Stats::CREATE_HIT);
  SIMPLE_STATS_COUNTER("disk_cache.miss");
  Trace("create entry hit ");
  return cache_entry.release();
}

EntryImpl* BackendImpl::OpenNextEntryImpl(void** iter) {
  return OpenFollowingEntry(true, iter);
}

EntryImpl* BackendImpl::OpenPrevEntryImpl(void** iter) {
  return OpenFollowingEntry(false, iter);
}

bool BackendImpl::SetMaxSize(int max_bytes) {
  COMPILE_ASSERT(sizeof(max_bytes) == sizeof(max_size_), unsupported_int_model);
  if (max_bytes < 0)
    return false;

  // Zero size means use the default.
  if (!max_bytes)
    return true;

  // Avoid a DCHECK later on.
  if (max_bytes >= kint32max - kint32max / 10)
    max_bytes = kint32max - kint32max / 10 - 1;

  user_flags_ |= kMaxSize;
  max_size_ = max_bytes;
  return true;
}

void BackendImpl::SetType(net::CacheType type) {
  DCHECK(type != net::MEMORY_CACHE);
  cache_type_ = type;
}

FilePath BackendImpl::GetFileName(Addr address) const {
  if (!address.is_separate_file() || !address.is_initialized()) {
    NOTREACHED();
    return FilePath();
  }

  std::string tmp = base::StringPrintf("f_%06x", address.FileNumber());
  return path_.AppendASCII(tmp);
}

MappedFile* BackendImpl::File(Addr address) {
  if (disabled_)
    return NULL;
  return block_files_.GetFile(address);
}

bool BackendImpl::CreateExternalFile(Addr* address) {
  int file_number = data_->header.last_file + 1;
  Addr file_address(0);
  bool success = false;
  for (int i = 0; i < 0x0fffffff; i++, file_number++) {
    if (!file_address.SetFileNumber(file_number)) {
      file_number = 1;
      continue;
    }
    FilePath name = GetFileName(file_address);
    int flags = base::PLATFORM_FILE_READ |
                base::PLATFORM_FILE_WRITE |
                base::PLATFORM_FILE_CREATE |
                base::PLATFORM_FILE_EXCLUSIVE_WRITE;
    base::PlatformFileError error;
    scoped_refptr<disk_cache::File> file(new disk_cache::File(
        base::CreatePlatformFile(name, flags, NULL, &error)));
    if (!file->IsValid()) {
      if (error != base::PLATFORM_FILE_ERROR_EXISTS)
        return false;
      continue;
    }

    success = true;
    break;
  }

  DCHECK(success);
  if (!success)
    return false;

  data_->header.last_file = file_number;
  address->set_value(file_address.value());
  return true;
}

bool BackendImpl::CreateBlock(FileType block_type, int block_count,
                             Addr* block_address) {
  return block_files_.CreateBlock(block_type, block_count, block_address);
}

void BackendImpl::DeleteBlock(Addr block_address, bool deep) {
  block_files_.DeleteBlock(block_address, deep);
}

LruData* BackendImpl::GetLruData() {
  return &data_->header.lru;
}

void BackendImpl::UpdateRank(EntryImpl* entry, bool modified) {
  if (!read_only_) {
    eviction_.UpdateRank(entry, modified);
  }
}

void BackendImpl::RecoveredEntry(CacheRankingsBlock* rankings) {
  Addr address(rankings->Data()->contents);
  EntryImpl* cache_entry = NULL;
  if (NewEntry(address, &cache_entry))
    return;

  uint32 hash = cache_entry->GetHash();
  cache_entry->Release();

  // Anything on the table means that this entry is there.
  if (data_->table[hash & mask_])
    return;

  data_->table[hash & mask_] = address.value();
}

void BackendImpl::InternalDoomEntry(EntryImpl* entry) {
  uint32 hash = entry->GetHash();
  std::string key = entry->GetKey();
  Addr entry_addr = entry->entry()->address();
  bool error;
  EntryImpl* parent_entry = MatchEntry(key, hash, true, entry_addr, &error);
  CacheAddr child(entry->GetNextAddress());

  Trace("Doom entry 0x%p", entry);

  if (!entry->doomed()) {
    // We may have doomed this entry from within MatchEntry.
    eviction_.OnDoomEntry(entry);
    entry->InternalDoom();
    if (!new_eviction_) {
      DecreaseNumEntries();
    }
    stats_.OnEvent(Stats::DOOM_ENTRY);
  }

  if (parent_entry) {
    parent_entry->SetNextAddress(Addr(child));
    parent_entry->Release();
  } else if (!error) {
    data_->table[hash & mask_] = child;
  }
}

// An entry may be linked on the DELETED list for a while after being doomed.
// This function is called when we want to remove it.
void BackendImpl::RemoveEntry(EntryImpl* entry) {
  if (!new_eviction_)
    return;

  DCHECK(ENTRY_NORMAL != entry->entry()->Data()->state);

  Trace("Remove entry 0x%p", entry);
  eviction_.OnDestroyEntry(entry);
  DecreaseNumEntries();
}

void BackendImpl::OnEntryDestroyBegin(Addr address) {
  EntriesMap::iterator it = open_entries_.find(address.value());
  if (it != open_entries_.end())
    open_entries_.erase(it);
}

void BackendImpl::OnEntryDestroyEnd() {
  DecreaseNumRefs();
  if (data_->header.num_bytes > max_size_ && !read_only_)
    eviction_.TrimCache(false);
}

EntryImpl* BackendImpl::GetOpenEntry(CacheRankingsBlock* rankings) const {
  DCHECK(rankings->HasData());
  EntriesMap::const_iterator it =
      open_entries_.find(rankings->Data()->contents);
  if (it != open_entries_.end()) {
    // We have this entry in memory.
    return it->second;
  }

  return NULL;
}

int32 BackendImpl::GetCurrentEntryId() const {
  return data_->header.this_id;
}

int BackendImpl::MaxFileSize() const {
  return max_size_ / 8;
}

void BackendImpl::ModifyStorageSize(int32 old_size, int32 new_size) {
  if (disabled_ || old_size == new_size)
    return;
  if (old_size > new_size)
    SubstractStorageSize(old_size - new_size);
  else
    AddStorageSize(new_size - old_size);

  // Update the usage statistics.
  stats_.ModifyStorageStats(old_size, new_size);
}

void BackendImpl::TooMuchStorageRequested(int32 size) {
  stats_.ModifyStorageStats(0, size);
}

bool BackendImpl::IsAllocAllowed(int current_size, int new_size) {
  DCHECK_GT(new_size, current_size);
  if (user_flags_ & kNoBuffering)
    return false;

  int to_add = new_size - current_size;
  if (buffer_bytes_ + to_add > MaxBuffersSize())
    return false;

  buffer_bytes_ += to_add;
  CACHE_UMA(COUNTS_50000, "BufferBytes", 0, buffer_bytes_ / 1024);
  return true;
}

void BackendImpl::BufferDeleted(int size) {
  buffer_bytes_ -= size;
  DCHECK_GE(size, 0);
}

bool BackendImpl::IsLoaded() const {
  CACHE_UMA(COUNTS, "PendingIO", GetSizeGroup(), num_pending_io_);
  if (user_flags_ & kNoLoadProtection)
    return false;

  return num_pending_io_ > 5;
}

std::string BackendImpl::HistogramName(const char* name, int experiment) const {
  if (!experiment)
    return base::StringPrintf("DiskCache.%d.%s", cache_type_, name);
  return base::StringPrintf("DiskCache.%d.%s_%d", cache_type_,
                            name, experiment);
}

base::WeakPtr<BackendImpl> BackendImpl::GetWeakPtr() {
  return ptr_factory_.GetWeakPtr();
}

int BackendImpl::GetSizeGroup() const {
  if (disabled_)
    return 0;

  // We want to report times grouped by the current cache size (50 MB groups).
  int group = data_->header.num_bytes / (50 * 1024 * 1024);
  if (group > 6)
    group = 6;  // Limit the number of groups, just in case.
  return group;
}

// We want to remove biases from some histograms so we only send data once per
// week.
bool BackendImpl::ShouldReportAgain() {
  if (uma_report_)
    return uma_report_ == 2;

  uma_report_++;
  int64 last_report = stats_.GetCounter(Stats::LAST_REPORT);
  Time last_time = Time::FromInternalValue(last_report);
  if (!last_report || (Time::Now() - last_time).InDays() >= 7) {
    stats_.SetCounter(Stats::LAST_REPORT, Time::Now().ToInternalValue());
    uma_report_++;
    return true;
  }
  return false;
}

void BackendImpl::FirstEviction() {
  DCHECK(data_->header.create_time);
  if (!GetEntryCount())
    return;  // This is just for unit tests.

  Time create_time = Time::FromInternalValue(data_->header.create_time);
  CACHE_UMA(AGE, "FillupAge", 0, create_time);

  int64 use_time = stats_.GetCounter(Stats::TIMER);
  CACHE_UMA(HOURS, "FillupTime", 0, static_cast<int>(use_time / 120));
  CACHE_UMA(PERCENTAGE, "FirstHitRatio", 0, stats_.GetHitRatio());

  if (!use_time)
    use_time = 1;
  CACHE_UMA(COUNTS_10000, "FirstEntryAccessRate", 0,
            static_cast<int>(data_->header.num_entries / use_time));
  CACHE_UMA(COUNTS, "FirstByteIORate", 0,
            static_cast<int>((data_->header.num_bytes / 1024) / use_time));

  int avg_size = data_->header.num_bytes / GetEntryCount();
  CACHE_UMA(COUNTS, "FirstEntrySize", 0, avg_size);

  int large_entries_bytes = stats_.GetLargeEntriesSize();
  int large_ratio = large_entries_bytes * 100 / data_->header.num_bytes;
  CACHE_UMA(PERCENTAGE, "FirstLargeEntriesRatio", 0, large_ratio);

  if (new_eviction_) {
    CACHE_UMA(PERCENTAGE, "FirstResurrectRatio", 0, stats_.GetResurrectRatio());
    CACHE_UMA(PERCENTAGE, "FirstNoUseRatio", 0,
              data_->header.lru.sizes[0] * 100 / data_->header.num_entries);
    CACHE_UMA(PERCENTAGE, "FirstLowUseRatio", 0,
              data_->header.lru.sizes[1] * 100 / data_->header.num_entries);
    CACHE_UMA(PERCENTAGE, "FirstHighUseRatio", 0,
              data_->header.lru.sizes[2] * 100 / data_->header.num_entries);
  }

  stats_.ResetRatios();
}

void BackendImpl::CriticalError(int error) {
  LOG(ERROR) << "Critical error found " << error;
  if (disabled_)
    return;

  stats_.OnEvent(Stats::FATAL_ERROR);
  LogStats();
  ReportError(error);

  // Setting the index table length to an invalid value will force re-creation
  // of the cache files.
  data_->header.table_len = 1;
  disabled_ = true;

  if (!num_refs_)
    MessageLoop::current()->PostTask(FROM_HERE,
        factory_.NewRunnableMethod(&BackendImpl::RestartCache, true));
}

void BackendImpl::ReportError(int error) {
  // We transmit positive numbers, instead of direct error codes.
  DCHECK_LE(error, 0);
  CACHE_UMA(CACHE_ERROR, "Error", 0, error * -1);
}

void BackendImpl::OnEvent(Stats::Counters an_event) {
  stats_.OnEvent(an_event);
}

void BackendImpl::OnRead(int32 bytes) {
  DCHECK_GE(bytes, 0);
  byte_count_ += bytes;
  if (byte_count_ < 0)
    byte_count_ = kint32max;
}

void BackendImpl::OnWrite(int32 bytes) {
  // We use the same implementation as OnRead... just log the number of bytes.
  OnRead(bytes);
}

void BackendImpl::OnStatsTimer() {
  stats_.OnEvent(Stats::TIMER);
  int64 time = stats_.GetCounter(Stats::TIMER);
  int64 current = stats_.GetCounter(Stats::OPEN_ENTRIES);

  // OPEN_ENTRIES is a sampled average of the number of open entries, avoiding
  // the bias towards 0.
  if (num_refs_ && (current != num_refs_)) {
    int64 diff = (num_refs_ - current) / 50;
    if (!diff)
      diff = num_refs_ > current ? 1 : -1;
    current = current + diff;
    stats_.SetCounter(Stats::OPEN_ENTRIES, current);
    stats_.SetCounter(Stats::MAX_ENTRIES, max_refs_);
  }

  CACHE_UMA(COUNTS, "NumberOfReferences", 0, num_refs_);

  CACHE_UMA(COUNTS_10000, "EntryAccessRate", 0, entry_count_);
  CACHE_UMA(COUNTS, "ByteIORate", 0, byte_count_ / 1024);
  entry_count_ = 0;
  byte_count_ = 0;

  if (!data_)
    first_timer_ = false;
  if (first_timer_) {
    first_timer_ = false;
    if (ShouldReportAgain())
      ReportStats();
  }

  // Save stats to disk at 5 min intervals.
  if (time % 10 == 0)
    stats_.Store();
}

void BackendImpl::IncrementIoCount() {
  num_pending_io_++;
}

void BackendImpl::DecrementIoCount() {
  num_pending_io_--;
}

void BackendImpl::SetUnitTestMode() {
  user_flags_ |= kUnitTestMode;
  unit_test_ = true;
}

void BackendImpl::SetUpgradeMode() {
  user_flags_ |= kUpgradeMode;
  read_only_ = true;
}

void BackendImpl::SetNewEviction() {
  user_flags_ |= kNewEviction;
  new_eviction_ = true;
}

void BackendImpl::SetFlags(uint32 flags) {
  user_flags_ |= flags;
}

void BackendImpl::ClearRefCountForTest() {
  num_refs_ = 0;
}

int BackendImpl::FlushQueueForTest(CompletionCallback* callback) {
  background_queue_.FlushQueue(callback);
  return net::ERR_IO_PENDING;
}

int BackendImpl::RunTaskForTest(Task* task, CompletionCallback* callback) {
  background_queue_.RunTask(task, callback);
  return net::ERR_IO_PENDING;
}

void BackendImpl::TrimForTest(bool empty) {
  eviction_.SetTestMode();
  eviction_.TrimCache(empty);
}

void BackendImpl::TrimDeletedListForTest(bool empty) {
  eviction_.SetTestMode();
  eviction_.TrimDeletedList(empty);
}

int BackendImpl::SelfCheck() {
  if (!init_) {
    LOG(ERROR) << "Init failed";
    return ERR_INIT_FAILED;
  }

  int num_entries = rankings_.SelfCheck();
  if (num_entries < 0) {
    LOG(ERROR) << "Invalid rankings list, error " << num_entries;
    return num_entries;
  }

  if (num_entries != data_->header.num_entries) {
    LOG(ERROR) << "Number of entries mismatch";
    return ERR_NUM_ENTRIES_MISMATCH;
  }

  return CheckAllEntries();
}

// ------------------------------------------------------------------------

int32 BackendImpl::GetEntryCount() const {
  if (!index_ || disabled_)
    return 0;
  // num_entries includes entries already evicted.
  int32 not_deleted = data_->header.num_entries -
                      data_->header.lru.sizes[Rankings::DELETED];

  if (not_deleted < 0) {
    NOTREACHED();
    not_deleted = 0;
  }

  return not_deleted;
}

int BackendImpl::OpenEntry(const std::string& key, Entry** entry,
                           CompletionCallback* callback) {
  DCHECK(callback);
  background_queue_.OpenEntry(key, entry, callback);
  return net::ERR_IO_PENDING;
}

int BackendImpl::CreateEntry(const std::string& key, Entry** entry,
                             CompletionCallback* callback) {
  DCHECK(callback);
  background_queue_.CreateEntry(key, entry, callback);
  return net::ERR_IO_PENDING;
}

int BackendImpl::DoomEntry(const std::string& key,
                           CompletionCallback* callback) {
  DCHECK(callback);
  background_queue_.DoomEntry(key, callback);
  return net::ERR_IO_PENDING;
}

int BackendImpl::DoomAllEntries(CompletionCallback* callback) {
  DCHECK(callback);
  background_queue_.DoomAllEntries(callback);
  return net::ERR_IO_PENDING;
}

int BackendImpl::DoomEntriesBetween(const base::Time initial_time,
                                    const base::Time end_time,
                                    CompletionCallback* callback) {
  DCHECK(callback);
  background_queue_.DoomEntriesBetween(initial_time, end_time, callback);
  return net::ERR_IO_PENDING;
}

int BackendImpl::DoomEntriesSince(const base::Time initial_time,
                                  CompletionCallback* callback) {
  DCHECK(callback);
  background_queue_.DoomEntriesSince(initial_time, callback);
  return net::ERR_IO_PENDING;
}

int BackendImpl::OpenNextEntry(void** iter, Entry** next_entry,
                               CompletionCallback* callback) {
  DCHECK(callback);
  background_queue_.OpenNextEntry(iter, next_entry, callback);
  return net::ERR_IO_PENDING;
}

void BackendImpl::EndEnumeration(void** iter) {
  background_queue_.EndEnumeration(*iter);
  *iter = NULL;
}

void BackendImpl::GetStats(StatsItems* stats) {
  if (disabled_)
    return;

  std::pair<std::string, std::string> item;

  item.first = "Entries";
  item.second = base::StringPrintf("%d", data_->header.num_entries);
  stats->push_back(item);

  item.first = "Pending IO";
  item.second = base::StringPrintf("%d", num_pending_io_);
  stats->push_back(item);

  item.first = "Max size";
  item.second = base::StringPrintf("%d", max_size_);
  stats->push_back(item);

  item.first = "Current size";
  item.second = base::StringPrintf("%d", data_->header.num_bytes);
  stats->push_back(item);

  stats_.GetItems(stats);
}

// ------------------------------------------------------------------------

// We just created a new file so we're going to write the header and set the
// file length to include the hash table (zero filled).
bool BackendImpl::CreateBackingStore(disk_cache::File* file) {
  AdjustMaxCacheSize(0);

  IndexHeader header;
  header.table_len = DesiredIndexTableLen(max_size_);

  // We need file version 2.1 for the new eviction algorithm.
  if (new_eviction_)
    header.version = 0x20001;

  header.create_time = Time::Now().ToInternalValue();

  if (!file->Write(&header, sizeof(header), 0))
    return false;

  return file->SetLength(GetIndexSize(header.table_len));
}

bool BackendImpl::InitBackingStore(bool* file_created) {
  file_util::CreateDirectory(path_);

  FilePath index_name = path_.AppendASCII(kIndexName);

  int flags = base::PLATFORM_FILE_READ |
              base::PLATFORM_FILE_WRITE |
              base::PLATFORM_FILE_OPEN_ALWAYS |
              base::PLATFORM_FILE_EXCLUSIVE_WRITE;
  scoped_refptr<disk_cache::File> file(new disk_cache::File(
      base::CreatePlatformFile(index_name, flags, file_created, NULL)));

  if (!file->IsValid())
    return false;

  bool ret = true;
  if (*file_created)
    ret = CreateBackingStore(file);

  file = NULL;
  if (!ret)
    return false;

  index_ = new MappedFile();
  data_ = reinterpret_cast<Index*>(index_->Init(index_name, 0));
  if (!data_) {
    LOG(ERROR) << "Unable to map Index file";
    return false;
  }

  if (index_->GetLength() < sizeof(Index)) {
    // We verify this again on CheckIndex() but it's easier to make sure now
    // that the header is there.
    LOG(ERROR) << "Corrupt Index file";
    return false;
  }

  return true;
}

// The maximum cache size will be either set explicitly by the caller, or
// calculated by this code.
void BackendImpl::AdjustMaxCacheSize(int table_len) {
  if (max_size_)
    return;

  // If table_len is provided, the index file exists.
  DCHECK(!table_len || data_->header.magic);

  // The user is not setting the size, let's figure it out.
  int64 available = base::SysInfo::AmountOfFreeDiskSpace(path_);
  if (available < 0) {
    max_size_ = kDefaultCacheSize;
    return;
  }

  if (table_len)
    available += data_->header.num_bytes;

  max_size_ = PreferedCacheSize(available);

  // Let's not use more than the default size while we tune-up the performance
  // of bigger caches. TODO(rvargas): remove this limit.
  if (max_size_ > kDefaultCacheSize * 4)
    max_size_ = kDefaultCacheSize * 4;

  if (!table_len)
    return;

  // If we already have a table, adjust the size to it.
  int current_max_size = MaxStorageSizeForTable(table_len);
  if (max_size_ > current_max_size)
    max_size_= current_max_size;
}

void BackendImpl::RestartCache(bool failure) {
  int64 errors = stats_.GetCounter(Stats::FATAL_ERROR);
  int64 full_dooms = stats_.GetCounter(Stats::DOOM_CACHE);
  int64 partial_dooms = stats_.GetCounter(Stats::DOOM_RECENT);
  int64 last_report = stats_.GetCounter(Stats::LAST_REPORT);

  PrepareForRestart();
  if (failure) {
    DCHECK(!num_refs_);
    DCHECK(!open_entries_.size());
    DelayedCacheCleanup(path_);
  } else {
    DeleteCache(path_, false);
  }

  // Don't call Init() if directed by the unit test: we are simulating a failure
  // trying to re-enable the cache.
  if (unit_test_)
    init_ = true;  // Let the destructor do proper cleanup.
  else if (SyncInit() == net::OK) {
    stats_.SetCounter(Stats::FATAL_ERROR, errors);
    stats_.SetCounter(Stats::DOOM_CACHE, full_dooms);
    stats_.SetCounter(Stats::DOOM_RECENT, partial_dooms);
    stats_.SetCounter(Stats::LAST_REPORT, last_report);
  }
}

void BackendImpl::PrepareForRestart() {
  // Reset the mask_ if it was not given by the user.
  if (!(user_flags_ & kMask))
    mask_ = 0;

  if (!(user_flags_ & kNewEviction))
    new_eviction_ = false;

  disabled_ = true;
  data_->header.crash = 0;
  index_ = NULL;
  data_ = NULL;
  block_files_.CloseFiles();
  rankings_.Reset();
  init_ = false;
  restarted_ = true;
}

int BackendImpl::NewEntry(Addr address, EntryImpl** entry) {
  EntriesMap::iterator it = open_entries_.find(address.value());
  if (it != open_entries_.end()) {
    // Easy job. This entry is already in memory.
    EntryImpl* this_entry = it->second;
    this_entry->AddRef();
    *entry = this_entry;
    return 0;
  }

  scoped_refptr<EntryImpl> cache_entry(
      new EntryImpl(this, address, read_only_));
  IncreaseNumRefs();
  *entry = NULL;

  if (!address.is_initialized() || address.is_separate_file() ||
      address.file_type() != BLOCK_256) {
    LOG(WARNING) << "Wrong entry address.";
    return ERR_INVALID_ADDRESS;
  }

  TimeTicks start = TimeTicks::Now();
  if (!cache_entry->entry()->Load())
    return ERR_READ_FAILURE;

  if (IsLoaded()) {
    CACHE_UMA(AGE_MS, "LoadTime", GetSizeGroup(), start);
  }

  if (!cache_entry->SanityCheck()) {
    LOG(WARNING) << "Messed up entry found.";
    return ERR_INVALID_ENTRY;
  }

  if (!cache_entry->LoadNodeAddress())
    return ERR_READ_FAILURE;

  // Prevent overwriting the dirty flag on the destructor.
  cache_entry->SetDirtyFlag(GetCurrentEntryId());

  if (!rankings_.SanityCheck(cache_entry->rankings(), false))
    return ERR_INVALID_LINKS;

  if (cache_entry->dirty()) {
    Trace("Dirty entry 0x%p 0x%x", reinterpret_cast<void*>(cache_entry.get()),
          address.value());
  }

  open_entries_[address.value()] = cache_entry;

  cache_entry->BeginLogging(net_log_, false);
  cache_entry.swap(entry);
  return 0;
}

EntryImpl* BackendImpl::MatchEntry(const std::string& key, uint32 hash,
                                   bool find_parent, Addr entry_addr,
                                   bool* match_error) {
  Addr address(data_->table[hash & mask_]);
  scoped_refptr<EntryImpl> cache_entry, parent_entry;
  EntryImpl* tmp = NULL;
  bool found = false;
  std::set<CacheAddr> visited;
  *match_error = false;

  for (;;) {
    if (disabled_)
      break;

    if (visited.find(address.value()) != visited.end()) {
      // It's possible for a buggy version of the code to write a loop. Just
      // break it.
      Trace("Hash collision loop 0x%x", address.value());
      address.set_value(0);
      parent_entry->SetNextAddress(address);
    }
    visited.insert(address.value());

    if (!address.is_initialized()) {
      if (find_parent)
        found = true;
      break;
    }

    int error = NewEntry(address, &tmp);
    cache_entry.swap(&tmp);

    if (error || cache_entry->dirty()) {
      // This entry is dirty on disk (it was not properly closed): we cannot
      // trust it.
      Addr child(0);
      if (!error)
        child.set_value(cache_entry->GetNextAddress());

      if (parent_entry) {
        parent_entry->SetNextAddress(child);
        parent_entry = NULL;
      } else {
        data_->table[hash & mask_] = child.value();
      }

      Trace("MatchEntry dirty %d 0x%x 0x%x", find_parent, entry_addr.value(),
            address.value());

      if (!error) {
        // It is important to call DestroyInvalidEntry after removing this
        // entry from the table.
        DestroyInvalidEntry(cache_entry);
        cache_entry = NULL;
      } else {
        Trace("NewEntry failed on MatchEntry 0x%x", address.value());
      }

      // Restart the search.
      address.set_value(data_->table[hash & mask_]);
      visited.clear();
      continue;
    }

    DCHECK_EQ(hash & mask_, cache_entry->entry()->Data()->hash & mask_);
    if (cache_entry->IsSameEntry(key, hash)) {
      if (!cache_entry->Update())
        cache_entry = NULL;
      found = true;
      if (find_parent && entry_addr.value() != address.value()) {
        Trace("Entry not on the index 0x%x", address.value());
        *match_error = true;
        parent_entry = NULL;
      }
      break;
    }
    if (!cache_entry->Update())
      cache_entry = NULL;
    parent_entry = cache_entry;
    cache_entry = NULL;
    if (!parent_entry)
      break;

    address.set_value(parent_entry->GetNextAddress());
  }

  if (parent_entry && (!find_parent || !found))
    parent_entry = NULL;

  if (find_parent && entry_addr.is_initialized() && !cache_entry) {
    *match_error = true;
    parent_entry = NULL;
  }

  if (cache_entry && (find_parent || !found))
    cache_entry = NULL;

  find_parent ? parent_entry.swap(&tmp) : cache_entry.swap(&tmp);
  return tmp;
}

// This is the actual implementation for OpenNextEntry and OpenPrevEntry.
EntryImpl* BackendImpl::OpenFollowingEntry(bool forward, void** iter) {
  if (disabled_)
    return NULL;

  DCHECK(iter);

  const int kListsToSearch = 3;
  scoped_refptr<EntryImpl> entries[kListsToSearch];
  scoped_ptr<Rankings::Iterator> iterator(
      reinterpret_cast<Rankings::Iterator*>(*iter));
  *iter = NULL;

  if (!iterator.get()) {
    iterator.reset(new Rankings::Iterator(&rankings_));
    bool ret = false;

    // Get an entry from each list.
    for (int i = 0; i < kListsToSearch; i++) {
      EntryImpl* temp = NULL;
      ret |= OpenFollowingEntryFromList(forward, static_cast<Rankings::List>(i),
                                        &iterator->nodes[i], &temp);
      entries[i].swap(&temp);  // The entry was already addref'd.
    }
    if (!ret)
      return NULL;
  } else {
    // Get the next entry from the last list, and the actual entries for the
    // elements on the other lists.
    for (int i = 0; i < kListsToSearch; i++) {
      EntryImpl* temp = NULL;
      if (iterator->list == i) {
          OpenFollowingEntryFromList(forward, iterator->list,
                                     &iterator->nodes[i], &temp);
      } else {
        temp = GetEnumeratedEntry(iterator->nodes[i]);
      }

      entries[i].swap(&temp);  // The entry was already addref'd.
    }
  }

  int newest = -1;
  int oldest = -1;
  Time access_times[kListsToSearch];
  for (int i = 0; i < kListsToSearch; i++) {
    if (entries[i].get()) {
      access_times[i] = entries[i]->GetLastUsed();
      if (newest < 0) {
        DCHECK_LT(oldest, 0);
        newest = oldest = i;
        continue;
      }
      if (access_times[i] > access_times[newest])
        newest = i;
      if (access_times[i] < access_times[oldest])
        oldest = i;
    }
  }

  if (newest < 0 || oldest < 0)
    return NULL;

  EntryImpl* next_entry;
  if (forward) {
    next_entry = entries[newest].release();
    iterator->list = static_cast<Rankings::List>(newest);
  } else {
    next_entry = entries[oldest].release();
    iterator->list = static_cast<Rankings::List>(oldest);
  }

  *iter = iterator.release();
  return next_entry;
}

bool BackendImpl::OpenFollowingEntryFromList(bool forward, Rankings::List list,
                                             CacheRankingsBlock** from_entry,
                                             EntryImpl** next_entry) {
  if (disabled_)
    return false;

  if (!new_eviction_ && Rankings::NO_USE != list)
    return false;

  Rankings::ScopedRankingsBlock rankings(&rankings_, *from_entry);
  CacheRankingsBlock* next_block = forward ?
      rankings_.GetNext(rankings.get(), list) :
      rankings_.GetPrev(rankings.get(), list);
  Rankings::ScopedRankingsBlock next(&rankings_, next_block);
  *from_entry = NULL;

  *next_entry = GetEnumeratedEntry(next.get());
  if (!*next_entry)
    return false;

  *from_entry = next.release();
  return true;
}

EntryImpl* BackendImpl::GetEnumeratedEntry(CacheRankingsBlock* next) {
  if (!next || disabled_)
    return NULL;

  EntryImpl* entry;
  if (NewEntry(Addr(next->Data()->contents), &entry)) {
    // TODO(rvargas) bug 73102: We should remove this node from the list, and
    // maybe do a better cleanup.
    return NULL;
  }

  if (entry->dirty()) {
    // We cannot trust this entry.
    InternalDoomEntry(entry);
    entry->Release();
    return NULL;
  }

  if (!entry->Update()) {
    entry->Release();
    return NULL;
  }

  // Note that it is unfortunate (but possible) for this entry to be clean, but
  // not actually the real entry. In other words, we could have lost this entry
  // from the index, and it could have been replaced with a newer one. It's not
  // worth checking that this entry is "the real one", so we just return it and
  // let the enumeration continue; this entry will be evicted at some point, and
  // the regular path will work with the real entry. With time, this problem
  // will disasappear because this scenario is just a bug.

  // Make sure that we save the key for later.
  entry->GetKey();

  return entry;
}

EntryImpl* BackendImpl::ResurrectEntry(EntryImpl* deleted_entry) {
  if (ENTRY_NORMAL == deleted_entry->entry()->Data()->state) {
    deleted_entry->Release();
    stats_.OnEvent(Stats::CREATE_MISS);
    Trace("create entry miss ");
    return NULL;
  }

  // We are attempting to create an entry and found out that the entry was
  // previously deleted.

  eviction_.OnCreateEntry(deleted_entry);
  entry_count_++;

  stats_.OnEvent(Stats::RESURRECT_HIT);
  Trace("Resurrect entry hit ");
  return deleted_entry;
}

void BackendImpl::DestroyInvalidEntry(EntryImpl* entry) {
  LOG(WARNING) << "Destroying invalid entry.";
  Trace("Destroying invalid entry 0x%p", entry);

  entry->SetPointerForInvalidEntry(GetCurrentEntryId());

  eviction_.OnDoomEntry(entry);
  entry->InternalDoom();

  if (!new_eviction_)
    DecreaseNumEntries();
  stats_.OnEvent(Stats::INVALID_ENTRY);
}

void BackendImpl::AddStorageSize(int32 bytes) {
  data_->header.num_bytes += bytes;
  DCHECK_GE(data_->header.num_bytes, 0);
}

void BackendImpl::SubstractStorageSize(int32 bytes) {
  data_->header.num_bytes -= bytes;
  DCHECK_GE(data_->header.num_bytes, 0);
}

void BackendImpl::IncreaseNumRefs() {
  num_refs_++;
  if (max_refs_ < num_refs_)
    max_refs_ = num_refs_;
}

void BackendImpl::DecreaseNumRefs() {
  DCHECK(num_refs_);
  num_refs_--;

  if (!num_refs_ && disabled_)
    MessageLoop::current()->PostTask(FROM_HERE,
        factory_.NewRunnableMethod(&BackendImpl::RestartCache, true));
}

void BackendImpl::IncreaseNumEntries() {
  data_->header.num_entries++;
  DCHECK_GT(data_->header.num_entries, 0);
}

void BackendImpl::DecreaseNumEntries() {
  data_->header.num_entries--;
  if (data_->header.num_entries < 0) {
    NOTREACHED();
    data_->header.num_entries = 0;
  }
}

void BackendImpl::LogStats() {
  StatsItems stats;
  GetStats(&stats);

  for (size_t index = 0; index < stats.size(); index++)
    VLOG(1) << stats[index].first << ": " << stats[index].second;
}

void BackendImpl::ReportStats() {
  CACHE_UMA(COUNTS, "Entries", 0, data_->header.num_entries);

  int current_size = data_->header.num_bytes / (1024 * 1024);
  int max_size = max_size_ / (1024 * 1024);
  CACHE_UMA(COUNTS_10000, "Size2", 0, current_size);
  CACHE_UMA(COUNTS_10000, "MaxSize2", 0, max_size);
  if (!max_size)
    max_size++;
  CACHE_UMA(PERCENTAGE, "UsedSpace", 0, current_size * 100 / max_size);

  CACHE_UMA(COUNTS_10000, "AverageOpenEntries2", 0,
            static_cast<int>(stats_.GetCounter(Stats::OPEN_ENTRIES)));
  CACHE_UMA(COUNTS_10000, "MaxOpenEntries2", 0,
            static_cast<int>(stats_.GetCounter(Stats::MAX_ENTRIES)));
  stats_.SetCounter(Stats::MAX_ENTRIES, 0);

  CACHE_UMA(COUNTS_10000, "TotalFatalErrors", 0,
            static_cast<int>(stats_.GetCounter(Stats::FATAL_ERROR)));
  CACHE_UMA(COUNTS_10000, "TotalDoomCache", 0,
            static_cast<int>(stats_.GetCounter(Stats::DOOM_CACHE)));
  CACHE_UMA(COUNTS_10000, "TotalDoomRecentEntries", 0,
            static_cast<int>(stats_.GetCounter(Stats::DOOM_RECENT)));
  stats_.SetCounter(Stats::FATAL_ERROR, 0);
  stats_.SetCounter(Stats::DOOM_CACHE, 0);
  stats_.SetCounter(Stats::DOOM_RECENT, 0);

  int64 total_hours = stats_.GetCounter(Stats::TIMER) / 120;
  if (!data_->header.create_time || !data_->header.lru.filled) {
    int cause = data_->header.create_time ? 0 : 1;
    if (!data_->header.lru.filled)
      cause |= 2;
    CACHE_UMA(CACHE_ERROR, "ShortReport", 0, cause);
    CACHE_UMA(HOURS, "TotalTimeNotFull", 0, static_cast<int>(total_hours));
    return;
  }

  // This is an up to date client that will report FirstEviction() data. After
  // that event, start reporting this:

  CACHE_UMA(HOURS, "TotalTime", 0, static_cast<int>(total_hours));

  int64 use_hours = stats_.GetCounter(Stats::LAST_REPORT_TIMER) / 120;
  stats_.SetCounter(Stats::LAST_REPORT_TIMER, stats_.GetCounter(Stats::TIMER));

  // We may see users with no use_hours at this point if this is the first time
  // we are running this code.
  if (use_hours)
    use_hours = total_hours - use_hours;

  if (!use_hours || !GetEntryCount() || !data_->header.num_bytes)
    return;

  CACHE_UMA(HOURS, "UseTime", 0, static_cast<int>(use_hours));
  CACHE_UMA(PERCENTAGE, "HitRatio", data_->header.experiment,
            stats_.GetHitRatio());

  int64 trim_rate = stats_.GetCounter(Stats::TRIM_ENTRY) / use_hours;
  CACHE_UMA(COUNTS, "TrimRate", 0, static_cast<int>(trim_rate));

  int avg_size = data_->header.num_bytes / GetEntryCount();
  CACHE_UMA(COUNTS, "EntrySize", 0, avg_size);
  CACHE_UMA(COUNTS, "EntriesFull", 0, data_->header.num_entries);

  CACHE_UMA(PERCENTAGE, "IndexLoad", 0,
            data_->header.num_entries * 100 / (mask_ + 1));

  int large_entries_bytes = stats_.GetLargeEntriesSize();
  int large_ratio = large_entries_bytes * 100 / data_->header.num_bytes;
  CACHE_UMA(PERCENTAGE, "LargeEntriesRatio", 0, large_ratio);

  if (new_eviction_) {
    CACHE_UMA(PERCENTAGE, "ResurrectRatio", data_->header.experiment,
              stats_.GetResurrectRatio());
    CACHE_UMA(PERCENTAGE, "NoUseRatio", 0,
              data_->header.lru.sizes[0] * 100 / data_->header.num_entries);
    CACHE_UMA(PERCENTAGE, "LowUseRatio", 0,
              data_->header.lru.sizes[1] * 100 / data_->header.num_entries);
    CACHE_UMA(PERCENTAGE, "HighUseRatio", 0,
              data_->header.lru.sizes[2] * 100 / data_->header.num_entries);
    CACHE_UMA(PERCENTAGE, "DeletedRatio", data_->header.experiment,
              data_->header.lru.sizes[4] * 100 / data_->header.num_entries);
  }

  stats_.ResetRatios();
  stats_.SetCounter(Stats::TRIM_ENTRY, 0);

  if (cache_type_ == net::DISK_CACHE)
    block_files_.ReportStats();
}

void BackendImpl::UpgradeTo2_1() {
  // 2.1 is basically the same as 2.0, except that new fields are actually
  // updated by the new eviction algorithm.
  DCHECK(0x20000 == data_->header.version);
  data_->header.version = 0x20001;
  data_->header.lru.sizes[Rankings::NO_USE] = data_->header.num_entries;
}

bool BackendImpl::CheckIndex() {
  DCHECK(data_);

  size_t current_size = index_->GetLength();
  if (current_size < sizeof(Index)) {
    LOG(ERROR) << "Corrupt Index file";
    return false;
  }

  if (new_eviction_) {
    // We support versions 2.0 and 2.1, upgrading 2.0 to 2.1.
    if (kIndexMagic != data_->header.magic ||
        kCurrentVersion >> 16 != data_->header.version >> 16) {
      LOG(ERROR) << "Invalid file version or magic";
      return false;
    }
    if (kCurrentVersion == data_->header.version) {
      // We need file version 2.1 for the new eviction algorithm.
      UpgradeTo2_1();
    }
  } else {
    if (kIndexMagic != data_->header.magic ||
        kCurrentVersion != data_->header.version) {
      LOG(ERROR) << "Invalid file version or magic";
      return false;
    }
  }

  if (!data_->header.table_len) {
    LOG(ERROR) << "Invalid table size";
    return false;
  }

  if (current_size < GetIndexSize(data_->header.table_len) ||
      data_->header.table_len & (kBaseTableLen - 1)) {
    LOG(ERROR) << "Corrupt Index file";
    return false;
  }

  AdjustMaxCacheSize(data_->header.table_len);

  if (data_->header.num_bytes < 0 ||
      (max_size_ < kint32max - kDefaultCacheSize &&
       data_->header.num_bytes > max_size_ + kDefaultCacheSize)) {
    LOG(ERROR) << "Invalid cache (current) size";
    return false;
  }

  if (data_->header.num_entries < 0) {
    LOG(ERROR) << "Invalid number of entries";
    return false;
  }

  if (!mask_)
    mask_ = data_->header.table_len - 1;

  // Load the table into memory with a single read.
  scoped_array<char> buf(new char[current_size]);
  return index_->Read(buf.get(), current_size, 0);
}

int BackendImpl::CheckAllEntries() {
  int num_dirty = 0;
  int num_entries = 0;
  DCHECK(mask_ < kuint32max);
  for (int i = 0; i <= static_cast<int>(mask_); i++) {
    Addr address(data_->table[i]);
    if (!address.is_initialized())
      continue;
    for (;;) {
      EntryImpl* tmp;
      int ret = NewEntry(address, &tmp);
      if (ret)
        return ret;
      scoped_refptr<EntryImpl> cache_entry;
      cache_entry.swap(&tmp);

      if (cache_entry->dirty())
        num_dirty++;
      else if (CheckEntry(cache_entry.get()))
        num_entries++;
      else
        return ERR_INVALID_ENTRY;

      address.set_value(cache_entry->GetNextAddress());
      if (!address.is_initialized())
        break;
    }
  }

  Trace("CheckAllEntries End");
  if (num_entries + num_dirty != data_->header.num_entries) {
    LOG(ERROR) << "Number of entries mismatch";
    return ERR_NUM_ENTRIES_MISMATCH;
  }

  return num_dirty;
}

bool BackendImpl::CheckEntry(EntryImpl* cache_entry) {
  bool ok = block_files_.IsValid(cache_entry->entry()->address());
  ok = ok && block_files_.IsValid(cache_entry->rankings()->address());
  EntryStore* data = cache_entry->entry()->Data();
  for (size_t i = 0; i < arraysize(data->data_addr); i++) {
    if (data->data_addr[i]) {
      Addr address(data->data_addr[i]);
      if (address.is_block_file())
        ok = ok && block_files_.IsValid(address);
    }
  }

  RankingsNode* rankings = cache_entry->rankings()->Data();
  return ok && !rankings->dummy;
}

int BackendImpl::MaxBuffersSize() {
  static int64 total_memory = base::SysInfo::AmountOfPhysicalMemory();
  static bool done = false;

  if (!done) {
    const int kMaxBuffersSize = 30 * 1024 * 1024;

    // We want to use up to 2% of the computer's memory.
    total_memory = total_memory * 2 / 100;
    if (total_memory > kMaxBuffersSize || total_memory <= 0)
      total_memory = kMaxBuffersSize;

    done = true;
  }

  return static_cast<int>(total_memory);
}

}  // namespace disk_cache
