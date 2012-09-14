// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/infinite_cache.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/hash.h"
#include "base/hash_tables.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram.h"
#include "base/pickle.h"
#include "base/platform_file.h"
#include "base/rand_util.h"
#include "base/sha1.h"
#include "base/time.h"
#include "base/threading/sequenced_worker_pool.h"
#include "net/base/net_errors.h"
#include "net/http/http_cache_transaction.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/http/http_util.h"
#include "third_party/zlib/zlib.h"

using base::PlatformFile;
using base::Time;
using base::TimeDelta;

namespace {

// Flags to use with a particular resource.
enum Flags {
  NO_CACHE = 1 << 0,
  NO_STORE = 1 << 1,
  EXPIRED = 1 << 2,
  TRUNCATED = 1 << 3,
  RESUMABLE = 1 << 4,
  REVALIDATEABLE = 1 << 5,
  DOOM_METHOD = 1 << 6,
  CACHED = 1 << 7
};

const int kKeySizeBytes = 20;
COMPILE_ASSERT(base::kSHA1Length == static_cast<unsigned>(kKeySizeBytes),
               invalid_key_length);
struct Key {
  char value[kKeySizeBytes];
};

// The actual data that we store for every resource.
struct Details {
  int32 expiration;
  int32 last_access;
  uint16 flags;
  uint8 use_count;
  uint8 update_count;
  uint32 vary_hash;
  int32 headers_size;
  int32 response_size;
  uint32 headers_hash;
  uint32 response_hash;
};
const size_t kRecordSize = sizeof(Key) + sizeof(Details);

// Some constants related to the database file.
uint32 kMagicSignature = 0x1f00cace;
uint32 kCurrentVersion = 0x10001;

// Basic limits for the experiment.
int kMaxNumEntries = 200 * 1000;
int kMaxTrackingSize = 40 * 1024 * 1024;

// Settings that control how we generate histograms.
int kTimerMinutes = 5;
int kReportSizeStep = 100 * 1024 * 1024;

// Buffer to read and write the file.
const size_t kBufferSize = 1024 * 1024;
const size_t kMaxRecordsToRead = kBufferSize / kRecordSize;
COMPILE_ASSERT(kRecordSize * kMaxRecordsToRead < kBufferSize, wrong_buffer);

// Functor for operator <.
struct Key_less {
  bool operator()(const Key& left, const Key& right) const {
    // left < right.
    return (memcmp(left.value, right.value, kKeySizeBytes) < 0);
  }
};

// Functor for operator ==.
struct Key_eq {
  bool operator()(const Key& left, const Key& right) const {
    return (memcmp(left.value, right.value, kKeySizeBytes) == 0);
  }
};

// Simple adaptor for the sha1 interface.
void CryptoHash(std::string source, Key* destination) {
  base::SHA1HashBytes(reinterpret_cast<const unsigned char*>(source.data()),
                      source.size(),
                      reinterpret_cast<unsigned char*>(destination->value));
}

// Simple adaptor for base::ReadPlatformFile.
bool ReadPlatformFile(PlatformFile file, size_t offset,
                      void* buffer, size_t buffer_len) {
  DCHECK_LE(offset, static_cast<size_t>(kuint32max));
  int bytes = base::ReadPlatformFile(file, static_cast<int64>(offset),
                                     reinterpret_cast<char*>(buffer),
                                     static_cast<int>(buffer_len));
  return (bytes == static_cast<int>(buffer_len));
}

// Simple adaptor for base::WritePlatformFile.
bool WritePlatformFile(PlatformFile file, size_t offset,
                       const void* buffer, size_t buffer_len) {
  DCHECK_LE(offset, static_cast<size_t>(kuint32max));
  int bytes = base::WritePlatformFile(file, static_cast<int64>(offset),
                                      reinterpret_cast<const char*>(buffer),
                                      static_cast<int>(buffer_len));
  return (bytes == static_cast<int>(buffer_len));
}

// 1 second resolution, +- 68 years from the baseline.
int32 TimeToInt(Time time) {
  int64 seconds = (time - Time::UnixEpoch()).InSeconds();
  if (seconds > kint32max)
    seconds = kint32max;
  if (seconds < kint32min)
    seconds = kint32min;
  return static_cast<int32>(seconds);
}

Time IntToTime(int32 time) {
  return Time::UnixEpoch() + TimeDelta::FromSeconds(time);
}

int32 GetExpiration(const net::HttpResponseInfo* response) {
  TimeDelta freshness =
      response->headers->GetFreshnessLifetime(response->response_time);

  // Avoid overflow when adding to current time.
  if (freshness.InDays() > 365 * 10)
    freshness = TimeDelta::FromDays(365 * 10);
  return TimeToInt(response->response_time + freshness);
}

uint32 GetCacheability(const net::HttpResponseInfo* response) {
  uint32 cacheability = 0;
  const net::HttpResponseHeaders* headers = response->headers;
  if (headers->HasHeaderValue("cache-control", "no-cache") ||
      headers->HasHeaderValue("pragma", "no-cache") ||
      headers->HasHeaderValue("vary", "*")) {
    cacheability |= NO_CACHE;
  }

  if (headers->HasHeaderValue("cache-control", "no-store"))
    cacheability |= NO_STORE;

  TimeDelta max_age;
  if (headers->GetMaxAgeValue(&max_age) && max_age.InSeconds() <= 0)
    cacheability |= NO_CACHE;

  return cacheability;
}

uint32 GetRevalidationFlags(const net::HttpResponseInfo* response) {
  uint32 revalidation = 0;
  std::string etag;
  response->headers->EnumerateHeader(NULL, "etag", &etag);

  std::string last_modified;
  response->headers->EnumerateHeader(NULL, "last-modified", &last_modified);

  if (!etag.empty() || !last_modified.empty())
    revalidation = REVALIDATEABLE;

  if (response->headers->HasStrongValidators())
    revalidation = RESUMABLE;

  return revalidation;
}


uint32 GetVaryHash(const net::HttpResponseInfo* response) {
  if (!response->vary_data.is_valid())
    return 0;

  uint32 hash = adler32(0, Z_NULL, 0);
  Pickle pickle;
  response->vary_data.Persist(&pickle);
  return adler32(hash, reinterpret_cast<const Bytef*>(pickle.data()),
                 pickle.size());
}

// Adaptor for PostTaskAndReply.
void OnComplete(const net::CompletionCallback& callback, int* result) {
  callback.Run(*result);
}

}  // namespace

