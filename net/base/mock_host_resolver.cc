// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/mock_host_resolver.h"

#include "base/string_split.h"
#include "base/string_util.h"
#include "base/ref_counted.h"
#include "base/threading/platform_thread.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/base/sys_addrinfo.h"

namespace net {

namespace {

char* do_strdup(const char* src) {
#if defined(OS_WIN)
  return _strdup(src);
#else
  return strdup(src);
#endif
}

// Fills |*addrlist| with a socket address for |host_list| which should be a
// comma-separated list of IPv4 or IPv6 literal(s) without enclosing brackets.
// If |canonical_name| is non-empty it is used as the DNS canonical name for
// the host. Returns OK on success, ERR_UNEXPECTED otherwise.
int CreateIPAddressList(const std::string& host_list,
                        const std::string& canonical_name,
                        AddressList* addrlist) {
  *addrlist = AddressList();
  std::vector<std::string> addresses;
  base::SplitString(host_list, ',', &addresses);
  for (size_t index = 0; index < addresses.size(); ++index) {
    IPAddressNumber ip_number;
    if (!ParseIPLiteralToNumber(addresses[index], &ip_number)) {
      LOG(WARNING) << "Not a supported IP literal: " << addresses[index];
      return ERR_UNEXPECTED;
    }

    AddressList result(ip_number, -1, false);
    struct addrinfo* ai = const_cast<struct addrinfo*>(result.head());
    if (index == 0)
      ai->ai_canonname = do_strdup(canonical_name.c_str());
    if (!addrlist->head())
      addrlist->Copy(result.head(), false);
    else
      addrlist->Append(result.head());
  }
  return OK;
}

}  // namespace

MockHostResolverBase::~MockHostResolverBase() {}

void MockHostResolverBase::Reset(HostResolverProc* interceptor) {
  synchronous_mode_ = false;

  // At the root of the chain, map everything to localhost.
  scoped_refptr<RuleBasedHostResolverProc> catchall(
      new RuleBasedHostResolverProc(NULL));
  catchall->AddRule("*", "127.0.0.1");

  // Next add a rules-based layer the use controls.
  rules_ = new RuleBasedHostResolverProc(catchall);

  HostResolverProc* proc = rules_;

  // Lastly add the provided interceptor to the front of the chain.
  if (interceptor) {
    interceptor->SetPreviousProc(proc);
    proc = interceptor;
  }

  HostCache* cache = NULL;

  if (use_caching_) {
    cache = new HostCache(
        100,  // max entries.
        base::TimeDelta::FromMinutes(1),
        base::TimeDelta::FromSeconds(0));
  }

  impl_.reset(new HostResolverImpl(proc, cache, 50u, NULL));
}

int MockHostResolverBase::Resolve(const RequestInfo& info,
                                  AddressList* addresses,
                                  CompletionCallback* callback,
                                  RequestHandle* out_req,
                                  const BoundNetLog& net_log) {
  if (synchronous_mode_) {
    callback = NULL;
    out_req = NULL;
  }
  return impl_->Resolve(info, addresses, callback, out_req, net_log);
}

void MockHostResolverBase::CancelRequest(RequestHandle req) {
  impl_->CancelRequest(req);
}

void MockHostResolverBase::AddObserver(Observer* observer) {
  impl_->AddObserver(observer);
}

void MockHostResolverBase::RemoveObserver(Observer* observer) {
  impl_->RemoveObserver(observer);
}

MockHostResolverBase::MockHostResolverBase(bool use_caching)
    : use_caching_(use_caching) {
  Reset(NULL);
}

//-----------------------------------------------------------------------------

struct RuleBasedHostResolverProc::Rule {
  enum ResolverType {
    kResolverTypeFail,
    kResolverTypeSystem,
    kResolverTypeIPLiteral,
  };

  ResolverType resolver_type;
  std::string host_pattern;
  AddressFamily address_family;
  HostResolverFlags host_resolver_flags;
  std::string replacement;
  std::string canonical_name;
  int latency_ms;  // In milliseconds.

