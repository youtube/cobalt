// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/policy_container_host.h"

#include "base/lazy_instance.h"
#include "base/ranges/algorithm.h"
#include "content/browser/renderer_host/frame_navigation_entry.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/local_network_access_util.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"

namespace content {

namespace {

// KeepAliveHandle is simply a class referencing a PolicyContainerHost through a
// scoped_refptr, hence maintaining it alive.
class KeepAliveHandle
    : public scoped_refptr<PolicyContainerHost>,
      public blink::mojom::PolicyContainerHostKeepAliveHandle {
 public:
  explicit KeepAliveHandle(PolicyContainerHost* policy_container_host) {
    wrapped_pointer = policy_container_host;
  }

 private:
  scoped_refptr<PolicyContainerHost> wrapped_pointer;
};

using TokenPolicyContainerMap =
    std::unordered_map<blink::LocalFrameToken,
                       PolicyContainerHost*,
                       blink::LocalFrameToken::Hasher>;
base::LazyInstance<TokenPolicyContainerMap>::Leaky
    g_token_policy_container_map = LAZY_INSTANCE_INITIALIZER;

}  // namespace

bool operator==(const PolicyContainerPolicies& lhs,
                const PolicyContainerPolicies& rhs) {
  return lhs.referrer_policy == rhs.referrer_policy &&
         lhs.ip_address_space == rhs.ip_address_space &&
         lhs.is_web_secure_context == rhs.is_web_secure_context &&
         base::ranges::equal(lhs.content_security_policies,
                             rhs.content_security_policies) &&
         lhs.cross_origin_opener_policy == rhs.cross_origin_opener_policy &&
         lhs.cross_origin_embedder_policy == rhs.cross_origin_embedder_policy &&
         lhs.sandbox_flags == rhs.sandbox_flags &&
         lhs.is_credentialless == rhs.is_credentialless &&
         lhs.can_navigate_top_without_user_gesture ==
             rhs.can_navigate_top_without_user_gesture;
}

bool operator!=(const PolicyContainerPolicies& lhs,
                const PolicyContainerPolicies& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& out,
                         const PolicyContainerPolicies& policies) {
  out << "{ referrer_policy: " << policies.referrer_policy
      << ", ip_address_space: " << policies.ip_address_space
      << ", is_web_secure_context: " << policies.is_web_secure_context
      << ", content_security_policies: ";

  if (policies.content_security_policies.empty()) {
    out << "[]";
  } else {
    out << "[ ";
    auto it = policies.content_security_policies.begin();
    for (; it + 1 != policies.content_security_policies.end(); ++it) {
      out << (*it)->header->header_value << ", ";
    }
    out << (*it)->header->header_value << " ]";
  }

  out << ", cross_origin_opener_policy: "
      << "{ value: " << policies.cross_origin_opener_policy.value
      << ", reporting_endpoint: "
      << policies.cross_origin_opener_policy.reporting_endpoint.value_or(
             "<null>")
      << ", report_only_value: "
      << policies.cross_origin_opener_policy.report_only_value
      << ", report_only_reporting_endpoint: "
      << policies.cross_origin_opener_policy.report_only_reporting_endpoint
             .value_or("<null>")
      << ", soap_by_default_value: "
      << policies.cross_origin_opener_policy.soap_by_default_value << " }";

  out << ", cross_origin_embedder_policy: "
      << "{ value: " << policies.cross_origin_embedder_policy.value
      << ", reporting_endpoint: "
      << policies.cross_origin_embedder_policy.reporting_endpoint.value_or(
             "<null>")
      << ", report_only_value: "
      << policies.cross_origin_embedder_policy.report_only_value
      << ", report_only_reporting_endpoint: "
      << policies.cross_origin_embedder_policy.report_only_reporting_endpoint
             .value_or("<null>")
      << " }";

  out << ", sandbox_flags: " << policies.sandbox_flags;
  out << ", is_credentialless: " << policies.is_credentialless;
  out << ", can_navigate_top_without_user_gesture: "
      << policies.can_navigate_top_without_user_gesture;

  return out << " }";
}

PolicyContainerPolicies::PolicyContainerPolicies() = default;

PolicyContainerPolicies::PolicyContainerPolicies(
    network::mojom::ReferrerPolicy referrer_policy,
    network::mojom::IPAddressSpace ip_address_space,
    bool is_web_secure_context,
    std::vector<network::mojom::ContentSecurityPolicyPtr>
        content_security_policies,
    const network::CrossOriginOpenerPolicy& cross_origin_opener_policy,
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    network::mojom::WebSandboxFlags sandbox_flags,
    bool is_credentialless,
    bool can_navigate_top_without_user_gesture)
    : referrer_policy(referrer_policy),
      ip_address_space(ip_address_space),
      is_web_secure_context(is_web_secure_context),
      content_security_policies(std::move(content_security_policies)),
      cross_origin_opener_policy(cross_origin_opener_policy),
      cross_origin_embedder_policy(cross_origin_embedder_policy),
      sandbox_flags(sandbox_flags),
      is_credentialless(is_credentialless),
      can_navigate_top_without_user_gesture(
          can_navigate_top_without_user_gesture) {}

PolicyContainerPolicies::PolicyContainerPolicies(
    const blink::mojom::PolicyContainerPolicies& policies)
    : referrer_policy(policies.referrer_policy),
      ip_address_space(policies.ip_address_space),
      content_security_policies(
          mojo::Clone(policies.content_security_policies)),
      cross_origin_embedder_policy(policies.cross_origin_embedder_policy),
      sandbox_flags(policies.sandbox_flags),
      is_credentialless(policies.is_credentialless),
      can_navigate_top_without_user_gesture(
          policies.can_navigate_top_without_user_gesture) {}

PolicyContainerPolicies::PolicyContainerPolicies(
    const GURL& url,
    network::mojom::URLResponseHead* response_head,
    ContentBrowserClient* client)
    : PolicyContainerPolicies(
          network::mojom::ReferrerPolicy::kDefault,
          CalculateIPAddressSpace(url, response_head, client),
          network::IsUrlPotentiallyTrustworthy(url),
          mojo::Clone(response_head->parsed_headers->content_security_policy),
          response_head->parsed_headers->cross_origin_opener_policy,
          response_head->parsed_headers->cross_origin_embedder_policy,
          network::mojom::WebSandboxFlags::kNone,
          /*is_credentialless=*/false,
          /*can_navigate_top_without_user_gesture=*/true) {
  for (auto& content_security_policy :
       response_head->parsed_headers->content_security_policy) {
    sandbox_flags |= content_security_policy->sandbox;
  }
}

PolicyContainerPolicies::PolicyContainerPolicies(PolicyContainerPolicies&&) =
    default;

PolicyContainerPolicies& PolicyContainerPolicies::operator=(
    PolicyContainerPolicies&&) = default;

PolicyContainerPolicies::~PolicyContainerPolicies() = default;

PolicyContainerPolicies PolicyContainerPolicies::Clone() const {
  return PolicyContainerPolicies(
      referrer_policy, ip_address_space, is_web_secure_context,
      mojo::Clone(content_security_policies), cross_origin_opener_policy,
      cross_origin_embedder_policy, sandbox_flags, is_credentialless,
      can_navigate_top_without_user_gesture);
}

std::unique_ptr<PolicyContainerPolicies> PolicyContainerPolicies::ClonePtr()
    const {
  return std::make_unique<PolicyContainerPolicies>(Clone());
}

void PolicyContainerPolicies::AddContentSecurityPolicies(
    std::vector<network::mojom::ContentSecurityPolicyPtr> policies) {
  content_security_policies.insert(content_security_policies.end(),
                                   std::make_move_iterator(policies.begin()),
                                   std::make_move_iterator(policies.end()));
}

blink::mojom::PolicyContainerPoliciesPtr
PolicyContainerPolicies::ToMojoPolicyContainerPolicies() const {
  return blink::mojom::PolicyContainerPolicies::New(
      cross_origin_embedder_policy, referrer_policy,
      mojo::Clone(content_security_policies), is_credentialless, sandbox_flags,
      ip_address_space, can_navigate_top_without_user_gesture);
}

PolicyContainerHost::PolicyContainerHost() = default;

PolicyContainerHost::PolicyContainerHost(PolicyContainerPolicies policies)
    : policies_(std::move(policies)) {}

PolicyContainerHost::~PolicyContainerHost() {
  // The PolicyContainerHost associated with |frame_token_| might have
  // changed. In that case, we must not remove the map entry.
  if (frame_token_ && FromFrameToken(frame_token_.value()) == this)
    g_token_policy_container_map.Get().erase(frame_token_.value());
}

void PolicyContainerHost::AssociateWithFrameToken(
    const blink::LocalFrameToken& frame_token,
    int process_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!frame_token_);
  frame_token_ = frame_token;
  process_id_ = process_id;
  g_token_policy_container_map.Get().erase(frame_token);
  g_token_policy_container_map.Get().emplace(frame_token, this);
}

