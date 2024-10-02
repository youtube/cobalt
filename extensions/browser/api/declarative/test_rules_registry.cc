// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "extensions/browser/api/declarative/test_rules_registry.h"

namespace extensions {

TestRulesRegistry::TestRulesRegistry(content::BrowserThread::ID owner_thread,
                                     const std::string& event_name,
                                     int rules_registry_id)
    : RulesRegistry(nullptr /*profile*/,
                    event_name,
                    owner_thread,
                    nullptr,
                    rules_registry_id) {}

TestRulesRegistry::TestRulesRegistry(content::BrowserContext* browser_context,
                                     const std::string& event_name,
                                     content::BrowserThread::ID owner_thread,
                                     RulesCacheDelegate* cache_delegate,
                                     int rules_registry_id)
    : RulesRegistry(browser_context,
                    event_name,
                    owner_thread,
                    cache_delegate,
                    rules_registry_id) {
}

std::string TestRulesRegistry::AddRulesImpl(
    const std::string& extension_id,
    const std::vector<const api::events::Rule*>& rules) {
  return result_;
}

std::string TestRulesRegistry::RemoveRulesImpl(
    const std::string& extension_id,
    const std::vector<std::string>& rule_identifiers) {
  return result_;
}

std::string TestRulesRegistry::RemoveAllRulesImpl(
    const std::string& extension_id) {
  return result_;
}

void TestRulesRegistry::SetResult(const std::string& result) {
  result_ = result;
}

TestRulesRegistry::~TestRulesRegistry() = default;

}  // namespace extensions
