// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/dnsrr_resolver.h"

#if defined(OS_POSIX)
#include <resolv.h>
#endif

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/string_piece.h"
#include "base/task.h"
#include "base/worker_pool.h"
#include "net/base/dns_reload_timer.h"
#include "net/base/dns_util.h"
#include "net/base/net_errors.h"

namespace net {

static const uint16 kClassIN = 1;

namespace {

class CompletionCallbackTask : public Task,
                               public MessageLoop::DestructionObserver {
 public:
  explicit CompletionCallbackTask(CompletionCallback* callback,
                                  MessageLoop* ml)
      : callback_(callback),
        rv_(OK),
        posted_(false),
        message_loop_(ml) {
    ml->AddDestructionObserver(this);
  }

  void set_rv(int rv) {
    rv_ = rv;
  }

  void Post() {
    AutoLock locked(lock_);
    DCHECK(!posted_);

    if (!message_loop_) {
      // MessageLoop got deleted, nothing to do.
      delete this;
      return;
    }
    posted_ = true;
    message_loop_->PostTask(FROM_HERE, this);
  }

  // Task interface
  void Run() {
    message_loop_->RemoveDestructionObserver(this);
    callback_->Run(rv_);
    // We will be deleted by the message loop.
  }

  // DestructionObserver interface
  virtual void WillDestroyCurrentMessageLoop() {
    AutoLock locked(lock_);
    message_loop_ = NULL;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CompletionCallbackTask);

  CompletionCallback* callback_;
  int rv_;
  bool posted_;

  Lock lock_;  // covers |message_loop_|
  MessageLoop* message_loop_;
};

#if defined(OS_POSIX)
class ResolveTask : public Task {
 public:
  ResolveTask(const std::string& name, uint16 rrtype,
              uint16 flags, CompletionCallback* callback,
              RRResponse* response, MessageLoop* ml)
      : name_(name),
        rrtype_(rrtype),
        flags_(flags),
        subtask_(new CompletionCallbackTask(callback, ml)),
        response_(response) {
  }

  virtual void Run() {
    // Runs on a worker thread.

    bool r = true;
    if ((_res.options & RES_INIT) == 0) {
      if (res_ninit(&_res) != 0)
        r = false;
    }

    if (r) {
      unsigned long saved_options = _res.options;
      r = Do();

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_OPENBSD)
      if (!r && DnsReloadTimerHasExpired()) {
        res_nclose(&_res);
        if (res_ninit(&_res) == 0)
          r = Do();
      }
#endif
      _res.options = saved_options;
    }
    int error = r ? OK : ERR_NAME_NOT_RESOLVED;

    subtask_->set_rv(error);
    subtask_.release()->Post();
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

    return response_->ParseFromResponse(answer, len, rrtype_);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ResolveTask);

  const std::string name_;
  const uint16 rrtype_;
  const uint16 flags_;
  scoped_ptr<CompletionCallbackTask> subtask_;
  RRResponse* const response_;
};
#else  // OS_POSIX
// On non-Linux platforms we fail everything for now.
class ResolveTask : public Task {
 public:
  ResolveTask(const std::string& name, uint16 rrtype,
              uint16 flags, CompletionCallback* callback,
              RRResponse* response, MessageLoop* ml)
      : subtask_(new CompletionCallbackTask(callback, ml)) {
  }

  virtual void Run() {
    subtask_->set_rv(ERR_NAME_NOT_RESOLVED);
    subtask_->Post();
  }

 private:
  CompletionCallbackTask* const subtask_;
  DISALLOW_COPY_AND_ASSIGN(ResolveTask);
};
#endif

// A Buffer is used for walking over a DNS packet.
class Buffer {
 public:
  Buffer(const uint8* p, unsigned len)
      : p_(p),
        packet_(p),
        len_(len),
        packet_len_(len) {
  }

  bool U8(uint8* v) {
    if (len_ < 1)
      return false;
    *v = *p_;
    p_++;
    len_--;
    return true;
  }

  bool U16(uint16* v) {
    if (len_ < 2)
      return false;
    *v = static_cast<uint16>(p_[0]) << 8 |
         static_cast<uint16>(p_[1]);
    p_ += 2;
    len_ -= 2;
    return true;
  }

  bool U32(uint32* v) {
    if (len_ < 4)
      return false;
    *v = static_cast<uint32>(p_[0]) << 24 |
         static_cast<uint32>(p_[1]) << 16 |
         static_cast<uint32>(p_[2]) << 8 |
         static_cast<uint32>(p_[3]);
    p_ += 4;
    len_ -= 4;
    return true;
  }

  bool Skip(unsigned n) {
    if (len_ < n)
      return false;
    p_ += n;
    len_ -= n;
    return true;
  }

  bool Block(base::StringPiece* out, unsigned len) {
    if (len_ < len)
      return false;
    *out = base::StringPiece(reinterpret_cast<const char*>(p_), len);
    p_ += len;
    len_ -= len;
    return true;
  }

  // DNSName parses a (possibly compressed) DNS name from the packet. If |name|
  // is not NULL, then the name is written into it. See RFC 1035 section 4.1.4.
  bool DNSName(std::string* name) {
    unsigned jumps = 0;
    const uint8* p = p_;
    unsigned len = len_;

    if (name)
      name->clear();

    for (;;) {
      if (len < 1)
        return false;
      uint8 d = *p;
      p++;
      len--;

      // The two couple of bits of the length give the type of the length. It's
      // either a direct length or a pointer to the remainder of the name.
      if ((d & 0xc0) == 0xc0) {
        // This limit matches the depth limit in djbdns.
        if (jumps > 100)
          return false;
        if (len < 1)
          return false;
        uint16 offset = static_cast<uint16>(d) << 8 |
                        static_cast<uint16>(p[0]);
        offset &= 0x3ff;
        p++;
        len--;

        if (jumps == 0) {
          p_ = p;
          len_ = len;
        }
        jumps++;

        if (offset >= packet_len_)
          return false;
        p = &packet_[offset];
      } else if ((d & 0xc0) == 0) {
        uint8 label_len = d;
        if (len < label_len)
          return false;
        if (name && label_len) {
          if (!name->empty())
            name->append(".");
          name->append(reinterpret_cast<const char*>(p), label_len);
        }
        p += label_len;
        len -= label_len;

        if (jumps == 0) {
          p_ = p;
          len_ = len;
        }

        if (label_len == 0)
          break;
      } else {
        return false;
      }
    }

    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Buffer);

  const uint8* p_;
  const uint8* const packet_;
  unsigned len_;
  const unsigned packet_len_;
};

}  // anonymous namespace

bool RRResponse::ParseFromResponse(const uint8* p, unsigned len,
                                   uint16 rrtype_requested) {
#if defined(OS_POSIX)
  name.clear();
  ttl = 0;
  dnssec = false;
  rrdatas.clear();
  signatures.clear();

  // RFC 1035 section 4.4.1
  uint8 flags2;
  Buffer buf(p, len);
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
#endif  // defined(OS_POSIX)

  return true;
}


// static
bool DnsRRResolver::Resolve(const std::string& name, uint16 rrtype,
                            uint16 flags, CompletionCallback* callback,
                            RRResponse* response) {
  if (!callback || !response || name.empty())
    return false;

  // Don't allow queries of type ANY
  if (rrtype == kDNS_ANY)
    return false;

  ResolveTask* task = new ResolveTask(name, rrtype, flags, callback, response,
                                      MessageLoop::current());

  return WorkerPool::PostTask(FROM_HERE, task, true /* task is slow */);
}

}  // namespace net
