// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/mock_host_resolver.h"

#include "base/string_util.h"
#include "base/platform_thread.h"
#include "base/ref_counted.h"
#include "net/base/net_errors.h"

namespace net {

MockHostResolver::MockHostResolver() {
  Reset(NULL, 0, 0);
}

int MockHostResolver::Resolve(const RequestInfo& info,
                              AddressList* addresses,
                              CompletionCallback* callback,
                              RequestHandle* out_req) {
  return impl_->Resolve(info, addresses, callback, out_req);
}

void MockHostResolver::CancelRequest(RequestHandle req) {
  impl_->CancelRequest(req);
}

void MockHostResolver::AddObserver(Observer* observer) {
  impl_->AddObserver(observer);
}

void MockHostResolver::RemoveObserver(Observer* observer) {
  impl_->RemoveObserver(observer);
}

void MockHostResolver::Shutdown() {
  impl_->Shutdown();
}

void MockHostResolver::Reset(HostResolverProc* interceptor,
                             int max_cache_entries,
                             int max_cache_age_ms) {
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

  impl_ = new HostResolverImpl(proc, max_cache_entries, max_cache_age_ms);
}

//-----------------------------------------------------------------------------

struct RuleBasedHostResolverProc::Rule {
  std::string host_pattern;
  std::string replacement;
  int latency_ms;  // In milliseconds.
  bool direct;     // if true, don't mangle hostname and ignore replacement
  Rule(const std::string& host_pattern, const std::string& replacement)
      : host_pattern(host_pattern),
        replacement(replacement),
        latency_ms(0),
        direct(false) {}
  Rule(const std::string& host_pattern, const std::string& replacement,
       const int latency_ms)
      : host_pattern(host_pattern),
        replacement(replacement),
        latency_ms(latency_ms),
        direct(false) {}
  Rule(const std::string& host_pattern, const std::string& replacement,
       const bool direct)
      : host_pattern(host_pattern),
        replacement(replacement),
        latency_ms(0),
        direct(direct) {}
};

RuleBasedHostResolverProc::RuleBasedHostResolverProc(HostResolverProc* previous)
    : HostResolverProc(previous) {
}

RuleBasedHostResolverProc::~RuleBasedHostResolverProc() {
}

void RuleBasedHostResolverProc::AddRule(const std::string& host_pattern,
                                        const std::string& replacement) {
  rules_.push_back(Rule(host_pattern, replacement));
}

void RuleBasedHostResolverProc::AddRuleWithLatency(
    const std::string& host_pattern,
    const std::string& replacement, int latency_ms) {
  rules_.push_back(Rule(host_pattern, replacement, latency_ms));
}

void RuleBasedHostResolverProc::AllowDirectLookup(const std::string& host) {
  rules_.push_back(Rule(host, "", true));
}

void RuleBasedHostResolverProc::AddSimulatedFailure(const std::string& host) {
  AddRule(host, "");
}

int RuleBasedHostResolverProc::Resolve(const std::string& host,
                                       AddressList* addrlist) {
  RuleList::iterator r;
  for (r = rules_.begin(); r != rules_.end(); ++r) {
    if (MatchPattern(host, r->host_pattern)) {
      if (r->latency_ms != 0) {
        PlatformThread::Sleep(r->latency_ms);
        // Hmm, this seems unecessary.
        r->latency_ms = 1;
      }
      const std::string& effective_host = r->direct ? host : r->replacement;
      if (effective_host.empty())
        return ERR_NAME_NOT_RESOLVED;
      return SystemHostResolverProc(effective_host, addrlist);
    }
  }
  return ResolveUsingPrevious(host, addrlist);
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