namespace BASE_HASH_NAMESPACE {
#if defined(COMPILER_MSVC)
inline size_t hash_value(const Key& key) {
  return base::Hash(key.value, kKeySizeBytes);
}
#elif defined(COMPILER_GCC)
template <>
struct hash<Key> {
  size_t operator()(const Key& key) const {
    return base::Hash(key.value, kKeySizeBytes);
  }
};
#endif

}  // BASE_HASH_NAMESPACE

namespace net {

struct InfiniteCacheTransaction::ResourceData {
  ResourceData() {
    memset(this, 0, sizeof(*this));
  }

  Key key;
  Details details;
};

InfiniteCacheTransaction::InfiniteCacheTransaction(InfiniteCache* cache)
    : cache_(cache->AsWeakPtr()), must_doom_entry_(false) {
}

InfiniteCacheTransaction::~InfiniteCacheTransaction() {
  Finish();
}

void InfiniteCacheTransaction::OnRequestStart(const HttpRequestInfo* request) {
  if (!cache_)
    return;

  std::string method = request->method;
  if (method == "POST" || method == "DELETE" || method == "PUT") {
    must_doom_entry_ = true;
  } else if (method != "GET") {
    cache_.reset();
    return;
  }

  resource_data_.reset(new ResourceData);
  CryptoHash(cache_->GenerateKey(request), &resource_data_->key);
}

void InfiniteCacheTransaction::OnResponseReceived(
    const HttpResponseInfo* response) {
  if (!cache_)
    return;

  Details& details = resource_data_->details;

  details.expiration = GetExpiration(response);
  details.last_access = TimeToInt(response->request_time);
  details.flags = GetCacheability(response);
  details.vary_hash = GetVaryHash(response);
  details.response_hash = adler32(0, Z_NULL, 0);  // Init the hash.

  if (!details.flags &&
      TimeToInt(response->response_time) == details.expiration) {
    details.flags = EXPIRED;
  }
  details.flags |= GetRevalidationFlags(response);

  if (must_doom_entry_)
    details.flags |= DOOM_METHOD;

  Pickle pickle;
  response->Persist(&pickle, true, false);  // Skip transient headers.
  details.headers_size = pickle.size();
  details.headers_hash = adler32(0, Z_NULL, 0);
  details.headers_hash = adler32(details.headers_hash,
                                 reinterpret_cast<const Bytef*>(pickle.data()),
                                 pickle.size());
}

void InfiniteCacheTransaction::OnDataRead(const char* data, int data_len) {
  if (!cache_)
    return;

  if (!data_len)
    return Finish();

  resource_data_->details.response_size += data_len;

  resource_data_->details.response_hash =
    adler32(resource_data_->details.response_hash,
            reinterpret_cast<const Bytef*>(data), data_len);
}

void InfiniteCacheTransaction::OnTruncatedResponse() {
  if (!cache_)
    return;

  resource_data_->details.flags |= TRUNCATED;
}

void InfiniteCacheTransaction::OnServedFromCache() {
  if (!cache_)
    return;

  resource_data_->details.flags |= CACHED;
}

void InfiniteCacheTransaction::Finish() {
  if (!cache_ || !resource_data_.get())
    return;

  if (!resource_data_->details.headers_size)
    return;

  cache_->ProcessResource(resource_data_.Pass());
  cache_.reset();
}

// ----------------------------------------------------------------------------

// This is the object that performs the bulk of the work.
// InfiniteCacheTransaction posts the transaction data to the InfiniteCache, and
// the InfiniteCache basically just forward requests to the Worker for actual
// processing.
// The Worker lives on a worker thread (basically a dedicated worker pool with
// only one thread), and flushes data to disk once every five minutes, when it
// is notified by the InfiniteCache.
// In general, there are no callbacks on completion of tasks, and the Worker can
// be as behind as it has to when processing requests.
class InfiniteCache::Worker : public base::RefCountedThreadSafe<Worker> {
 public:
  Worker() : init_(false), flushed_(false) {}

