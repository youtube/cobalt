// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/ssl_config_type_converter.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace mojo {

namespace {
absl::optional<bool> OptionalBoolFromMojo(network::mojom::OptionalBool v) {
  switch (v) {
    case network::mojom::OptionalBool::kTrue:
      return absl::make_optional(true);
    case network::mojom::OptionalBool::kFalse:
      return absl::make_optional(false);
    case network::mojom::OptionalBool::kUnset:
      return absl::nullopt;
  }
  NOTREACHED_NORETURN();
}
}  // namespace

int MojoSSLVersionToNetSSLVersion(network::mojom::SSLVersion mojo_version) {
  switch (mojo_version) {
    case network::mojom::SSLVersion::kTLS12:
      return net::SSL_PROTOCOL_VERSION_TLS1_2;
    case network::mojom::SSLVersion::kTLS13:
      return net::SSL_PROTOCOL_VERSION_TLS1_3;
  }
  NOTREACHED();
  return net::SSL_PROTOCOL_VERSION_TLS1_3;
}

net::SSLContextConfig MojoSSLConfigToSSLContextConfig(
    const network::mojom::SSLConfigPtr& mojo_config) {
  net::SSLContextConfig net_config;

  net_config.version_min =
      MojoSSLVersionToNetSSLVersion(mojo_config->version_min);
  net_config.version_max =
      MojoSSLVersionToNetSSLVersion(mojo_config->version_max);
  DCHECK_LE(net_config.version_min, net_config.version_max);

  net_config.disabled_cipher_suites = mojo_config->disabled_cipher_suites;
  net_config.post_quantum_override =
      OptionalBoolFromMojo(mojo_config->post_quantum_override);
  net_config.ech_enabled = mojo_config->ech_enabled;
  net_config.insecure_hash_override =
      OptionalBoolFromMojo(mojo_config->insecure_hash_override);
  net_config.rsa_key_usage_for_local_anchors_override = OptionalBoolFromMojo(
      mojo_config->rsa_key_usage_for_local_anchors_override);
  return net_config;
}

net::CertVerifier::Config MojoSSLConfigToCertVerifierConfig(
    const network::mojom::SSLConfigPtr& mojo_config) {
  net::CertVerifier::Config net_config;
  net_config.enable_rev_checking = mojo_config->rev_checking_enabled;
  net_config.require_rev_checking_local_anchors =
      mojo_config->rev_checking_required_local_anchors;
  net_config.enable_sha1_local_anchors =
      mojo_config->sha1_local_anchors_enabled;
  net_config.disable_symantec_enforcement =
      mojo_config->symantec_enforcement_disabled;

  return net_config;
}

}  // namespace mojo