PolicyContainerHost* PolicyContainerHost::FromFrameToken(
    const blink::LocalFrameToken& frame_token) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto it = g_token_policy_container_map.Get().find(frame_token);
  if (it == g_token_policy_container_map.Get().end())
    return nullptr;
  return it->second;
}

void PolicyContainerHost::SetReferrerPolicy(
    network::mojom::ReferrerPolicy referrer_policy) {
  policies_.referrer_policy = referrer_policy;
  if (frame_token_) {
    if (RenderFrameHostImpl* rfh = RenderFrameHostImpl::FromFrameToken(
            process_id_, frame_token_.value())) {
      rfh->DidChangeReferrerPolicy(referrer_policy);
    }
  }
}

void PolicyContainerHost::AddContentSecurityPolicies(
    std::vector<network::mojom::ContentSecurityPolicyPtr>
        content_security_policies) {
  policies_.AddContentSecurityPolicies(std::move(content_security_policies));
}

blink::mojom::PolicyContainerPtr
PolicyContainerHost::CreatePolicyContainerForBlink() {
  // This function might be called several times, for example if we need to
  // recreate the RenderFrame after the renderer process died. We gracefully
  // handle this by resetting the receiver and creating a new one. It would be
  // good to find a way to check that the previous remote has been deleted or is
  // not needed anymore. Unfortunately, this cannot be done with a disconnect
  // handler, since the mojo disconnect notification is not guaranteed to be
  // received before we try to create a new remote.
  policy_container_host_receiver_.reset();
  mojo::PendingAssociatedRemote<blink::mojom::PolicyContainerHost> remote;
  Bind(blink::mojom::PolicyContainerBindParams::New(
      remote.InitWithNewEndpointAndPassReceiver()));

  return blink::mojom::PolicyContainer::New(
      policies_.ToMojoPolicyContainerPolicies(), std::move(remote));
}

scoped_refptr<PolicyContainerHost> PolicyContainerHost::Clone() const {
  return base::MakeRefCounted<PolicyContainerHost>(policies_.Clone());
}

void PolicyContainerHost::Bind(
    blink::mojom::PolicyContainerBindParamsPtr bind_params) {
  policy_container_host_receiver_.Bind(std::move(bind_params->receiver));

  // Keep the PolicyContainerHost alive, as long as its PolicyContainer (owning
  // the mojo remote) in the renderer process alive.
  scoped_refptr<PolicyContainerHost> copy = this;
  policy_container_host_receiver_.set_disconnect_handler(base::BindOnce(
      [](scoped_refptr<PolicyContainerHost>) {}, std::move(copy)));
}

void PolicyContainerHost::IssueKeepAliveHandle(
    mojo::PendingReceiver<blink::mojom::PolicyContainerHostKeepAliveHandle>
        receiver) {
  keep_alive_handles_receiver_set_.Add(std::make_unique<KeepAliveHandle>(this),
                                       std::move(receiver));
}

}  // namespace content