  // Construction and destruction helpers.
  void Init(const FilePath& path);
  void Cleanup();

  // Deletes all tracked data.
  void DeleteData(int* result);

  // Deletes requests between |initial_time| and |end_time|.
  void DeleteDataBetween(base::Time initial_time,
                         base::Time end_time,
                         int* result);

  // Performs the actual processing of a new transaction. Takes ownership of
  // the transaction |data|.
  void Process(scoped_ptr<InfiniteCacheTransaction::ResourceData> data);

  // Test helpers.
  void Query(int* result);
  void Flush(int* result);

  // Timer notification.
  void OnTimer();

 private:
  friend class base::RefCountedThreadSafe<Worker>;
#if defined(COMPILER_MSVC)
  typedef BASE_HASH_NAMESPACE::hash_map<
      Key, Details, BASE_HASH_NAMESPACE::hash_compare<Key, Key_less> > KeyMap;
#elif defined(COMPILER_GCC)
  typedef BASE_HASH_NAMESPACE::hash_map<
      Key, Details, BASE_HASH_NAMESPACE::hash<Key>, Key_eq> KeyMap;
#endif

  // Header for the data file. The file starts with the header, followed by
  // all the records, and a data hash at the end (just of the records, not the
  // header). Note that the header has a dedicated hash.
  struct Header {
    uint32 magic;
    uint32 version;
    int32 num_entries;
    int32 generation;
    uint64 creation_time;
    uint64 update_time;
    int64 total_size;
    int64 size_last_report;
    int32 use_minutes;
    int32 num_hits;
    int32 num_bad_hits;
    int32 num_requests;
    int32 disabled;
    uint32 header_hash;
  };

  ~Worker() {}

