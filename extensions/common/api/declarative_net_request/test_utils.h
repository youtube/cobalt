// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_TEST_UTILS_H_
#define EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_TEST_UTILS_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/values.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/url_pattern.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {
namespace declarative_net_request {

struct DictionarySource {
  DictionarySource() = default;
  virtual ~DictionarySource() = default;
  virtual base::Value::Dict ToValue() const = 0;
};

// Helper structs to simplify building base::Values which can later be used to
// serialize to JSON. The generated implementation for the JSON rules schema is
// not used since it's not flexible enough to generate the base::Value/JSON we
// want for tests.
struct TestRuleCondition : public DictionarySource {
  TestRuleCondition();
  ~TestRuleCondition() override;
  TestRuleCondition(const TestRuleCondition&);
  TestRuleCondition& operator=(const TestRuleCondition&);

  absl::optional<std::string> url_filter;
  absl::optional<std::string> regex_filter;
  absl::optional<bool> is_url_filter_case_sensitive;
  absl::optional<std::vector<std::string>> domains;
  absl::optional<std::vector<std::string>> excluded_domains;
  absl::optional<std::vector<std::string>> initiator_domains;
  absl::optional<std::vector<std::string>> excluded_initiator_domains;
  absl::optional<std::vector<std::string>> request_domains;
  absl::optional<std::vector<std::string>> excluded_request_domains;
  absl::optional<std::vector<std::string>> request_methods;
  absl::optional<std::vector<std::string>> excluded_request_methods;
  absl::optional<std::vector<std::string>> resource_types;
  absl::optional<std::vector<std::string>> excluded_resource_types;
  absl::optional<std::vector<int>> tab_ids;
  absl::optional<std::vector<int>> excluded_tab_ids;
  absl::optional<std::string> domain_type;

  base::Value::Dict ToValue() const override;
};

struct TestRuleQueryKeyValue : public DictionarySource {
  TestRuleQueryKeyValue();
  ~TestRuleQueryKeyValue() override;
  TestRuleQueryKeyValue(const TestRuleQueryKeyValue&);
  TestRuleQueryKeyValue& operator=(const TestRuleQueryKeyValue&);

  absl::optional<std::string> key;
  absl::optional<std::string> value;
  absl::optional<bool> replace_only;

  base::Value::Dict ToValue() const override;
};

struct TestRuleQueryTransform : public DictionarySource {
  TestRuleQueryTransform();
  ~TestRuleQueryTransform() override;
  TestRuleQueryTransform(const TestRuleQueryTransform&);
  TestRuleQueryTransform& operator=(const TestRuleQueryTransform&);

  absl::optional<std::vector<std::string>> remove_params;
  absl::optional<std::vector<TestRuleQueryKeyValue>> add_or_replace_params;

  base::Value::Dict ToValue() const override;
};

struct TestRuleTransform : public DictionarySource {
  TestRuleTransform();
  ~TestRuleTransform() override;
  TestRuleTransform(const TestRuleTransform&);
  TestRuleTransform& operator=(const TestRuleTransform&);

  absl::optional<std::string> scheme;
  absl::optional<std::string> host;
  absl::optional<std::string> port;
  absl::optional<std::string> path;
  absl::optional<std::string> query;
  absl::optional<TestRuleQueryTransform> query_transform;
  absl::optional<std::string> fragment;
  absl::optional<std::string> username;
  absl::optional<std::string> password;

  base::Value::Dict ToValue() const override;
};

struct TestRuleRedirect : public DictionarySource {
  TestRuleRedirect();
  ~TestRuleRedirect() override;
  TestRuleRedirect(const TestRuleRedirect&);
  TestRuleRedirect& operator=(const TestRuleRedirect&);

  absl::optional<std::string> extension_path;
  absl::optional<TestRuleTransform> transform;
  absl::optional<std::string> url;
  absl::optional<std::string> regex_substitution;

  base::Value::Dict ToValue() const override;
};

struct TestHeaderInfo : public DictionarySource {
  TestHeaderInfo(std::string header,
                 std::string operation,
                 absl::optional<std::string> value);
  ~TestHeaderInfo() override;
  TestHeaderInfo(const TestHeaderInfo&);
  TestHeaderInfo& operator=(const TestHeaderInfo&);

  absl::optional<std::string> header;
  absl::optional<std::string> operation;
  absl::optional<std::string> value;

  base::Value::Dict ToValue() const override;
};

struct TestRuleAction : public DictionarySource {
  TestRuleAction();
  ~TestRuleAction() override;
  TestRuleAction(const TestRuleAction&);
  TestRuleAction& operator=(const TestRuleAction&);

