// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/host_resolver_proc.h"

#include "build/build_config.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include <resolv.h>
#endif

#include "base/logging.h"
#include "net/base/address_list.h"
#include "net/base/dns_reload_timer.h"
#include "net/base/net_errors.h"
#include "net/base/sys_addrinfo.h"

namespace net {

namespace {

bool IsAllLocalhostOfOneFamily(const struct addrinfo* ai) {
  bool saw_v4_localhost = false;
  bool saw_v6_localhost = false;
  for (; ai != NULL; ai = ai->ai_next) {
    switch (ai->ai_family) {
      case AF_INET: {
        const struct sockaddr_in* addr_in =
            reinterpret_cast<struct sockaddr_in*>(ai->ai_addr);
        if ((ntohl(addr_in->sin_addr.s_addr) & 0xff000000) == 0x7f000000)
          saw_v4_localhost = true;
        else
          return false;
        break;
      }
      case AF_INET6: {
        const struct sockaddr_in6* addr_in6 =
            reinterpret_cast<struct sockaddr_in6*>(ai->ai_addr);
        if (IN6_IS_ADDR_LOOPBACK(&addr_in6->sin6_addr))
          saw_v6_localhost = true;
        else
          return false;
        break;
      }
      default:
        NOTREACHED();
        return false;
    }
  }

  return saw_v4_localhost != saw_v6_localhost;
}

}  // namespace

HostResolverProc* HostResolverProc::default_proc_ = NULL;

HostResolverProc::HostResolverProc(HostResolverProc* previous) {
  SetPreviousProc(previous);

  // Implicitly fall-back to the global default procedure.
  if (!previous)
    SetPreviousProc(default_proc_);
}

HostResolverProc::~HostResolverProc() {
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

int SystemHostResolverProc(const std::string& host,
                           AddressFamily address_family,
                           HostResolverFlags host_resolver_flags,
                           AddressList* addrlist,
                           int* os_error) {
  static const size_t kMaxHostLength = 4096;

  if (os_error)
    *os_error = 0;

  // The result of |getaddrinfo| for empty hosts is inconsistent across systems.
  // On Windows it gives the default interface's address, whereas on Linux it
  // gives an error. We will make it fail on all platforms for consistency.
  if (host.empty())
    return ERR_NAME_NOT_RESOLVED;

  // Limit the size of hostnames that will be resolved to combat issues in some
  // platform's resolvers.
  if (host.size() > kMaxHostLength)
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

  // On Linux AI_ADDRCONFIG doesn't consider loopback addreses, even if only
  // loopback addresses are configured. So don't use it when there are only
  // loopback addresses.
  if (host_resolver_flags & HOST_RESOLVER_LOOPBACK_ONLY)
    hints.ai_flags &= ~AI_ADDRCONFIG;

  if (host_resolver_flags & HOST_RESOLVER_CANONNAME)
    hints.ai_flags |= AI_CANONNAME;

  // Restrict result set to only this socket type to avoid duplicates.
  hints.ai_socktype = SOCK_STREAM;

  int err = getaddrinfo(host.c_str(), NULL, &hints, &ai);
  bool should_retry = false;
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_OPENBSD)
  // If we fail, re-initialise the resolver just in case there have been any
  // changes to /etc/resolv.conf and retry. See http://crbug.com/11380 for info.
  if (err && DnsReloadTimerHasExpired()) {
    // When there's no network connection, _res may not be initialized by
    // getaddrinfo. Therefore, we call res_nclose only when there are ns
    // entries.
    if (_res.nscount > 0)
      res_nclose(&_res);
    if (!res_ninit(&_res))
      should_retry = true;
  }
#endif
  // If the lookup was restricted (either by address family, or address
  // detection), and the results where all localhost of a single family,
  // maybe we should retry.  There were several bugs related to these
  // issues, for example http://crbug.com/42058 and http://crbug.com/49024
  if ((hints.ai_family != AF_UNSPEC || hints.ai_flags & AI_ADDRCONFIG) &&
      err == 0 && IsAllLocalhostOfOneFamily(ai)) {
    if (host_resolver_flags & HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6) {
      hints.ai_family = AF_UNSPEC;
      should_retry = true;
    }
    if (hints.ai_flags & AI_ADDRCONFIG) {
      hints.ai_flags &= ~AI_ADDRCONFIG;
      should_retry = true;
    }
  }
  if (should_retry) {
    if (ai != NULL) {
      freeaddrinfo(ai);
      ai = NULL;
    }
    err = getaddrinfo(host.c_str(), NULL, &hints, &ai);
  }

  if (err) {
#if defined(OS_WIN)
    err = WSAGetLastError();
#endif

    // Return the OS error to the caller.
    if (os_error)
      *os_error = err;

    // If the call to getaddrinfo() failed because of a system error, report
    // it separately from ERR_NAME_NOT_RESOLVED.
#if defined(OS_WIN)
    if (err != WSAHOST_NOT_FOUND && err != WSANO_DATA)
      return ERR_NAME_RESOLUTION_FAILED;
#elif defined(OS_POSIX)
    if (err != EAI_NONAME && err != EAI_NODATA)
      return ERR_NAME_RESOLUTION_FAILED;
#endif

    return ERR_NAME_NOT_RESOLVED;
  }

  addrlist->Adopt(ai);
  return OK;
}

}  // namespace net
