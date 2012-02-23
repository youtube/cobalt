// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/dnsrr_resolver.h"

#if defined(OS_POSIX)
#include <netinet/in.h>
#include <resolv.h>
#endif

#if defined(OS_WIN)
#include <windns.h>
#endif

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/string_piece.h"
#include "base/synchronization/lock.h"
#include "base/threading/worker_pool.h"
#include "net/base/dns_reloader.h"
#include "net/base/dns_util.h"
#include "net/base/net_errors.h"

// Life of a query:
//
// DnsRRResolver RRResolverJob RRResolverWorker       ...         Handle
//      |                       (origin loop)    (worker loop)
//      |
//   Resolve()
//      |---->-------------------<creates>
//      |
//      |---->----<creates>
//      |
//      |---->---------------------------------------------------<creates>
//      |
//      |---->--------------------Start
//      |                           |
//      |                        PostTask
//      |
//      |                                     <starts resolving>
//      |---->-----AddHandle                          |
//                                                    |
//                                                    |
//                                                    |
//                                                  Finish
//                                                    |
//                                                 PostTask
//
//                                   |
//                                DoReply
//      |----<-----------------------|
//  HandleResult
//      |
//      |---->-----HandleResult
//                      |
//                      |------>-----------------------------------Post
//
//
//
// A cache hit:
//
// DnsRRResolver                       Handle
//      |
//   Resolve()
//      |---->------------------------<creates>
//      |
//      |
//   PostTask
//
// (MessageLoop cycles)
//
//                                      Post

namespace net {

#if defined(OS_WIN)
// DnsRRIsParsedByWindows returns true if Windows knows how to parse the given
// RR type. RR data is returned in a DNS_RECORD structure which may be raw (if
// Windows doesn't parse it) or may be a parse result. It's unclear how this
// API is intended to evolve in the future. If Windows adds support for new RR
// types in a future version a client which expected raw data will break.
// See http://msdn.microsoft.com/en-us/library/ms682082(v=vs.85).aspx
static bool DnsRRIsParsedByWindows(uint16 rrtype) {
  // We only cover the types which are defined in dns_util.h
  switch (rrtype) {
    case kDNS_CNAME:
    case kDNS_TXT:
    case kDNS_DS:
    case kDNS_RRSIG:
    case kDNS_DNSKEY:
      return true;
    default:
      return false;
  }
}
#endif

// kMaxCacheEntries is the number of RRResponse objects that we'll cache.
static const unsigned kMaxCacheEntries = 32;
// kNegativeTTLSecs is the number of seconds for which we'll cache a negative
// cache entry.
static const unsigned kNegativeTTLSecs = 60;

RRResponse::RRResponse()
    : ttl(0), dnssec(false), negative(false) {
}

RRResponse::~RRResponse() {}

class RRResolverHandle {
 public:
  RRResolverHandle(const CompletionCallback& callback, RRResponse* response)
      : callback_(callback),
        response_(response) {
  }

  // Cancel ensures that the result callback will never be made.
  void Cancel() {
    callback_.Reset();
    response_ = NULL;
  }

  // Post copies the contents of |response| to the caller's RRResponse and
  // calls the callback.
  void Post(int rv, const RRResponse* response) {
    if (!callback_.is_null()) {
      if (response_ && response)
        *response_ = *response;
      callback_.Run(rv);
    }
    delete this;
  }

 private:
  CompletionCallback callback_;
  RRResponse* response_;
};


// RRResolverWorker runs on a worker thread and takes care of the blocking
// process of performing the DNS resolution.
class RRResolverWorker {
 public:
  RRResolverWorker(const std::string& name, uint16 rrtype, uint16 flags,
                   DnsRRResolver* dnsrr_resolver)
      : name_(name),
        rrtype_(rrtype),
        flags_(flags),
        origin_loop_(MessageLoop::current()),
        dnsrr_resolver_(dnsrr_resolver),
        canceled_(false),
        result_(ERR_UNEXPECTED) {
  }

