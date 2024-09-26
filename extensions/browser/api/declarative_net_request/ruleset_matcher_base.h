// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MATCHER_BASE_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MATCHER_BASE_H_

#include <map>
#include <vector>

#include "content/public/browser/global_routing_id.h"
#include "extensions/browser/api/declarative_net_request/flat/extension_ruleset_generated.h"
#include "extensions/browser/api/declarative_net_request/request_action.h"
#include "extensions/common/api/declarative_net_request/constants.h"
#include "extensions/common/extension_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GURL;

namespace content {
class NavigationHandle;
class RenderFrameHost;
}  // namespace content

namespace extensions {

namespace declarative_net_request {
struct RequestParams;

// An abstract class for rule matchers. Overridden by different kinds of
// matchers, e.g. filter lists and regex.
class RulesetMatcherBase {
 public:
  RulesetMatcherBase(const ExtensionId& extension_id, RulesetID ruleset_id);

  RulesetMatcherBase(const RulesetMatcherBase&) = delete;
  RulesetMatcherBase& operator=(const RulesetMatcherBase&) = delete;

  virtual ~RulesetMatcherBase();

  // Returns the ruleset's highest priority matching RequestAction for the
  // onBeforeRequest phase, or absl::nullopt if the ruleset has no matching
  // rule. Also takes into account any matching allowAllRequests rules for the
  // ancestor frames.
  absl::optional<RequestAction> GetBeforeRequestAction(
      const RequestParams& params) const;

  // Returns a vector of RequestAction for all matching modifyHeaders rules
  // with priority greater than |min_priority| if specified.
  virtual std::vector<RequestAction> GetModifyHeadersActions(
      const RequestParams& params,
      absl::optional<uint64_t> min_priority) const = 0;

  // Returns whether this modifies "extraHeaders".
  virtual bool IsExtraHeadersMatcher() const = 0;

  // Returns the number of rules in this matcher.
  virtual size_t GetRulesCount() const = 0;

  // Returns the extension ID with which this matcher is associated.
  const ExtensionId& extension_id() const { return extension_id_; }

  // The ruleset ID of the matcher.
  RulesetID ruleset_id() const { return ruleset_id_; }

  void OnRenderFrameCreated(content::RenderFrameHost* host);
  void OnRenderFrameDeleted(content::RenderFrameHost* host);
  void OnDidFinishNavigation(content::NavigationHandle* navigation_handle);

  // Returns the tracked highest priority matching allowsAllRequests action, if
  // any, for |host|.
  absl::optional<RequestAction> GetAllowlistedFrameActionForTesting(
      content::RenderFrameHost* host) const;

 protected:
  using ExtensionMetadataList =
      ::flatbuffers::Vector<flatbuffers::Offset<flat::UrlRuleMetadata>>;

  // Helper to create a RequestAction of type |BLOCK| or |COLLAPSE|.
  RequestAction CreateBlockOrCollapseRequestAction(
      const RequestParams& params,
      const url_pattern_index::flat::UrlRule& rule) const;

  // Helper to create a RequestAction of type |ALLOW|.
  RequestAction CreateAllowAction(
      const RequestParams& params,
      const url_pattern_index::flat::UrlRule& rule) const;

  // Helper to create a RequestAction of type |ALLOW_ALL_REQUESTS|.
  RequestAction CreateAllowAllRequestsAction(
      const RequestParams& params,
      const url_pattern_index::flat::UrlRule& rule) const;

  // Helper to create a RequestAction of type |REDIRECT| with the request
  // upgraded. Returns absl::nullopt if the request is not upgradeable.
  absl::optional<RequestAction> CreateUpgradeAction(
      const RequestParams& params,
      const url_pattern_index::flat::UrlRule& rule) const;

  // Helpers to create a RequestAction of type |REDIRECT| with the appropriate
  // redirect url. Can return absl::nullopt if the redirect url is ill-formed or
  // same as the current request url.
  absl::optional<RequestAction> CreateRedirectActionFromMetadata(
      const RequestParams& params,
      const url_pattern_index::flat::UrlRule& rule,
      const ExtensionMetadataList& metadata_list) const;
  absl::optional<RequestAction> CreateRedirectAction(
      const RequestParams& params,
      const url_pattern_index::flat::UrlRule& rule,
      GURL redirect_url) const;

  // Helper to create a list of RequestActions of type |MODIFY_HEADERS| with the
  // appropriate list of headers for each action.
  std::vector<RequestAction> GetModifyHeadersActionsFromMetadata(
      const RequestParams& params,
      const std::vector<const url_pattern_index::flat::UrlRule*>& rules,
      const ExtensionMetadataList& metadata_list) const;

 private:
  // Returns the ruleset's highest priority matching allowAllRequests action or
  // absl::nullopt if there is no corresponding matching rule. Only takes into
  // account the request |params| passed in. This doesn't take any account any
  // matching allowAllRequests rules for ancestor frames.
  virtual absl::optional<RequestAction> GetAllowAllRequestsAction(
      const RequestParams& params) const = 0;

  // Returns the ruleset's highest priority matching RequestAction for the
  // onBeforeRequest phase, or absl::nullopt if the ruleset has no matching
  // rule. This doesn't take any account any matching allowAllRequests rules for
  // ancestor frames.
  virtual absl::optional<RequestAction> GetBeforeRequestActionIgnoringAncestors(
      const RequestParams& params) const = 0;

  RequestAction CreateRequestAction(
      RequestAction::Type type,
      const url_pattern_index::flat::UrlRule& rule) const;

  // Returns the matching RequestAction from |allowlisted_frames_| or
  // absl::nullopt if none is found.
  absl::optional<RequestAction> GetAllowlistedFrameAction(
      content::GlobalRenderFrameHostId frame_id) const;

  const ExtensionId extension_id_;
  const RulesetID ruleset_id_;

  // Stores the IDs for the RenderFrameHosts which are allow-listed due to an
  // allowAllRequests action and the corresponding highest priority
  // RequestAction.
  std::map<content::GlobalRenderFrameHostId, const RequestAction>
      allowlisted_frames_;
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_RULESET_MATCHER_BASE_H_