  Rule(ResolverType resolver_type,
       const std::string& host_pattern,
       AddressFamily address_family,
       HostResolverFlags host_resolver_flags,
       const std::string& replacement,
       const std::string& canonical_name,
       int latency_ms)
      : resolver_type(resolver_type),
        host_pattern(host_pattern),
        address_family(address_family),
        host_resolver_flags(host_resolver_flags),
        replacement(replacement),
        canonical_name(canonical_name),
        latency_ms(latency_ms) {}
};

RuleBasedHostResolverProc::RuleBasedHostResolverProc(HostResolverProc* previous)
    : HostResolverProc(previous) {
}

void RuleBasedHostResolverProc::AddRule(const std::string& host_pattern,
                                        const std::string& replacement) {
  AddRuleForAddressFamily(host_pattern, ADDRESS_FAMILY_UNSPECIFIED,
                          replacement);
}

void RuleBasedHostResolverProc::AddRuleForAddressFamily(
    const std::string& host_pattern,
    AddressFamily address_family,
    const std::string& replacement) {
  DCHECK(!replacement.empty());
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY |
      HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
  Rule rule(Rule::kResolverTypeSystem, host_pattern, address_family, flags,
            replacement, "", 0);
  rules_.push_back(rule);
}

void RuleBasedHostResolverProc::AddIPLiteralRule(
    const std::string& host_pattern,
    const std::string& ip_literal,
    const std::string& canonical_name) {
  // Literals are always resolved to themselves by HostResolverImpl,
  // consequently we do not support remapping them.
  IPAddressNumber ip_number;
  DCHECK(!ParseIPLiteralToNumber(host_pattern, &ip_number));
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY |
      HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
  if (!canonical_name.empty())
    flags |= HOST_RESOLVER_CANONNAME;
  Rule rule(Rule::kResolverTypeIPLiteral, host_pattern,
            ADDRESS_FAMILY_UNSPECIFIED, flags, ip_literal, canonical_name,
            0);
  rules_.push_back(rule);
}

void RuleBasedHostResolverProc::AddRuleWithLatency(
    const std::string& host_pattern,
    const std::string& replacement,
    int latency_ms) {
  DCHECK(!replacement.empty());
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY |
      HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
  Rule rule(Rule::kResolverTypeSystem, host_pattern, ADDRESS_FAMILY_UNSPECIFIED,
            flags, replacement, "", latency_ms);
  rules_.push_back(rule);
}

void RuleBasedHostResolverProc::AllowDirectLookup(
    const std::string& host_pattern) {
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY |
      HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
  Rule rule(Rule::kResolverTypeSystem, host_pattern, ADDRESS_FAMILY_UNSPECIFIED,
            flags, "", "", 0);
  rules_.push_back(rule);
}

void RuleBasedHostResolverProc::AddSimulatedFailure(
    const std::string& host_pattern) {
  HostResolverFlags flags = HOST_RESOLVER_LOOPBACK_ONLY |
      HOST_RESOLVER_DEFAULT_FAMILY_SET_DUE_TO_NO_IPV6;
  Rule rule(Rule::kResolverTypeFail, host_pattern, ADDRESS_FAMILY_UNSPECIFIED,
            flags, "", "", 0);
  rules_.push_back(rule);
}

int RuleBasedHostResolverProc::Resolve(const std::string& host,
                                       AddressFamily address_family,
                                       HostResolverFlags host_resolver_flags,
                                       AddressList* addrlist,
                                       int* os_error) {
  RuleList::iterator r;
  for (r = rules_.begin(); r != rules_.end(); ++r) {
    bool matches_address_family =
        r->address_family == ADDRESS_FAMILY_UNSPECIFIED ||
        r->address_family == address_family;
    // Flags match if all of the bitflags in host_resolver_flags are enabled
    // in the rule's host_resolver_flags. However, the rule may have additional
    // flags specified, in which case the flags should still be considered a
    // match.
    bool matches_flags = (r->host_resolver_flags & host_resolver_flags) ==
        host_resolver_flags;
    if (matches_flags && matches_address_family &&
        MatchPattern(host, r->host_pattern)) {
      if (r->latency_ms != 0)
        base::PlatformThread::Sleep(r->latency_ms);

      // Remap to a new host.
      const std::string& effective_host =
          r->replacement.empty() ? host : r->replacement;

      // Apply the resolving function to the remapped hostname.
      switch (r->resolver_type) {
        case Rule::kResolverTypeFail:
          return ERR_NAME_NOT_RESOLVED;
        case Rule::kResolverTypeSystem:
          return SystemHostResolverProc(effective_host,
                                        address_family,
                                        host_resolver_flags,
                                        addrlist, os_error);
        case Rule::kResolverTypeIPLiteral:
          return CreateIPAddressList(effective_host,
                                     r->canonical_name,
                                     addrlist);
        default:
          NOTREACHED();
          return ERR_UNEXPECTED;
      }
    }
  }
  return ResolveUsingPrevious(host, address_family,
                              host_resolver_flags, addrlist, os_error);
}

RuleBasedHostResolverProc::~RuleBasedHostResolverProc() {
}

//-----------------------------------------------------------------------------

WaitingHostResolverProc::WaitingHostResolverProc(HostResolverProc* previous)
    : HostResolverProc(previous), event_(false, false) {}

void WaitingHostResolverProc::Signal() {
  event_.Signal();
}

int WaitingHostResolverProc::Resolve(const std::string& host,
                                     AddressFamily address_family,
                                     HostResolverFlags host_resolver_flags,
                                     AddressList* addrlist,
                                     int* os_error) {
  event_.Wait();
  return ResolveUsingPrevious(host, address_family, host_resolver_flags,
                              addrlist, os_error);
}

WaitingHostResolverProc::~WaitingHostResolverProc() {}

//-----------------------------------------------------------------------------

ScopedDefaultHostResolverProc::ScopedDefaultHostResolverProc() {}

ScopedDefaultHostResolverProc::ScopedDefaultHostResolverProc(
    HostResolverProc* proc) {
  Init(proc);
}

ScopedDefaultHostResolverProc::~ScopedDefaultHostResolverProc() {
  HostResolverProc* old_proc = HostResolverProc::SetDefault(previous_proc_);
  // The lifetimes of multiple instances must be nested.
  CHECK_EQ(old_proc, current_proc_);
}

void ScopedDefaultHostResolverProc::Init(HostResolverProc* proc) {
  current_proc_ = proc;
  previous_proc_ = HostResolverProc::SetDefault(current_proc_);
  current_proc_->SetLastProc(previous_proc_);
}

}  // namespace net