  bool Start() {
    DCHECK_EQ(MessageLoop::current(), origin_loop_);

    return base::WorkerPool::PostTask(
        FROM_HERE, base::Bind(&RRResolverWorker::Run, base::Unretained(this)),
        true /* task is slow */);
  }

  // Cancel is called from the origin loop when the DnsRRResolver is getting
  // deleted.
  void Cancel() {
    DCHECK_EQ(MessageLoop::current(), origin_loop_);
    base::AutoLock locked(lock_);
    canceled_ = true;
  }

 private:

#if defined(OS_ANDROID)

  void Run() {
    NOTIMPLEMENTED();
  }

#elif defined(OS_POSIX)

  void Run() {
    // Runs on a worker thread.

    if (HandleTestCases()) {
      Finish();
      return;
    }

    bool r = true;
#if defined(OS_MACOSX) || defined(OS_OPENBSD)
    if ((_res.options & RES_INIT) == 0) {
#if defined(OS_OPENBSD)
      if (res_init() != 0)
#else
      if (res_ninit(&_res) != 0)
#endif
        r = false;
    }
#else
    DnsReloaderMaybeReload();
#endif

    if (r) {
      unsigned long saved_options = _res.options;
      r = Do();
      _res.options = saved_options;
    }

    response_.fetch_time = base::Time::Now();

    if (r) {
      result_ = OK;
    } else {
      result_ = ERR_NAME_NOT_RESOLVED;
      response_.negative = true;
      response_.ttl = kNegativeTTLSecs;
    }

    Finish();
  }

  bool Do() {
    // For DNSSEC, a 4K buffer is suggested
    static const unsigned kMaxDNSPayload = 4096;

#ifndef RES_USE_DNSSEC
    // Some versions of libresolv don't have support for the DO bit. In this
    // case, we proceed without it.
    static const int RES_USE_DNSSEC = 0;
#endif

#ifndef RES_USE_EDNS0
    // Some versions of glibc are so old that they don't support EDNS0 either.
    // http://code.google.com/p/chromium/issues/detail?id=51676
    static const int RES_USE_EDNS0 = 0;
#endif

    // We set the options explicitly. Note that this removes several default
    // options: RES_DEFNAMES and RES_DNSRCH (see res_init(3)).
    _res.options = RES_INIT | RES_RECURSE | RES_USE_EDNS0 | RES_USE_DNSSEC;
    uint8 answer[kMaxDNSPayload];
    int len = res_search(name_.c_str(), kClassIN, rrtype_, answer,
                         sizeof(answer));
    if (len == -1)
      return false;

    return response_.ParseFromResponse(answer, len, rrtype_);
  }

#else  // OS_WIN

  void Run() {
    if (HandleTestCases()) {
      Finish();
      return;
    }

    // See http://msdn.microsoft.com/en-us/library/ms682016(v=vs.85).aspx
    PDNS_RECORD record = NULL;
    DNS_STATUS status =
        DnsQuery_A(name_.c_str(), rrtype_, DNS_QUERY_STANDARD,
                   NULL /* pExtra (reserved) */, &record, NULL /* pReserved */);
    response_.fetch_time = base::Time::Now();
    response_.name = name_;
    response_.dnssec = false;
    response_.ttl = 0;

    if (status != 0) {
      response_.negative = true;
      result_ = ERR_NAME_NOT_RESOLVED;
    } else {
      response_.negative = false;
      result_ = OK;
      for (DNS_RECORD* cur = record; cur; cur = cur->pNext) {
        if (cur->wType == rrtype_) {
          response_.ttl = record->dwTtl;
          // Windows will parse some types of resource records. If we want one
          // of these types then we have to reserialise the record.
          switch (rrtype_) {
            case kDNS_TXT: {
              // http://msdn.microsoft.com/en-us/library/ms682109(v=vs.85).aspx
              const DNS_TXT_DATA* txt = &cur->Data.TXT;
              std::string rrdata;

              for (DWORD i = 0; i < txt->dwStringCount; i++) {
                // Although the string is typed as a PWSTR, it's actually just
                // an ASCII byte-string.  Also, the string must be < 256
                // elements because the length in the DNS packet is a single
                // byte.
                const char* s = reinterpret_cast<char*>(txt->pStringArray[i]);
                size_t len = strlen(s);
                DCHECK_LT(len, 256u);
                char len8 = static_cast<char>(len);
                rrdata.push_back(len8);
                rrdata += s;
              }
              response_.rrdatas.push_back(rrdata);
              break;
            }
            default:
              if (DnsRRIsParsedByWindows(rrtype_)) {
                // Windows parses this type, but we don't have code to unparse
                // it.
                NOTREACHED() << "you need to add code for the RR type here";
                response_.negative = true;
                result_ = ERR_INVALID_ARGUMENT;
              } else {
                // This type is given to us raw.
                response_.rrdatas.push_back(
                    std::string(reinterpret_cast<char*>(&cur->Data),
                                cur->wDataLength));
              }
          }
        }
      }
    }

    DnsRecordListFree(record, DnsFreeRecordList);
    Finish();
  }

#endif  // OS_WIN

