// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/mapped_host_resolver.h"

#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "net/base/net_util.h"

namespace net {

MappedHostResolver::MappedHostResolver(HostResolver* impl)
    : impl_(impl) {
}

int MappedHostResolver::Resolve(const RequestInfo& info,
                                AddressList* addresses,
                                CompletionCallback* callback,
                                RequestHandle* out_req,
                                LoadLog* load_log) {
  // Modify the request before forwarding it to |impl_|.
  RequestInfo modified_info = info;
  RewriteRequest(&modified_info);
  return impl_->Resolve(modified_info, addresses, callback, out_req, load_log);
}

void MappedHostResolver::CancelRequest(RequestHandle req) {
  impl_->CancelRequest(req);
}

void MappedHostResolver::AddObserver(Observer* observer) {
  impl_->AddObserver(observer);
}

void MappedHostResolver::RemoveObserver(Observer* observer) {
  impl_->RemoveObserver(observer);
}

HostResolverImpl* MappedHostResolver::GetAsHostResolverImpl() {
  return impl_->GetAsHostResolverImpl();
}

MappedHostResolver::~MappedHostResolver() {
}

bool MappedHostResolver::RewriteRequest(RequestInfo* info) const {
  // Check if the hostname was excluded.
  for (ExclusionRuleList::const_iterator it = exclusion_rules_.begin();
       it != exclusion_rules_.end(); ++it) {
    const ExclusionRule& rule = *it;
    if (MatchPatternASCII(info->hostname(), rule.hostname_pattern))
      return false;
  }

  // Check if the hostname was remapped.
  for (MapRuleList::const_iterator it = map_rules_.begin();
       it != map_rules_.end(); ++it) {
    const MapRule& rule = *it;

    if (!MatchPatternASCII(info->hostname(), rule.hostname_pattern))
      continue;  // This rule doesn't apply.

    info->set_hostname(rule.replacement_hostname);
    if (rule.replacement_port != -1)
      info->set_port(rule.replacement_port);
    return true;
  }

  return false;
}

bool MappedHostResolver::AddRuleFromString(const std::string& rule_string) {
  std::string trimmed;
  TrimWhitespaceASCII(rule_string, TRIM_ALL, &trimmed);
  std::vector<std::string> parts;
  SplitString(trimmed, ' ', &parts);

  // Test for EXCLUSION rule.
  if (parts.size() == 2 && LowerCaseEqualsASCII(parts[0], "exclude")) {
    ExclusionRule rule;
    rule.hostname_pattern = StringToLowerASCII(parts[1]);
    exclusion_rules_.push_back(rule);
    return true;
  }

  // Test for MAP rule.
  if (parts.size() == 3 && LowerCaseEqualsASCII(parts[0], "map")) {
    MapRule rule;
    rule.hostname_pattern = StringToLowerASCII(parts[1]);

    if (!ParseHostAndPort(parts[2], &rule.replacement_hostname,
                          &rule.replacement_port)) {
      return false;  // Failed parsing the hostname/port.
    }

    map_rules_.push_back(rule);
    return true;
  }

  return false;
}

void MappedHostResolver::SetRulesFromString(const std::string& rules_string) {
  exclusion_rules_.clear();
  map_rules_.clear();

  StringTokenizer rules(rules_string, ",");
  while (rules.GetNext()) {
    bool ok = AddRuleFromString(rules.token());
    LOG_IF(ERROR, !ok) << "Failed parsing rule: " << rules.token();
  }
}

}  // namespace net
