// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_MOCK_HOST_RESOLVER_H_
#define NET_BASE_MOCK_HOST_RESOLVER_H_

#include <list>

#include "base/waitable_event.h"
#include "net/base/host_resolver_impl.h"
#include "net/base/host_resolver_proc.h"

namespace net {

class RuleBasedHostResolverProc;

// In most cases, it is important that unit tests avoid making actual DNS
// queries since the resulting tests can be flaky, especially if the network is
// unreliable for some reason.  To simplify writing tests that avoid making
// actual DNS queries, pass a MockHostResolver as the HostResolver dependency.
// The socket addresses returned can be configured using the
// RuleBasedHostResolverProc:
//
//   host_resolver->rules()->AddRule("foo.com", "1.2.3.4");
//   host_resolver->rules()->AddRule("bar.com", "2.3.4.5");
//
// The above rules define a static mapping from hostnames to IP address
// literals.  The first parameter to AddRule specifies a host pattern to match
// against, and the second parameter indicates what value should be used to
// replace the given hostname.  So, the following is also supported:
//
//   host_mapper->AddRule("*.com", "127.0.0.1");
//
// Replacement doesn't have to be string representing an IP address. It can
// re-map one hostname to another as well.
class MockHostResolver : public HostResolver {
 public:
  // Creates a MockHostResolver that does NOT cache entries
  // (the HostResolverProc will be called for every lookup). If you need
  // caching behavior, call Reset() with non-zero cache size.
  MockHostResolver();

  virtual ~MockHostResolver() {}

  // HostResolver methods:
  virtual int Resolve(const RequestInfo& info, AddressList* addresses,
                      CompletionCallback* callback, RequestHandle* out_req);
  virtual void CancelRequest(RequestHandle req);
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);
  // TODO(eroman): temp hack for http://crbug.com/15513
  virtual void Shutdown();

  RuleBasedHostResolverProc* rules() { return rules_; }

  // Resets the mock.
  void Reset(HostResolverProc* interceptor,
             int max_cache_entries,
             int max_cache_age_ms);

 private:
  scoped_refptr<HostResolverImpl> impl_;
  scoped_refptr<RuleBasedHostResolverProc> rules_;
};

// RuleBasedHostResolverProc applies a set of rules to map a host string to
// a replacement host string. It then uses the system host resolver to return
// a socket address. Generally the replacement should be an IPv4 literal so
// there is no network dependency.
class RuleBasedHostResolverProc : public HostResolverProc {
 public:
  explicit RuleBasedHostResolverProc(HostResolverProc* previous);
  ~RuleBasedHostResolverProc();

  // Any hostname matching the given pattern will be replaced with the given
  // replacement value.  Usually, replacement should be an IP address literal.
  void AddRule(const std::string& host_pattern,
               const std::string& replacement);

  void AddRuleWithLatency(const std::string& host_pattern,
                          const std::string& replacement,
                          int latency_ms);

  // Make sure that |host| will not be re-mapped or even processed by underlying
  // host resolver procedures. It can also be a pattern.
  void AllowDirectLookup(const std::string& host);

  // Simulate a lookup failure for |host| (it also can be a pattern).
  void AddSimulatedFailure(const std::string& host);

  // HostResolverProc methods:
  virtual int Resolve(const std::string& host, AddressList* addrlist);

 private:
  struct Rule;
  typedef std::list<Rule> RuleList;
  RuleList rules_;
};

// Using WaitingHostResolverProc you can simulate very long lookups.
class WaitingHostResolverProc : public HostResolverProc {
 public:
  explicit WaitingHostResolverProc(HostResolverProc* previous)
      : HostResolverProc(previous), event_(false, false) {}

  void Signal() {
    event_.Signal();
  }

  // HostResolverProc methods:
  virtual int Resolve(const std::string& host, AddressList* addrlist) {
    event_.Wait();
    return ResolveUsingPrevious(host, addrlist);
  }

  base::WaitableEvent event_;
};

// This class sets the HostResolverProc for a particular scope. If there are
// multiple ScopedDefaultHostResolverProc in existence, then the last one
// allocated will be used. However, if it does not provide a matching rule,
// then it should delegate to the previously set HostResolverProc.
//
// NOTE: Only use this as a catch-all safety net. Individual tests should use
// MockHostResolver.
class ScopedDefaultHostResolverProc {
 public:
  ScopedDefaultHostResolverProc() {}
  explicit ScopedDefaultHostResolverProc(HostResolverProc* proc);

  ~ScopedDefaultHostResolverProc();

  void Init(HostResolverProc* proc);

 private:
  scoped_refptr<HostResolverProc> current_proc_;
  scoped_refptr<HostResolverProc> previous_proc_;
};

}  // namespace net

#endif  // NET_BASE_MOCK_HOST_RESOLVER_H_