  // HandleTestCases stuffs in magic test values in the event that the query is
  // from a unittest.
  bool HandleTestCases() {
    if (rrtype_ == kDNS_TESTING) {
      response_.fetch_time = base::Time::Now();

      if (name_ == "www.testing.notatld") {
        response_.ttl = 86400;
        response_.negative = false;
        response_.rrdatas.push_back("goats!");
        result_ = OK;
        return true;
      } else if (name_ == "nx.testing.notatld") {
        response_.negative = true;
        result_ = ERR_NAME_NOT_RESOLVED;
        return true;
      }
    }

    return false;
  }

  // DoReply runs on the origin thread.
  void DoReply() {
    DCHECK_EQ(MessageLoop::current(), origin_loop_);
    {
      // We lock here because the worker thread could still be in Finished,
      // after the PostTask, but before unlocking |lock_|. If we do not lock in
      // this case, we will end up deleting a locked Lock, which can lead to
      // memory leaks or worse errors.
      base::AutoLock locked(lock_);
      if (!canceled_)
        dnsrr_resolver_->HandleResult(name_, rrtype_, result_, response_);
    }
    delete this;
  }

  void Finish() {
    // Runs on the worker thread.
    // We assume that the origin loop outlives the DnsRRResolver. If the
    // DnsRRResolver is deleted, it will call Cancel on us. If it does so
    // before the Acquire, we'll delete ourselves and return. If it's trying to
    // do so concurrently, then it'll block on the lock and we'll call PostTask
    // while the DnsRRResolver (and therefore the MessageLoop) is still alive.
    // If it does so after this function, we assume that the MessageLoop will
    // process pending tasks. In which case we'll notice the |canceled_| flag
    // in DoReply.

    bool canceled;
    {
      base::AutoLock locked(lock_);
      canceled = canceled_;
      if (!canceled) {
        origin_loop_->PostTask(FROM_HERE, base::Bind(
            &RRResolverWorker::DoReply, base::Unretained(this)));
      }
    }

    if (canceled)
      delete this;
  }

  const std::string name_;
  const uint16 rrtype_;
  const uint16 flags_;
  MessageLoop* const origin_loop_;
  DnsRRResolver* const dnsrr_resolver_;

  base::Lock lock_;
  bool canceled_;

  int result_;
  RRResponse response_;

