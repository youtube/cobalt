// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "net/base/host_resolver_proc.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <ws2tcpip.h>
#include <wspiapi.h>  // Needed for Win2k compat.
#elif defined(OS_POSIX)
#include <netdb.h>
#include <sys/socket.h>
#endif
#if defined(OS_LINUX)
#include <resolv.h>
#endif

#include "base/logging.h"
#include "base/time.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"

#if defined(OS_LINUX)
#include "base/singleton.h"
#include "base/thread_local_storage.h"
#endif

namespace net {

HostResolverProc* HostResolverProc::default_proc_ = NULL;

HostResolverProc::HostResolverProc(HostResolverProc* previous) {
  set_previous_proc(previous);

  // Implicitly fall-back to the global default procedure.
  if (!previous)
    set_previous_proc(default_proc_);
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

int HostResolverProc::ResolveUsingPrevious(const std::string& host,
                                           AddressFamily address_family,
                                           AddressList* addrlist) {
  if (previous_proc_)
    return previous_proc_->Resolve(host, address_family, addrlist);

  // Final fallback is the system resolver.
  return SystemHostResolverProc(host, address_family, addrlist);
}

#if defined(OS_LINUX)
// On Linux changes to /etc/resolv.conf can go unnoticed thus resulting in
// DNS queries failing either because nameservers are unknown on startup
// or because nameserver info has changed as a result of e.g. connecting to
// a new network. Some distributions patch glibc to stat /etc/resolv.conf
// to try to automatically detect such changes but these patches are not
// universal and even patched systems such as Jaunty appear to need calls
// to res_ninit to reload the nameserver information in different threads.
//
// We adopt the Mozilla solution here which is to call res_ninit when
// lookups fail and to rate limit the reloading to once per second per
// thread.

// Keep a timer per calling thread to rate limit the calling of res_ninit.
class DnsReloadTimer {
 public:
  DnsReloadTimer() {
    tls_index_.Initialize(SlotReturnFunction);
  }

  ~DnsReloadTimer() { }

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
  // We use thread local storage to identify which base::TimeTicks to
  // interact with.
  static ThreadLocalStorage::Slot tls_index_ ;

  DISALLOW_COPY_AND_ASSIGN(DnsReloadTimer);
};

// A TLS slot to the TimeTicks for the current thread.
// static
ThreadLocalStorage::Slot DnsReloadTimer::tls_index_(base::LINKER_INITIALIZED);

#endif  // defined(OS_LINUX)

int SystemHostResolverProc(const std::string& host,
                           AddressFamily address_family,
                           AddressList* addrlist) {
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

#if defined(OS_WIN)
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
  hints.ai_flags = 0;
#else
  hints.ai_flags = AI_ADDRCONFIG;
#endif

  // Restrict result set to only this socket type to avoid duplicates.
  hints.ai_socktype = SOCK_STREAM;

  // Copy up to the first 255 bytes of |host| onto the stack, so we can see
  // what it was when getaddrinfo() crashes.
  // TODO(eroman): Remove this once done investigating http://crbug.com/22083.
  char buffer[256];
  size_t actual_size = host.size() + 1;  // The size of |host| (including NULL).
  size_t saved_size = std::min(actual_size, sizeof(buffer));
  memcpy(buffer, host.data(), saved_size - 1);
  buffer[saved_size - 1] = '\0';

  // Try to use the copy of |host| that was saved to the stack.
  // (This will help rule out concurrent mutations on |host| as a factor.)
  const char* host_cstr = (actual_size == saved_size) ? buffer : host.c_str();

  int err = getaddrinfo(host_cstr, NULL, &hints, &ai);

  // Keep the variables alive so compiler can't optimize away.
  CHECK(actual_size > 0 && buffer[saved_size - 1] == '\0');

#if defined(OS_LINUX)
  net::DnsReloadTimer* dns_timer = Singleton<net::DnsReloadTimer>::get();
  // If we fail, re-initialise the resolver just in case there have been any
  // changes to /etc/resolv.conf and retry. See http://crbug.com/11380 for info.
  if (err && dns_timer->Expired()) {
    res_nclose(&_res);
    if (!res_ninit(&_res))
      err = getaddrinfo(host.c_str(), NULL, &hints, &ai);
  }
#endif

  if (err)
    return ERR_NAME_NOT_RESOLVED;

  addrlist->Adopt(ai);
  return OK;
}

}  // namespace net
