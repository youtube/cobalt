/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_CONTENT_SECURITY_POLICY_STRUCT_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_CONTENT_SECURITY_POLICY_STRUCT_H_

#include "services/network/public/mojom/content_security_policy.mojom-shared.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"

namespace blink {

struct WebCSPSource {
  WebString scheme;
  WebString host;
  int port = -1;
  WebString path;
  bool is_host_wildcard;
  bool is_port_wildcard;
};

struct WebCSPHashSource {
  network::mojom::CSPHashAlgorithm algorithm;
  WebVector<uint8_t> value;
};

struct WebCSPSourceList {
  WebVector<WebCSPSource> sources;
  WebVector<WebString> nonces;
  WebVector<WebCSPHashSource> hashes;
  bool allow_self;
  bool allow_star;
  bool allow_response_redirects;
  bool allow_inline;
  bool allow_inline_speculation_rules;
  bool allow_eval;
  bool allow_wasm_eval;
  bool allow_wasm_unsafe_eval;
  bool allow_dynamic;
  bool allow_unsafe_hashes;
  bool report_sample;
};

struct WebContentSecurityPolicyDirective {
  network::mojom::CSPDirectiveName name;
  WebCSPSourceList source_list;
};

struct WebContentSecurityPolicyRawDirective {
  network::mojom::CSPDirectiveName name;
  WebString value;
};

struct WebCSPTrustedTypes {
  WebVector<WebString> list;
  bool allow_any;
  bool allow_duplicates;
};

struct WebContentSecurityPolicyHeader {
  WebString header_value;
  network::mojom::ContentSecurityPolicyType type =
      network::mojom::ContentSecurityPolicyType::kEnforce;
  network::mojom::ContentSecurityPolicySource source =
      network::mojom::ContentSecurityPolicySource::kHTTP;
};

struct WebContentSecurityPolicy {
  WebCSPSource self_origin;
  WebVector<WebContentSecurityPolicyRawDirective> raw_directives;
  WebVector<WebContentSecurityPolicyDirective> directives;
  bool upgrade_insecure_requests;
  bool treat_as_public_address;
  bool block_all_mixed_content;
  network::mojom::WebSandboxFlags sandbox =
      network::mojom::WebSandboxFlags::kNone;
  WebContentSecurityPolicyHeader header;
  bool use_reporting_api;
  WebVector<WebString> report_endpoints;
  network::mojom::CSPRequireTrustedTypesFor require_trusted_types_for;
  absl::optional<WebCSPTrustedTypes> trusted_types;
  WebVector<WebString> parsing_errors;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_WEB_CONTENT_SECURITY_POLICY_STRUCT_H_