  DISALLOW_COPY_AND_ASSIGN(RRResolverWorker);
};

bool RRResponse::HasExpired(const base::Time current_time) const {
  const base::TimeDelta delta(base::TimeDelta::FromSeconds(ttl));
  const base::Time expiry = fetch_time + delta;
  return current_time >= expiry;
}

#if defined(OS_POSIX) && !defined(OS_ANDROID)
bool RRResponse::ParseFromResponse(const uint8* p, unsigned len,
                                   uint16 rrtype_requested) {
  name.clear();
  ttl = 0;
  dnssec = false;
  negative = false;
  rrdatas.clear();
  signatures.clear();

  // RFC 1035 section 4.4.1
  uint8 flags2;
  DnsResponseBuffer buf(p, len);
  if (!buf.Skip(2) ||  // skip id
      !buf.Skip(1) ||  // skip first flags byte
      !buf.U8(&flags2)) {
    return false;
  }

  // Bit 5 is the Authenticated Data (AD) bit. See
  // http://tools.ietf.org/html/rfc2535#section-6.1
  if (flags2 & 32) {
    // AD flag is set. We'll trust it if it came from a local nameserver.
    // Currently the resolv structure is IPv4 only, so we can't test for IPv6
    // loopback addresses.
    if (_res.nscount == 1 &&
        memcmp(&_res.nsaddr_list[0].sin_addr,
               "\x7f\x00\x00\x01" /* 127.0.0.1 */, 4) == 0) {
      dnssec = true;
    }
  }

  uint16 query_count, answer_count, authority_count, additional_count;
  if (!buf.U16(&query_count) ||
      !buf.U16(&answer_count) ||
      !buf.U16(&authority_count) ||
      !buf.U16(&additional_count)) {
    return false;
  }

  if (query_count != 1)
    return false;

  uint16 type, klass;
  if (!buf.DNSName(NULL) ||
      !buf.U16(&type) ||
      !buf.U16(&klass) ||
      type != rrtype_requested ||
      klass != kClassIN) {
    return false;
  }

  if (answer_count < 1)
    return false;

  for (uint32 i = 0; i < answer_count; i++) {
    std::string* name = NULL;
    if (i == 0)
      name = &this->name;
    uint32 ttl;
    uint16 rrdata_len;
    if (!buf.DNSName(name) ||
        !buf.U16(&type) ||
        !buf.U16(&klass) ||
        !buf.U32(&ttl) ||
        !buf.U16(&rrdata_len)) {
      return false;
    }

    base::StringPiece rrdata;
    if (!buf.Block(&rrdata, rrdata_len))
      return false;

    if (klass == kClassIN && type == rrtype_requested) {
      if (i == 0)
        this->ttl = ttl;
      rrdatas.push_back(std::string(rrdata.data(), rrdata.size()));
    } else if (klass == kClassIN && type == kDNS_RRSIG) {
      signatures.push_back(std::string(rrdata.data(), rrdata.size()));
    }
  }

  return true;
}
#endif  // defined(OS_POSIX) && !defined(OS_ANDROID)


// An RRResolverJob is a one-to-one counterpart of an RRResolverWorker. It
// lives only on the DnsRRResolver's origin message loop.
class RRResolverJob {
 public:
  explicit RRResolverJob(RRResolverWorker* worker)
      : worker_(worker) {
  }

  ~RRResolverJob() {
    if (worker_) {
      worker_->Cancel();
      worker_ = NULL;
      PostAll(ERR_ABORTED, NULL);
    }
  }

  void AddHandle(RRResolverHandle* handle) {
    handles_.push_back(handle);
  }

  void HandleResult(int result, const RRResponse& response) {
    worker_ = NULL;
    PostAll(result, &response);
  }

 private:
  void PostAll(int result, const RRResponse* response) {
    std::vector<RRResolverHandle*> handles;
    handles_.swap(handles);

    for (std::vector<RRResolverHandle*>::iterator
         i = handles.begin(); i != handles.end(); i++) {
      (*i)->Post(result, response);
      // Post() causes the RRResolverHandle to delete itself.
    }
  }

