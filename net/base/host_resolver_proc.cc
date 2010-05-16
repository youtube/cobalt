// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_resolver_proc.h"

#include "build/build_config.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include <resolv.h>
#endif

#include "base/logging.h"
#include "base/time.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/sys_addrinfo.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "base/singleton.h"
#include "base/thread_local_storage.h"
#endif

namespace net {

HostResolverProc* HostResolverProc::default_proc_ = NULL;

HostResolverProc::HostResolverProc(HostResolverProc* previous) {
  SetPreviousProc(previous);

  // Implicitly fall-back to the global default procedure.
  if (!previous)
    SetPreviousProc(default_proc_);
}

void HostResolverProc::SetPreviousProc(HostResolverProc* proc) {
  HostResolverProc* current_previous = previous_proc_;
  previous_proc_ = NULL;
  // Now that we've guaranteed |this| is the last proc in a chain, we can
  // detect potential cycles using GetLastProc().
  previous_proc_ = (GetLastProc(proc) == this) ? current_previous : proc;
}

void HostResolverProc::SetLastProc(HostResolverProc* proc) {
  GetLastProc(this)->SetPreviousProc(proc);
}

// static
HostResolverProc* HostResolverProc::GetLastProc(HostResolverProc* proc) {
  if (proc == NULL)
    return NULL;
  HostResolverProc* last_proc = proc;
  while (last_proc->previous_proc_ != NULL)
    last_proc = last_proc->previous_proc_;
  return last_proc;
}

// static
HostResolverProc* HostResolverProc::SetDefault(HostResolverProc* proc) {
  HostResolverProc* old = default_proc_;
  default_proc_ = proc;
  return old;
}

// static
HostResolverProc* HostResolverProc::GetDefault() {
  return default_proc_;
}

int HostResolverProc::ResolveUsingPrevious(
    const std::string& host,
    AddressFamily address_family,
    HostResolverFlags host_resolver_flags,
    AddressList* addrlist,
    int* os_error) {
  if (previous_proc_) {
    return previous_proc_->Resolve(host, address_family, host_resolver_flags,
                                   addrlist, os_error);
  }

  // Final fallback is the system resolver.
  return SystemHostResolverProc(host, address_family, host_resolver_flags,
                                addrlist, os_error);
}

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_OPENBSD)
// On Linux/BSD, changes to /etc/resolv.conf can go unnoticed thus resulting
// in DNS queries failing either because nameservers are unknown on startup
// or because nameserver info has changed as a result of e.g. connecting to
// a new network. Some distributions patch glibc to stat /etc/resolv.conf
// to try to automatically detect such changes but these patches are not
// universal and even patched systems such as Jaunty appear to need calls
// to res_ninit to reload the nameserver information in different threads.
//
// We adopt the Mozilla solution here which is to call res_ninit when
// lookups fail and to rate limit the reloading to once per second per
// thread.
//
// OpenBSD does not have thread-safe res_ninit/res_nclose so we can't do
// the same trick there.

// Keep a timer per calling thread to rate limit the calling of res_ninit.
class DnsReloadTimer {
 public:
  // Check if the timer for the calling thread has expired. When no
  // timer exists for the calling thread, create one.
  bool Expired() {
    const base::TimeDelta kRetryTime = base::TimeDelta::FromSeconds(1);
    base::TimeTicks now = base::TimeTicks::Now();
    base::TimeTicks* timer_ptr =
      static_cast<base::TimeTicks*>(tls_index_.Get());

    if (!timer_ptr) {
      timer_ptr = new base::TimeTicks();
      *timer_ptr = base::TimeTicks::Now();
      tls_index_.Set(timer_ptr);
      // Return true to reload dns info on the first call for each thread.
      return true;
    } else if (now - *timer_ptr > kRetryTime) {
      *timer_ptr = now;
      return true;
    } else {
      return false;
    }
  }

  // Free the allocated timer.
  static void SlotReturnFunction(void* data) {
    base::TimeTicks* tls_data = static_cast<base::TimeTicks*>(data);
    delete tls_data;
  }

 private:
  friend struct DefaultSingletonTraits<DnsReloadTimer>;