  // Methods to load and store data on disk.
  void LoadData();
  void StoreData();
  void InitializeData();
  bool ReadData(PlatformFile file);
  bool WriteData(PlatformFile file);
  bool ReadAndVerifyHeader(PlatformFile file);

  // Book-keeping methods.
  void Add(const Details& details);
  void Remove(const Details& details);
  void UpdateSize(int old_size, int new_size);

  // Bulk of report generation methods.
  void RecordHit(const Details& old, Details* details);
  void RecordUpdate(const Details& old, Details* details);
  void GenerateHistograms();

  // Cache logic methods.
  bool CanReuse(const Details& old, const Details& current);
  bool DataChanged(const Details& old, const Details& current);
  bool HeadersChanged(const Details& old, const Details& current);

  KeyMap map_;
  bool init_;
  bool flushed_;
  scoped_ptr<Header> header_;
  FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(Worker);
};

void InfiniteCache::Worker::Init(const FilePath& path) {
  path_ = path;
  LoadData();
}

void InfiniteCache::Worker::Cleanup() {
  if (init_)
    StoreData();

  map_.clear();
}

void InfiniteCache::Worker::DeleteData(int* result) {
  if (!init_)
    return;

  map_.clear();
  InitializeData();
  file_util::Delete(path_, false);
  *result = OK;
  UMA_HISTOGRAM_BOOLEAN("InfiniteCache.DeleteAll", true);
}

void InfiniteCache::Worker::DeleteDataBetween(base::Time initial_time,
                                              base::Time end_time,
                                              int* result) {
  if (!init_)
    return;

  for (KeyMap::iterator i = map_.begin(); i != map_.end();) {
    Time last_access = IntToTime(i->second.last_access);
    if (last_access >= initial_time && last_access <= end_time) {
      KeyMap::iterator next = i;
      ++next;
      Remove(i->second);
      map_.erase(i);
      i = next;
      continue;
    }
    ++i;
  }

  file_util::Delete(path_, false);
  StoreData();
  *result = OK;
  UMA_HISTOGRAM_BOOLEAN("InfiniteCache.DeleteAll", true);
}

void InfiniteCache::Worker::Process(
    scoped_ptr<InfiniteCacheTransaction::ResourceData> data) {
  if (!init_)
    return;

  if (data->details.response_size > kMaxTrackingSize)
    return;

  if (header_->num_entries == kMaxNumEntries)
    return;

  header_->num_requests++;
  KeyMap::iterator i = map_.find(data->key);
  if (i != map_.end()) {
    if (data->details.flags & DOOM_METHOD) {
      Remove(i->second);
      map_.erase(i);
      return;
    }
    data->details.use_count = i->second.use_count;
    data->details.update_count = i->second.update_count;
    if (data->details.flags & CACHED) {
      RecordHit(i->second, &data->details);
    } else {
      bool reused = CanReuse(i->second, data->details);
      bool data_changed = DataChanged(i->second, data->details);
      bool headers_changed = HeadersChanged(i->second, data->details);

      if (reused && data_changed)
        header_->num_bad_hits++;

      if (reused)
        RecordHit(i->second, &data->details);

      if (headers_changed)
        UMA_HISTOGRAM_BOOLEAN("InfiniteCache.HeadersChange", true);

      if (data_changed)
        RecordUpdate(i->second, &data->details);
    }

    if (data->details.flags & NO_STORE) {
      Remove(i->second);
      map_.erase(i);
      return;
    }

    map_[data->key] = data->details;
    return;
  }

  if (data->details.flags & NO_STORE)
    return;

  if (data->details.flags & DOOM_METHOD)
    return;

  map_[data->key] = data->details;
  Add(data->details);
}

void InfiniteCache::Worker::LoadData() {
  if (path_.empty())
    return InitializeData();;

  PlatformFile file = base::CreatePlatformFile(
      path_, base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ, NULL, NULL);
  if (file == base::kInvalidPlatformFileValue)
    return InitializeData();
  if (!ReadData(file))
    InitializeData();
  base::ClosePlatformFile(file);
  if (header_->disabled)
    map_.clear();
}

void InfiniteCache::Worker::StoreData() {
  if (!init_ || flushed_ || path_.empty())
    return;

  header_->update_time = Time::Now().ToInternalValue();
  header_->generation++;
  header_->header_hash = base::Hash(
      reinterpret_cast<char*>(header_.get()), offsetof(Header, header_hash));

  FilePath temp_file = path_.ReplaceExtension(FILE_PATH_LITERAL("tmp"));
  PlatformFile file = base::CreatePlatformFile(
      temp_file, base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_WRITE,
      NULL, NULL);
  if (file == base::kInvalidPlatformFileValue)
    return;
  bool success = WriteData(file);
  base::ClosePlatformFile(file);
  if (success) {
    if (!file_util::ReplaceFile(temp_file, path_))
      file_util::Delete(temp_file, false);
  } else {
    LOG(ERROR) << "Failed to write experiment data";
  }
}

void InfiniteCache::Worker::InitializeData() {
  header_.reset(new Header);
  memset(header_.get(), 0, sizeof(Header));
  header_->magic = kMagicSignature;
  header_->version = kCurrentVersion;
  header_->creation_time = Time::Now().ToInternalValue();

  UMA_HISTOGRAM_BOOLEAN("InfiniteCache.Initialize", true);
  init_ = true;
}

bool InfiniteCache::Worker::ReadData(PlatformFile file) {
  if (!ReadAndVerifyHeader(file))
    return false;

  scoped_array<char> buffer(new char[kBufferSize]);
  size_t offset = sizeof(Header);
  uint32 hash = adler32(0, Z_NULL, 0);

  for (int remaining_records = header_->num_entries; remaining_records;) {
    int num_records = std::min(header_->num_entries,
                               static_cast<int>(kMaxRecordsToRead));
    size_t num_bytes = num_records * kRecordSize;
    remaining_records -= num_records;
    if (!remaining_records)
      num_bytes += sizeof(uint32);  // Trailing hash.
    DCHECK_LE(num_bytes, kBufferSize);

    if (!ReadPlatformFile(file, offset, buffer.get(), num_bytes))
      return false;

    hash = adler32(hash, reinterpret_cast<const Bytef*>(buffer.get()),
                   num_records * kRecordSize);
    if (!remaining_records &&
        hash != *reinterpret_cast<uint32*>(buffer.get() +
                                           num_records * kRecordSize)) {
      return false;
    }

    for (int i = 0; i < num_records; i++) {
      char* record = buffer.get() + i * kRecordSize;
      Key key = *reinterpret_cast<Key*>(record);
      Details details = *reinterpret_cast<Details*>(record + sizeof(key));
      map_[key] = details;
    }
    offset += num_bytes;
  }
  if (header_->num_entries != static_cast<int>(map_.size())) {
    NOTREACHED();
    return false;
  }

  init_ = true;
  return true;
}

bool InfiniteCache::Worker::WriteData(PlatformFile file) {
  if (!base::TruncatePlatformFile(file, 0))
    return false;

  if (!WritePlatformFile(file, 0, header_.get(), sizeof(Header)))
    return false;

  scoped_array<char> buffer(new char[kBufferSize]);
  size_t offset = sizeof(Header);
  uint32 hash = adler32(0, Z_NULL, 0);

  DCHECK_EQ(header_->num_entries, static_cast<int32>(map_.size()));
  KeyMap::iterator iterator = map_.begin();
  for (int remaining_records = header_->num_entries; remaining_records;) {
    int num_records = std::min(header_->num_entries,
                               static_cast<int>(kMaxRecordsToRead));
    size_t num_bytes = num_records * kRecordSize;
    remaining_records -= num_records;

    for (int i = 0; i < num_records; i++) {
      if (iterator == map_.end()) {
        NOTREACHED();
        return false;
      }
      char* record = buffer.get() + i * kRecordSize;
      *reinterpret_cast<Key*>(record) = iterator->first;
      *reinterpret_cast<Details*>(record + sizeof(Key)) = iterator->second;
      ++iterator;
    }

    hash = adler32(hash, reinterpret_cast<const Bytef*>(buffer.get()),
                   num_bytes);

    if (!remaining_records) {
      num_bytes += sizeof(uint32);  // Trailing hash.
      *reinterpret_cast<uint32*>(buffer.get() +
                                 num_records * kRecordSize) = hash;
    }

    DCHECK_LE(num_bytes, kBufferSize);
    if (!WritePlatformFile(file, offset, buffer.get(), num_bytes))
      return false;

    offset += num_bytes;
  }
  base::FlushPlatformFile(file);  // Ignore return value.
  return true;
}

bool InfiniteCache::Worker::ReadAndVerifyHeader(PlatformFile file) {
  base::PlatformFileInfo info;
  if (!base::GetPlatformFileInfo(file, &info))
    return false;

  if (info.size < static_cast<int>(sizeof(Header)))
    return false;

  header_.reset(new Header);
  if (!ReadPlatformFile(file, 0, header_.get(), sizeof(Header)))
    return false;

  if (header_->magic != kMagicSignature)
    return false;

  if (header_->version != kCurrentVersion)
    return false;

  if (header_->num_entries > kMaxNumEntries)
    return false;

  size_t expected_size = kRecordSize * header_->num_entries +
                         sizeof(Header) + sizeof(uint32);  // Trailing hash.

  if (info.size < static_cast<int>(expected_size))
    return false;

  uint32 hash = base::Hash(reinterpret_cast<char*>(header_.get()),
                                 offsetof(Header, header_hash));
  if (hash != header_->header_hash)
    return false;

  return true;
}

void InfiniteCache::Worker::Query(int* result) {
  *result = static_cast<int>(map_.size());
}

void InfiniteCache::Worker::Flush(int* result) {
  flushed_ = false;
  StoreData();
  flushed_ = true;
  *result = OK;
}

void InfiniteCache::Worker::OnTimer() {
  header_->use_minutes += kTimerMinutes;
  GenerateHistograms();
  StoreData();
}

void InfiniteCache::Worker::Add(const Details& details) {
  UpdateSize(0, details.headers_size);
  UpdateSize(0, details.response_size);
  header_->num_entries = static_cast<int>(map_.size());
  if (header_->num_entries == kMaxNumEntries) {
    int use_hours = header_->use_minutes / 60;
    int age_hours = (Time::Now() -
                     Time::FromInternalValue(header_->creation_time)).InHours();
    UMA_HISTOGRAM_COUNTS_10000("InfiniteCache.MaxUseTime", use_hours);
    UMA_HISTOGRAM_COUNTS_10000("InfiniteCache.MaxAge", age_hours);

    int entry_size = static_cast<int>(header_->total_size / kMaxNumEntries);
    UMA_HISTOGRAM_COUNTS("InfiniteCache.FinalAvgEntrySize", entry_size);
    header_->disabled = 1;
    map_.clear();
  }
}

void InfiniteCache::Worker::Remove(const Details& details) {
  UpdateSize(details.headers_size, 0);
  UpdateSize(details.response_size, 0);
  header_->num_entries--;
}

void InfiniteCache::Worker::UpdateSize(int old_size, int new_size) {
  header_->total_size += new_size - old_size;
  DCHECK_GE(header_->total_size, 0);
}

void InfiniteCache::Worker::RecordHit(const Details& old, Details* details) {
  header_->num_hits++;
  int access_delta = (IntToTime(details->last_access) -
                      IntToTime(old.last_access)).InMinutes();
  if (old.use_count)
    UMA_HISTOGRAM_COUNTS("InfiniteCache.ReuseAge", access_delta);
  else
    UMA_HISTOGRAM_COUNTS("InfiniteCache.FirstReuseAge", access_delta);

  details->use_count = old.use_count;
  if (details->use_count < kuint8max)
    details->use_count++;
  UMA_HISTOGRAM_CUSTOM_COUNTS("InfiniteCache.UseCount", details->use_count, 0,
                              kuint8max, 25);
}

void InfiniteCache::Worker::RecordUpdate(const Details& old, Details* details) {
  int access_delta = (IntToTime(details->last_access) -
                      IntToTime(old.last_access)).InMinutes();
  if (old.update_count)
    UMA_HISTOGRAM_COUNTS("InfiniteCache.UpdateAge", access_delta);
  else
    UMA_HISTOGRAM_COUNTS("InfiniteCache.FirstUpdateAge", access_delta);

  details->update_count = old.update_count;
  if (details->update_count < kuint8max)
    details->update_count++;

  UMA_HISTOGRAM_CUSTOM_COUNTS("InfiniteCache.UpdateCount",
                              details->update_count, 0, kuint8max, 25);
  details->use_count = 0;
}

void InfiniteCache::Worker::GenerateHistograms() {
  bool new_size_step = (header_->total_size / kReportSizeStep !=
                        header_->size_last_report / kReportSizeStep);
  header_->size_last_report = header_->total_size;
  if (!new_size_step && (header_->use_minutes % 60 != 0))
    return;

  if (header_->disabled)
    return;

  int hit_ratio = header_->num_hits * 100;
  if (header_->num_requests)
    hit_ratio /= header_->num_requests;
  else
    hit_ratio = 0;

  // We'll be generating pairs of histograms that can be used to get the hit
  // ratio for any bucket of the paired histogram.
  bool report_second_stat = base::RandInt(0, 99) < hit_ratio;

  if (header_->use_minutes % 60 == 0) {
    int use_hours = header_->use_minutes / 60;
    int age_hours = (Time::Now() -
                     Time::FromInternalValue(header_->creation_time)).InHours();
    UMA_HISTOGRAM_COUNTS_10000("InfiniteCache.UseTime", use_hours);
    UMA_HISTOGRAM_COUNTS_10000("InfiniteCache.Age", age_hours);
    if (report_second_stat) {
      UMA_HISTOGRAM_COUNTS_10000("InfiniteCache.HitRatioByUseTime", use_hours);
      UMA_HISTOGRAM_COUNTS_10000("InfiniteCache.HitRatioByAge", age_hours);
    }
  }

  if (new_size_step) {
    int size_bucket = static_cast<int>(header_->total_size / kReportSizeStep);
    UMA_HISTOGRAM_ENUMERATION("InfiniteCache.Size", std::min(size_bucket, 50),
                              51);
    UMA_HISTOGRAM_ENUMERATION("InfiniteCache.SizeCoarse", size_bucket / 5, 51);
    UMA_HISTOGRAM_COUNTS("InfiniteCache.Entries", header_->num_entries);
    UMA_HISTOGRAM_COUNTS_10000("InfiniteCache.BadHits", header_->num_bad_hits);
    if (report_second_stat) {
      UMA_HISTOGRAM_ENUMERATION("InfiniteCache.HitRatioBySize",
                                std::min(size_bucket, 50), 51);
      UMA_HISTOGRAM_ENUMERATION("InfiniteCache.HitRatioBySizeCoarse",
                                size_bucket / 5, 51);
      UMA_HISTOGRAM_COUNTS("InfiniteCache.HitRatioByEntries",
                           header_->num_entries);
    }
    header_->num_hits = 0;
    header_->num_bad_hits = 0;
    header_->num_requests = 0;
  }
}

bool InfiniteCache::Worker::CanReuse(const Details& old,
                                     const Details& current) {
  enum ReuseStatus {
    REUSE_OK = 0,
    REUSE_NO_CACHE,
    REUSE_ALWAYS_EXPIRED,
    REUSE_EXPIRED,
    REUSE_TRUNCATED,
    REUSE_VARY,
    REUSE_DUMMY_VALUE,
    // Not an individual value; it's added to another reason.
    REUSE_REVALIDATEABLE = 7
  };
  int reason = REUSE_OK;

  if (old.flags & NO_CACHE)
    reason = REUSE_NO_CACHE;

  if (old.flags & EXPIRED)
    reason = REUSE_ALWAYS_EXPIRED;

  if (old.flags & TRUNCATED)
    reason = REUSE_TRUNCATED;

  Time expiration = IntToTime(old.expiration);
  if (expiration < Time::Now())
    reason = REUSE_EXPIRED;

  if (old.vary_hash != current.vary_hash)
    reason = REUSE_VARY;

  bool have_to_drop  = (old.flags & TRUNCATED) && !(old.flags & RESUMABLE);
  if (reason && (old.flags & REVALIDATEABLE) && !have_to_drop)
    reason += REUSE_REVALIDATEABLE;

  UMA_HISTOGRAM_ENUMERATION("InfiniteCache.ReuseFailure", reason, 15);
  return !reason;
}

bool InfiniteCache::Worker::DataChanged(const Details& old,
                                        const Details& current) {
  bool changed = false;
  if (old.response_size != current.response_size) {
    changed = true;
    UpdateSize(old.response_size, current.response_size);
  }

  if (old.response_hash != current.response_hash)
    changed = true;

  return changed;
}

bool InfiniteCache::Worker::HeadersChanged(const Details& old,
                                           const Details& current) {
  bool changed = false;
  if (old.headers_size != current.headers_size) {
    changed = true;
    UpdateSize(old.headers_size, current.headers_size);
  }

  if (old.headers_hash != current.headers_hash)
    changed = true;

  return changed;
}

// ----------------------------------------------------------------------------

InfiniteCache::InfiniteCache() {
}

InfiniteCache::~InfiniteCache() {
  if (!worker_)
    return;

  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&InfiniteCache::Worker::Cleanup, worker_));
  worker_ = NULL;
}

