// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/csp/conversion_util.h"

#include "services/network/public/mojom/content_security_policy.mojom-blink.h"

namespace blink {

namespace {

<<<<<<< HEAD
network::mojom::blink::CSPSourcePtr ConvertSource(const WebCSPSource& source) {
=======
// TODO(arthursonzogni): Remove this when BeginNavigation will be sent directly
// from blink.
WebCSPSource ConvertToPublic(network::mojom::blink::CSPSourcePtr source) {
  return {source->scheme,
          source->host,
          source->port,
          source->path,
          source->is_host_wildcard,
          source->is_port_wildcard};
}

// TODO(arthursonzogni): Remove this when BeginNavigation will be sent directly
// from blink.
WebCSPHashSource ConvertToPublic(
    network::mojom::blink::CSPHashSourcePtr hash_source) {
  return {hash_source->algorithm, std::move(hash_source->value)};
}

// TODO(arthursonzogni): Remove this when BeginNavigation will be sent directly
// from blink.
WebCSPSourceList ConvertToPublic(
    network::mojom::blink::CSPSourceListPtr source_list) {
  WebVector<WebCSPSource> sources(source_list->sources.size());
  for (wtf_size_t i = 0; i < source_list->sources.size(); ++i)
    sources[i] = ConvertToPublic(std::move(source_list->sources[i]));
  WebVector<WebCSPHashSource> hashes(source_list->hashes.size());
  for (wtf_size_t i = 0; i < source_list->hashes.size(); ++i)
    hashes[i] = ConvertToPublic(std::move(source_list->hashes[i]));
  return {std::move(sources),
          std::move(source_list->nonces),
          std::move(hashes),
          source_list->allow_self,
          source_list->allow_star,
          source_list->allow_response_redirects,
          source_list->allow_inline,
          source_list->allow_inline_speculation_rules,
          source_list->allow_eval,
          source_list->allow_wasm_eval,
          source_list->allow_wasm_unsafe_eval,
          source_list->allow_dynamic,
          source_list->allow_unsafe_hashes,
#if BUILDFLAG(IS_COBALT)
          source_list->report_sample,
          source_list->cobalt_insecure_local_network};
#else
          source_list->report_sample};
#endif
}

// TODO(arthursonzogni): Remove this when BeginNavigation will be sent directly
// from blink.
absl::optional<WebCSPTrustedTypes> ConvertToPublic(
    network::mojom::blink::CSPTrustedTypesPtr trusted_types) {
  if (!trusted_types)
    return absl::nullopt;
  return WebCSPTrustedTypes{std::move(trusted_types->list),
                            trusted_types->allow_any,
                            trusted_types->allow_duplicates};
}

// TODO(arthursonzogni): Remove this when BeginNavigation will be sent directly
// from blink.
WebContentSecurityPolicyHeader ConvertToPublic(
    network::mojom::blink::ContentSecurityPolicyHeaderPtr header) {
  return {header->header_value, header->type, header->source};
}

Vector<String> ConvertToWTF(const WebVector<blink::WebString>& list_in) {
  Vector<String> list_out;
  for (const auto& element : list_in)
    list_out.emplace_back(element);
  return list_out;
}

network::mojom::blink::CSPSourcePtr ConvertToMojoBlink(
    const WebCSPSource& source) {
>>>>>>> a40bc8ae05d (Add custom cobalt-insecure-local-network csp source (#4958))
  return network::mojom::blink::CSPSource::New(
      source.scheme, source.host, source.port, source.path,
      source.is_host_wildcard, source.is_port_wildcard);
}

network::mojom::blink::CSPHashSourcePtr ConvertHashSource(
    const WebCSPHashSource& hash_source) {
  return network::mojom::blink::CSPHashSource::New(
      hash_source.algorithm, Vector<uint8_t>(hash_source.value));
}

network::mojom::blink::CSPSourceListPtr ConvertSourceList(
    const WebCSPSourceList& source_list) {
  return network::mojom::blink::CSPSourceList::New(
      WTF::ToVector(source_list.sources, ConvertSource),
      Vector<String>(source_list.nonces),
      WTF::ToVector(source_list.hashes, ConvertHashSource),
      source_list.allow_self, source_list.allow_star, source_list.allow_inline,
      source_list.allow_inline_speculation_rules, source_list.allow_eval,
      source_list.allow_wasm_eval, source_list.allow_wasm_unsafe_eval,
      source_list.allow_dynamic, source_list.allow_unsafe_hashes,
<<<<<<< HEAD
      source_list.report_sample, source_list.report_hash_algorithm);
=======
#if BUILDFLAG(IS_COBALT)
      source_list.report_sample, source_list.cobalt_insecure_local_network);
#else
      source_list.report_sample);
#endif
>>>>>>> a40bc8ae05d (Add custom cobalt-insecure-local-network csp source (#4958))
}

}  // namespace

network::mojom::blink::ContentSecurityPolicyPtr ConvertToMojoBlink(
    const WebContentSecurityPolicy& policy_in) {
  HashMap<network::mojom::CSPDirectiveName, String> raw_directives;
  for (const auto& directive : policy_in.raw_directives) {
    raw_directives.insert(directive.name, directive.value);
  }

  HashMap<network::mojom::CSPDirectiveName,
          network::mojom::blink::CSPSourceListPtr>
      directives;
  for (const auto& directive : policy_in.directives) {
    directives.insert(directive.name, ConvertSourceList(directive.source_list));
  }

  return network::mojom::blink::ContentSecurityPolicy::New(
      ConvertSource(policy_in.self_origin), std::move(raw_directives),
      std::move(directives), policy_in.upgrade_insecure_requests,
      policy_in.treat_as_public_address, policy_in.block_all_mixed_content,
      policy_in.sandbox,
      network::mojom::blink::ContentSecurityPolicyHeader::New(
          policy_in.header.header_value, policy_in.header.type,
          policy_in.header.source),
      policy_in.use_reporting_api, Vector<String>(policy_in.report_endpoints),
      policy_in.require_sri_for, policy_in.require_trusted_types_for,
      policy_in.trusted_types
          ? network::mojom::blink::CSPTrustedTypes::New(
                Vector<String>(policy_in.trusted_types->list),
                policy_in.trusted_types->allow_any,
                policy_in.trusted_types->allow_duplicates)
          : nullptr,
      Vector<String>(policy_in.parsing_errors));
}

Vector<network::mojom::blink::ContentSecurityPolicyPtr> ConvertToMojoBlink(
    const std::vector<WebContentSecurityPolicy>& list_in) {
  return Vector<network::mojom::blink::ContentSecurityPolicyPtr>(
      list_in, [](const auto& policy) { return ConvertToMojoBlink(policy); });
}

}  // namespace blink