  absl::optional<std::string> type;
  absl::optional<std::vector<TestHeaderInfo>> request_headers;
  absl::optional<std::vector<TestHeaderInfo>> response_headers;
  absl::optional<TestRuleRedirect> redirect;

  base::Value::Dict ToValue() const override;
};

struct TestRule : public DictionarySource {
  TestRule();
  ~TestRule() override;
  TestRule(const TestRule&);
  TestRule& operator=(const TestRule&);

  absl::optional<int> id;
  absl::optional<int> priority;
  absl::optional<TestRuleCondition> condition;
  absl::optional<TestRuleAction> action;

  base::Value::Dict ToValue() const override;
};

// Helper function to build a generic TestRule.
TestRule CreateGenericRule(int id = kMinValidID);

// Helper function to build a generic regex TestRule.
TestRule CreateRegexRule(int id = kMinValidID);

// Bitmasks to configure the extension under test.
enum ConfigFlag {
  kConfig_None = 0,

  // Whether a background script ("background.js") will be persisted for the
  // extension. Clients can listen in to the "ready" message from the background
  // page to detect its loading.
  kConfig_HasBackgroundScript = 1 << 0,

  // Whether the extension has the declarativeNetRequestFeedback permission.
  kConfig_HasFeedbackPermission = 1 << 1,

  // Whether the extension has the activeTab permission.
  kConfig_HasActiveTab = 1 << 2,

  // Whether the "declarative_net_request" manifest key should be omitted.
  kConfig_OmitDeclarativeNetRequestKey = 1 << 3,

  // Whether the "declarativeNetRequest" permission should be omitted.
  kConfig_OmitDeclarativeNetRequestPermission = 1 << 4,

  // Whether the "declarativeNetRequestWithHostAccess" permission should be
  // included.
  kConfig_HasDelarativeNetRequestWithHostAccessPermission = 1 << 5,
};

// Describes a single extension ruleset.
struct TestRulesetInfo {
  TestRulesetInfo(const std::string& manifest_id_and_path,
                  base::Value::List rules_value,
                  bool enabled = true);
  TestRulesetInfo(const std::string& manifest_id,
                  const std::string& relative_file_path,
                  base::Value::List rules_value,
                  bool enabled = true);
  // Used to support the copy ctor, or to deliberately create `rules_value` of
  // the wrong type.
  TestRulesetInfo(const std::string& manifest_id,
                  const std::string& relative_file_path,
                  const base::Value& rules_value,
                  bool enabled = true);
  TestRulesetInfo(const TestRulesetInfo&);
  TestRulesetInfo& operator=(const TestRulesetInfo&);

  // Unique ID for the ruleset.
  const std::string manifest_id;

  // File path relative to the extension directory.
  const std::string relative_file_path;

  // The base::Value corresponding to the rules in the ruleset.
  const base::Value rules_value;

  // Whether the ruleset is enabled by default.
  const bool enabled;

  // Returns the corresponding value to be specified in the manifest for the
  // ruleset.
  base::Value::Dict GetManifestValue() const;
};

// Helper to build an extension manifest which uses the
// kDeclarativeNetRequestKey manifest key. |hosts| specifies the host
// permissions to grant. |flags| is a bitmask of ConfigFlag to configure the
// extension. |ruleset_info| specifies the static rulesets for the extension.
base::Value::Dict CreateManifest(
    const std::vector<TestRulesetInfo>& ruleset_info,
    const std::vector<std::string>& hosts = {},
    unsigned flags = ConfigFlag::kConfig_None,
    const std::string& extension_name = "Test Extension");

// Returns a base::Value::List corresponding to a vector of strings.
base::Value::List ToListValue(const std::vector<std::string>& vec);

// Returns a base::Value::List corresponding to a vector of TestRules.
base::Value::List ToListValue(const std::vector<TestRule>& rules);

// Writes the rulesets specified in |ruleset_info| in the given |extension_dir|
// together with the manifest file. |hosts| specifies the host permissions, the
// extensions should have. |flags| is a bitmask of ConfigFlag to configure the
// extension.
void WriteManifestAndRulesets(
    const base::FilePath& extension_dir,
    const std::vector<TestRulesetInfo>& ruleset_info,
    const std::vector<std::string>& hosts,
    unsigned flags = ConfigFlag::kConfig_None,
    const std::string& extension_name = "Test Extension");

// Specialization of WriteManifestAndRulesets above for an extension with a
// single static ruleset.
void WriteManifestAndRuleset(
    const base::FilePath& extension_dir,
    const TestRulesetInfo& ruleset_info,
    const std::vector<std::string>& hosts,
    unsigned flags = ConfigFlag::kConfig_None,
    const std::string& extension_name = "Test Extension");

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_COMMON_API_DECLARATIVE_NET_REQUEST_TEST_UTILS_H_