void InfiniteCache::Init(const FilePath& path) {
  worker_pool_ = new base::SequencedWorkerPool(1, "Infinite cache thread");
  task_runner_ = worker_pool_->GetSequencedTaskRunnerWithShutdownBehavior(
                     worker_pool_->GetSequenceToken(),
                     base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN);

  worker_ = new Worker();
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&InfiniteCache::Worker::Init, worker_,
                                    path));

  timer_.Start(FROM_HERE, TimeDelta::FromMinutes(kTimerMinutes), this,
               &InfiniteCache::OnTimer);
}

InfiniteCacheTransaction* InfiniteCache::CreateInfiniteCacheTransaction() {
  if (!worker_)
    return NULL;
  return new InfiniteCacheTransaction(this);
}

int InfiniteCache::DeleteData(const CompletionCallback& callback) {
  if (!worker_)
    return OK;
  int* result = new int;
  task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&InfiniteCache::Worker::DeleteData, worker_,
                            result),
      base::Bind(&OnComplete, callback, base::Owned(result)));
  return ERR_IO_PENDING;
}

int InfiniteCache::DeleteDataBetween(base::Time initial_time,
                                     base::Time end_time,
                                     const CompletionCallback& callback) {
  if (!worker_)
    return OK;
  int* result = new int;
  task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&InfiniteCache::Worker::DeleteDataBetween, worker_,
                            initial_time, end_time, result),
      base::Bind(&OnComplete, callback, base::Owned(result)));
  return ERR_IO_PENDING;
}

std::string InfiniteCache::GenerateKey(const HttpRequestInfo* request) {
  // Don't add any upload data identifier.
  return HttpUtil::SpecForRequest(request->url);
}

void InfiniteCache::ProcessResource(
    scoped_ptr<InfiniteCacheTransaction::ResourceData> data) {
  if (!worker_)
    return;
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&InfiniteCache::Worker::Process, worker_,
                                    base::Passed(&data)));
}

void InfiniteCache::OnTimer() {
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&InfiniteCache::Worker::OnTimer, worker_));
}

int InfiniteCache::QueryItemsForTest(const CompletionCallback& callback) {
  DCHECK(worker_);
  int* result = new int;
  task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&InfiniteCache::Worker::Query, worker_, result),
      base::Bind(&OnComplete, callback, base::Owned(result)));
  return net::ERR_IO_PENDING;
}

int InfiniteCache::FlushDataForTest(const CompletionCallback& callback) {
  DCHECK(worker_);
  int* result = new int;
  task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&InfiniteCache::Worker::Flush, worker_, result),
      base::Bind(&OnComplete, callback, base::Owned(result)));
  return net::ERR_IO_PENDING;
}

}  // namespace net
