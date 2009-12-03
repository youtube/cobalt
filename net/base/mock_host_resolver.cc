// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/mock_host_resolver.h"

#include "base/string_util.h"
#include "base/platform_thread.h"
#include "base/ref_counted.h"
#include "googleurl/src/url_canon_ip.h"
#include "net/base/net_errors.h"

namespace net {

namespace {
// Fills |addrlist| with a socket address for |host| which should be an
// IPv6 literal. Returns OK on success.
int ResolveIPV6LiteralUsingGURL(const std::string& host,
                                AddressList* addrlist) {
  // GURL expects the hostname to be surrounded with brackets.
  std::string host_brackets = "[" + host + "]";
  url_parse::Component host_comp(0, host_brackets.size());

  // Try parsing the hostname as an IPv6 literal.
  unsigned char ipv6_addr[16];  // 128 bits.
  bool ok = url_canon::IPv6AddressToNumber(host_brackets.data(),
                                           host_comp,
                                           ipv6_addr);
  if (!ok) {
    LOG(WARNING) << "Not an IPv6 literal: " << host;
    return ERR_UNEXPECTED;
  }

  *addrlist = AddressList::CreateIPv6Address(ipv6_addr);
  return OK;
}

}  // namespace

MockHostResolverBase::MockHostResolverBase(bool use_caching)
    : use_caching_(use_caching) {
  Reset(NULL);
}

int MockHostResolverBase::Resolve(const RequestInfo& info,
                                  AddressList* addresses,
                                  CompletionCallback* callback,
                                  RequestHandle* out_req,
                                  LoadLog* load_log) {
  if (synchronous_mode_) {
    callback = NULL;
    out_req = NULL;
  }
  return impl_->Resolve(info, addresses, callback, out_req, load_log);
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

HostCache* MockHostResolverBase::GetHostCache() {
  return impl_->GetHostCache();
}

void MockHostResolverBase::Shutdown() {
  impl_->Shutdown();
}

void MockHostResolverBase::Reset(HostResolverProc* interceptor) {
  synchronous_mode_ = false;

  // At the root of the chain, map everything to localhost.
  scoped_refptr<RuleBasedHostResolverProc> catchall =
      new RuleBasedHostResolverProc(NULL);
  catchall->AddRule("*", "127.0.0.1");

  // Next add a rules-based layer the use controls.
  rules_ = new RuleBasedHostResolverProc(catchall);

  HostResolverProc* proc = rules_;

  // Lastly add the provided interceptor to the front of the chain.
  if (interceptor) {
    interceptor->set_previous_proc(proc);
    proc = interceptor;
  }

  int max_cache_entries = use_caching_ ? 100 : 0;
  int max_cache_age_ms = use_caching_ ? 60000 : 0;

  impl_ = new HostResolverImpl(proc, max_cache_entries, max_cache_age_ms);
}

//-----------------------------------------------------------------------------

struct RuleBasedHostResolverProc::Rule {
  enum ResolverType {
    kResolverTypeFail,
    kResolverTypeSystem,
    kResolverTypeIPV6Literal,
  };

  ResolverType resolver_type;
  std::string host_pattern;
  AddressFamily address_family;
  std::string replacement;
  int latency_ms;  // In milliseconds.

  Rule(ResolverType resolver_type,
       const std::string& host_pattern,
       AddressFamily address_family,
       const std::string& replacement,
       int latency_ms)
      : resolver_type(resolver_type),
        host_pattern(host_pattern),
        address_family(address_family),
        replacement(replacement),
        latency_ms(latency_ms) {}
};

RuleBasedHostResolverProc::RuleBasedHostResolverProc(HostResolverProc* previous)
    : HostResolverProc(previous) {
}

RuleBasedHostResolverProc::~RuleBasedHostResolverProc() {
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
  Rule rule(Rule::kResolverTypeSystem, host_pattern,
            address_family, replacement, 0);
  rules_.push_back(rule);
}

void RuleBasedHostResolverProc::AddIPv6Rule(const std::string& host_pattern,
                                            const std::string& ipv6_literal) {
  Rule rule(Rule::kResolverTypeIPV6Literal, host_pattern,
            ADDRESS_FAMILY_UNSPECIFIED, ipv6_literal, 0);
  rules_.push_back(rule);
}

void RuleBasedHostResolverProc::AddRuleWithLatency(
    const std::string& host_pattern,
    const std::string& replacement,
    int latency_ms) {
  DCHECK(!replacement.empty());
  Rule rule(Rule::kResolverTypeSystem, host_pattern,
            ADDRESS_FAMILY_UNSPECIFIED, replacement, latency_ms);
  rules_.push_back(rule);
}

void RuleBasedHostResolverProc::AllowDirectLookup(
    const std::string& host_pattern) {
  Rule rule(Rule::kResolverTypeSystem, host_pattern,
            ADDRESS_FAMILY_UNSPECIFIED, "", 0);
  rules_.push_back(rule);
}

void RuleBasedHostResolverProc::AddSimulatedFailure(
    const std::string& host_pattern) {
  Rule rule(Rule::kResolverTypeFail, host_pattern,
            ADDRESS_FAMILY_UNSPECIFIED, "", 0);
  rules_.push_back(rule);
}

int RuleBasedHostResolverProc::Resolve(const std::string& host,
                                       AddressFamily address_family,
                                       AddressList* addrlist) {
  RuleList::iterator r;
  for (r = rules_.begin(); r != rules_.end(); ++r) {
    bool matches_address_family =
        r->address_family == ADDRESS_FAMILY_UNSPECIFIED ||
        r->address_family == address_family;

    if (matches_address_family && MatchPatternASCII(host, r->host_pattern)) {
      if (r->latency_ms != 0)
        PlatformThread::Sleep(r->latency_ms);

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
                                        addrlist);
        case Rule::kResolverTypeIPV6Literal:
          return ResolveIPV6LiteralUsingGURL(effective_host, addrlist);
        default:
          NOTREACHED();
          return ERR_UNEXPECTED;
      }
    }
  }
  return ResolveUsingPrevious(host, address_family, addrlist);
}

//-----------------------------------------------------------------------------

ScopedDefaultHostResolverProc::ScopedDefaultHostResolverProc(
    HostResolverProc* proc) : current_proc_(proc) {
  previous_proc_ = HostResolverProc::SetDefault(current_proc_);
  current_proc_->set_previous_proc(previous_proc_);
}

ScopedDefaultHostResolverProc::~ScopedDefaultHostResolverProc() {
  HostResolverProc* old_proc = HostResolverProc::SetDefault(previous_proc_);
  // The lifetimes of multiple instances must be nested.
  CHECK(old_proc == current_proc_);
}

void ScopedDefaultHostResolverProc::Init(HostResolverProc* proc) {
  current_proc_ = proc;
  previous_proc_ = HostResolverProc::SetDefault(current_proc_);
  current_proc_->set_previous_proc(previous_proc_);
}

}  // namespace net