  std::vector<RRResolverHandle*> handles_;
  RRResolverWorker* worker_;
};


DnsRRResolver::DnsRRResolver()
    : requests_(0),
      cache_hits_(0),
      inflight_joins_(0),
      in_destructor_(false) {
}

DnsRRResolver::~DnsRRResolver() {
  DCHECK(!in_destructor_);
  in_destructor_ = true;
  STLDeleteValues(&inflight_);
}

intptr_t DnsRRResolver::Resolve(const std::string& name, uint16 rrtype,
                                uint16 flags,
                                const CompletionCallback& callback,
                                RRResponse* response,
                                int priority /* ignored */,
                                const BoundNetLog& netlog /* ignored */) {
  DCHECK(CalledOnValidThread());
  DCHECK(!in_destructor_);

  if (callback.is_null() || !response || name.empty())
    return kInvalidHandle;

  // Don't allow queries of type ANY
  if (rrtype == kDNS_ANY)
    return kInvalidHandle;

  requests_++;

  const std::pair<std::string, uint16> key(make_pair(name, rrtype));
  // First check the cache.
  std::map<std::pair<std::string, uint16>, RRResponse>::iterator i;
  i = cache_.find(key);
  if (i != cache_.end()) {
    if (!i->second.HasExpired(base::Time::Now())) {
      int error;
      if (i->second.negative) {
        error = ERR_NAME_NOT_RESOLVED;
      } else {
        error = OK;
        *response = i->second;
      }
      RRResolverHandle* handle = new RRResolverHandle(
          callback, NULL /* no response pointer because we've already filled */
                         /* it in */);
      cache_hits_++;
      // We need a typed NULL pointer in order to make the templates work out.
      static const RRResponse* kNoResponse = NULL;
      MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(
              &RRResolverHandle::Post, base::Unretained(handle), error,
              kNoResponse));
      return reinterpret_cast<intptr_t>(handle);
    } else {
      // entry has expired.
      cache_.erase(i);
    }
  }

  // No cache hit. See if a request is currently in flight.
  RRResolverJob* job;
  std::map<std::pair<std::string, uint16>, RRResolverJob*>::const_iterator j;
  j = inflight_.find(key);
  if (j != inflight_.end()) {
    // The request is in flight already. We'll just attach our callback.
    inflight_joins_++;
    job = j->second;
  } else {
    // Need to make a new request.
    RRResolverWorker* worker = new RRResolverWorker(name, rrtype, flags, this);
    job = new RRResolverJob(worker);
    inflight_.insert(make_pair(key, job));
    if (!worker->Start()) {
      inflight_.erase(key);
      delete job;
      delete worker;
      return kInvalidHandle;
    }
  }

  RRResolverHandle* handle = new RRResolverHandle(callback, response);
  job->AddHandle(handle);
  return reinterpret_cast<intptr_t>(handle);
}

void DnsRRResolver::CancelResolve(intptr_t h) {
  DCHECK(CalledOnValidThread());
  RRResolverHandle* handle = reinterpret_cast<RRResolverHandle*>(h);
  handle->Cancel();
}

void DnsRRResolver::OnIPAddressChanged() {
  DCHECK(CalledOnValidThread());
  DCHECK(!in_destructor_);

  std::map<std::pair<std::string, uint16>, RRResolverJob*> inflight;
  inflight.swap(inflight_);
  cache_.clear();

  STLDeleteValues(&inflight);
}

// HandleResult is called on the origin message loop.
void DnsRRResolver::HandleResult(const std::string& name, uint16 rrtype,
                                 int result, const RRResponse& response) {
  DCHECK(CalledOnValidThread());

  const std::pair<std::string, uint16> key(std::make_pair(name, rrtype));

  DCHECK_GE(kMaxCacheEntries, 1u);
  DCHECK_LE(cache_.size(), kMaxCacheEntries);
  if (cache_.size() == kMaxCacheEntries) {
    // need to remove an element of the cache.
    const base::Time current_time(base::Time::Now());
    std::map<std::pair<std::string, uint16>, RRResponse>::iterator i, cur;
    for (i = cache_.begin(); i != cache_.end(); ) {
      cur = i++;
      if (cur->second.HasExpired(current_time))
        cache_.erase(cur);
    }
  }
  if (cache_.size() == kMaxCacheEntries) {
    // if we didn't clear out any expired entries, we just remove the first
    // element. Crummy but simple.
    cache_.erase(cache_.begin());
  }

  cache_.insert(std::make_pair(key, response));

  std::map<std::pair<std::string, uint16>, RRResolverJob*>::iterator j;
  j = inflight_.find(key);
  if (j == inflight_.end()) {
    NOTREACHED();
    return;
  }
  RRResolverJob* job = j->second;
  inflight_.erase(j);

  job->HandleResult(result, response);
  delete job;
}

}  // namespace net