  DnsReloadTimer() {
    // During testing the DnsReloadTimer Singleton may be created and destroyed
    // multiple times. Initialize the ThreadLocalStorage slot only once.
    if (!tls_index_.initialized())
      tls_index_.Initialize(SlotReturnFunction);
  }

  ~DnsReloadTimer() {
  }

  // We use thread local storage to identify which base::TimeTicks to
  // interact with.
  static ThreadLocalStorage::Slot tls_index_ ;

  DISALLOW_COPY_AND_ASSIGN(DnsReloadTimer);
};

// A TLS slot to the TimeTicks for the current thread.
// static
ThreadLocalStorage::Slot DnsReloadTimer::tls_index_(base::LINKER_INITIALIZED);

#endif  // defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_OPENBSD)

int SystemHostResolverProc(const std::string& host,
                           AddressFamily address_family,
                           HostResolverFlags host_resolver_flags,
                           AddressList* addrlist,
                           int* os_error) {
  if (os_error)
    *os_error = 0;

  // The result of |getaddrinfo| for empty hosts is inconsistent across systems.
  // On Windows it gives the default interface's address, whereas on Linux it
  // gives an error. We will make it fail on all platforms for consistency.
  if (host.empty())
    return ERR_NAME_NOT_RESOLVED;

  struct addrinfo* ai = NULL;
  struct addrinfo hints = {0};

  switch (address_family) {
    case ADDRESS_FAMILY_IPV4:
      hints.ai_family = AF_INET;
      break;
    case ADDRESS_FAMILY_IPV6:
      hints.ai_family = AF_INET6;
      break;
    case ADDRESS_FAMILY_UNSPECIFIED:
      hints.ai_family = AF_UNSPEC;
      break;
    default:
      NOTREACHED();
      hints.ai_family = AF_UNSPEC;
  }

#if defined(OS_WIN) || defined(OS_OPENBSD)
  // DO NOT USE AI_ADDRCONFIG ON WINDOWS.
  //
  // The following comment in <winsock2.h> is the best documentation I found
  // on AI_ADDRCONFIG for Windows:
  //   Flags used in "hints" argument to getaddrinfo()
  //       - AI_ADDRCONFIG is supported starting with Vista
  //       - default is AI_ADDRCONFIG ON whether the flag is set or not
  //         because the performance penalty in not having ADDRCONFIG in
  //         the multi-protocol stack environment is severe;
  //         this defaulting may be disabled by specifying the AI_ALL flag,
  //         in that case AI_ADDRCONFIG must be EXPLICITLY specified to
  //         enable ADDRCONFIG behavior
  //
  // Not only is AI_ADDRCONFIG unnecessary, but it can be harmful.  If the
  // computer is not connected to a network, AI_ADDRCONFIG causes getaddrinfo
  // to fail with WSANO_DATA (11004) for "localhost", probably because of the
  // following note on AI_ADDRCONFIG in the MSDN getaddrinfo page:
  //   The IPv4 or IPv6 loopback address is not considered a valid global
  //   address.
  // See http://crbug.com/5234.
  //
  // OpenBSD does not support it, either.
  hints.ai_flags = 0;
#else
  hints.ai_flags = AI_ADDRCONFIG;
#endif

  if (host_resolver_flags & HOST_RESOLVER_CANONNAME)
    hints.ai_flags |= AI_CANONNAME;

  // Restrict result set to only this socket type to avoid duplicates.
  hints.ai_socktype = SOCK_STREAM;

  int err = getaddrinfo(host.c_str(), NULL, &hints, &ai);
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_OPENBSD)
  net::DnsReloadTimer* dns_timer = Singleton<net::DnsReloadTimer>::get();
  // If we fail, re-initialise the resolver just in case there have been any
  // changes to /etc/resolv.conf and retry. See http://crbug.com/11380 for info.
  if (err && dns_timer->Expired()) {
    res_nclose(&_res);
    if (!res_ninit(&_res))
      err = getaddrinfo(host.c_str(), NULL, &hints, &ai);
  }
#endif

  if (err) {
    if (os_error) {
#if defined(OS_WIN)
      *os_error = WSAGetLastError();
#else
      *os_error = err;
#endif
    }
    return ERR_NAME_NOT_RESOLVED;
  }

  addrlist->Adopt(ai);
  return OK;
}

}  // namespace net
