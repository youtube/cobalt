// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_MAPPED_HOST_RESOLVER_H_
#define NET_BASE_MAPPED_HOST_RESOLVER_H_

#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "net/base/host_resolver.h"

namespace net {

// This class wraps an existing HostResolver instance, but modifies the
// request before passing it off to |impl|. This is different from
// MockHostResolver which does the remapping at the HostResolverProc
// layer, so it is able to preserve the effectiveness of the cache.
class MappedHostResolver : public HostResolver {
 public:
  // Creates a MappedHostResolver that forwards all of its requests through
  // |impl|.
  explicit MappedHostResolver(HostResolver* impl);

  // HostResolver methods:
  virtual int Resolve(const RequestInfo& info,
                      AddressList* addresses,
                      CompletionCallback* callback,
                      RequestHandle* out_req,
                      const BoundNetLog& net_log);
  virtual void CancelRequest(RequestHandle req);
  virtual void AddObserver(Observer* observer);
  virtual void RemoveObserver(Observer* observer);
  virtual void Flush() { impl_->Flush(); }
  virtual HostResolverImpl* GetAsHostResolverImpl();

  // Adds a rule to this mapper. The format of the rule can be one of:
  //
  //   "MAP" <hostname_pattern> <replacement_host> [":" <replacement_port>]
  //   "EXCLUDE" <hostname_pattern>
  //
  // The <replacement_host> can be either a hostname, or an IP address literal.
  //
  // Returns true if the rule was successfully parsed and added.
  bool AddRuleFromString(const std::string& rule_string);

  // Takes a comma separated list of rules, and assigns them to this resolver.
  void SetRulesFromString(const std::string& rules_string);

 private:
  struct MapRule {
    MapRule() : replacement_port(-1) {}

    std::string hostname_pattern;
    std::string replacement_hostname;
    int replacement_port;
  };

  struct ExclusionRule {
    std::string hostname_pattern;
  };

  typedef std::vector<MapRule> MapRuleList;
  typedef std::vector<ExclusionRule> ExclusionRuleList;

  virtual ~MappedHostResolver();

  // Modifies |*info| based on the current rules. Returns true if the
  // RequestInfo was modified, false otherwise.
  bool RewriteRequest(RequestInfo* info) const;

  scoped_refptr<HostResolver> impl_;

  MapRuleList map_rules_;
  ExclusionRuleList exclusion_rules_;
};

}  // namespace net

#endif  // NET_BASE_MAPPED_HOST_RESOLVER_H_
