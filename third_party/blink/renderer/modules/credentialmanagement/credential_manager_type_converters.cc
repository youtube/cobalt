// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/credentialmanagement/credential_manager_type_converters.h"

#include <algorithm>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-blink.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_device_public_key_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_large_blob_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_payment_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_values.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authenticator_selection_criteria.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_cable_authentication_data.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_credential_logout_r_ps_request.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_credential_request_options_context.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_provider_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_identity_user_info.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_m_doc_element.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_m_doc_provider.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_parameters.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_request_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_rp_entity.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_user_entity.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_remote_desktop_client_override.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/modules/credentialmanagement/credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/federated_credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/password_credential.h"
#include "third_party/blink/renderer/modules/credentialmanagement/public_key_credential.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/boringssl/src/include/openssl/sha.h"

namespace mojo {

using blink::mojom::blink::AttestationConveyancePreference;
using blink::mojom::blink::AuthenticatorAttachment;
using blink::mojom::blink::AuthenticatorSelectionCriteria;
using blink::mojom::blink::AuthenticatorSelectionCriteriaPtr;
using blink::mojom::blink::AuthenticatorTransport;
using blink::mojom::blink::CableAuthentication;
using blink::mojom::blink::CableAuthenticationPtr;
using blink::mojom::blink::CredentialInfo;
using blink::mojom::blink::CredentialInfoPtr;
using blink::mojom::blink::CredentialType;
using blink::mojom::blink::DevicePublicKeyRequest;
using blink::mojom::blink::DevicePublicKeyRequestPtr;
using blink::mojom::blink::IdentityProvider;
using blink::mojom::blink::IdentityProviderConfig;
using blink::mojom::blink::IdentityProviderConfigPtr;
using blink::mojom::blink::IdentityProviderPtr;
using blink::mojom::blink::IdentityUserInfo;
using blink::mojom::blink::IdentityUserInfoPtr;
using blink::mojom::blink::LargeBlobSupport;
using blink::mojom::blink::LogoutRpsRequest;
using blink::mojom::blink::LogoutRpsRequestPtr;
using blink::mojom::blink::MDocElement;
using blink::mojom::blink::MDocElementPtr;
using blink::mojom::blink::MDocProvider;
using blink::mojom::blink::MDocProviderPtr;
using blink::mojom::blink::PRFValues;
using blink::mojom::blink::PRFValuesPtr;
using blink::mojom::blink::PublicKeyCredentialCreationOptionsPtr;
using blink::mojom::blink::PublicKeyCredentialDescriptor;
using blink::mojom::blink::PublicKeyCredentialDescriptorPtr;
using blink::mojom::blink::PublicKeyCredentialParameters;
using blink::mojom::blink::PublicKeyCredentialParametersPtr;
using blink::mojom::blink::PublicKeyCredentialRequestOptionsPtr;
using blink::mojom::blink::PublicKeyCredentialRpEntity;
using blink::mojom::blink::PublicKeyCredentialRpEntityPtr;
using blink::mojom::blink::PublicKeyCredentialType;
using blink::mojom::blink::PublicKeyCredentialUserEntity;
using blink::mojom::blink::PublicKeyCredentialUserEntityPtr;
using blink::mojom::blink::RemoteDesktopClientOverride;
using blink::mojom::blink::RemoteDesktopClientOverridePtr;
using blink::mojom::blink::ResidentKeyRequirement;
using blink::mojom::blink::RpContext;
using blink::mojom::blink::UserVerificationRequirement;

namespace {

static constexpr int kCoseEs256 = -7;
static constexpr int kCoseRs256 = -257;

PublicKeyCredentialParametersPtr CreatePublicKeyCredentialParameter(int alg) {
  auto mojo_parameter = PublicKeyCredentialParameters::New();
  mojo_parameter->type = PublicKeyCredentialType::PUBLIC_KEY;
  mojo_parameter->algorithm_identifier = alg;
  return mojo_parameter;
}

// HashPRFValue hashes a PRF evaluation point with a fixed prefix in order to
// separate the set of points that a website can evaluate. See
// https://w3c.github.io/webauthn/#prf-extension.
Vector<uint8_t> HashPRFValue(const blink::DOMArrayPiece value) {
  constexpr char kPrefix[] = "WebAuthn PRF";

  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  // This deliberately includes the terminating NUL.
  SHA256_Update(&ctx, kPrefix, sizeof(kPrefix));
  SHA256_Update(&ctx, value.Data(), value.ByteLength());

  uint8_t digest[SHA256_DIGEST_LENGTH];
  SHA256_Final(digest, &ctx);

  return Vector<uint8_t>(base::span(digest));
}

// SortPRFValuesByCredentialId is a "less than" function that puts the single,
// optional element without a credential ID at the beginning and otherwise
// lexicographically sorts by credential ID. The browser requires that PRF
// values be presented in this order so that it can easily establish that there
// are no duplicates.
bool SortPRFValuesByCredentialId(const PRFValuesPtr& a, const PRFValuesPtr& b) {
  if (!a->id.has_value()) {
    return true;
  } else if (!b->id.has_value()) {
    return false;
  } else {
    return std::lexicographical_compare(a->id->begin(), a->id->end(),
                                        b->id->begin(), b->id->end());
  }
}

}  // namespace

// static
CredentialInfoPtr TypeConverter<CredentialInfoPtr, blink::Credential*>::Convert(
    blink::Credential* credential) {
  auto info = CredentialInfo::New();
  info->id = credential->id();
  if (credential->IsPasswordCredential()) {
    ::blink::PasswordCredential* password_credential =
        static_cast<::blink::PasswordCredential*>(credential);
    info->type = CredentialType::PASSWORD;
    info->password = password_credential->password();
    info->name = password_credential->name();
    info->icon = password_credential->iconURL();
    info->federation = blink::SecurityOrigin::CreateUniqueOpaque();
  } else {
    DCHECK(credential->IsFederatedCredential());
    ::blink::FederatedCredential* federated_credential =
        static_cast<::blink::FederatedCredential*>(credential);
    info->type = CredentialType::FEDERATED;
    info->password = g_empty_string;
    info->federation = federated_credential->GetProviderAsOrigin();
    info->name = federated_credential->name();
    info->icon = federated_credential->iconURL();
  }
  return info;
}

// static
blink::Credential*
TypeConverter<blink::Credential*, CredentialInfoPtr>::Convert(
    const CredentialInfoPtr& info) {
  switch (info->type) {
    case CredentialType::FEDERATED:
      return blink::FederatedCredential::Create(info->id, info->federation,
                                                info->name, info->icon);
    case CredentialType::PASSWORD:
      return blink::PasswordCredential::Create(info->id, info->password,
                                               info->name, info->icon);
    case CredentialType::EMPTY:
      return nullptr;
  }
  NOTREACHED();
  return nullptr;
}

// static helper method.
Vector<uint8_t> ConvertFixedSizeArray(const blink::V8BufferSource* buffer,
                                      unsigned length) {
  if (blink::DOMArrayPiece(buffer).ByteLength() != length)
    return {};

  return ConvertTo<Vector<uint8_t>>(buffer);
}

// static
Vector<uint8_t>
TypeConverter<Vector<uint8_t>, blink::V8UnionArrayBufferOrArrayBufferView*>::
    Convert(const blink::V8UnionArrayBufferOrArrayBufferView* buffer) {
  DCHECK(buffer);
  Vector<uint8_t> vector;
  switch (buffer->GetContentType()) {
    case blink::V8UnionArrayBufferOrArrayBufferView::ContentType::kArrayBuffer:
      vector.Append(static_cast<uint8_t*>(buffer->GetAsArrayBuffer()->Data()),
                    base::checked_cast<wtf_size_t>(
                        buffer->GetAsArrayBuffer()->ByteLength()));
      break;
    case blink::V8UnionArrayBufferOrArrayBufferView::ContentType::
        kArrayBufferView:
      vector.Append(
          static_cast<uint8_t*>(buffer->GetAsArrayBufferView()->BaseAddress()),
          base::checked_cast<wtf_size_t>(
              buffer->GetAsArrayBufferView()->byteLength()));
      break;
  }
  return vector;
}

// static
PublicKeyCredentialType TypeConverter<PublicKeyCredentialType, String>::Convert(
    const String& type) {
  if (type == "public-key")
    return PublicKeyCredentialType::PUBLIC_KEY;
  NOTREACHED();
  return PublicKeyCredentialType::PUBLIC_KEY;
}

// static
absl::optional<AuthenticatorTransport>
TypeConverter<absl::optional<AuthenticatorTransport>, String>::Convert(
    const String& transport) {
  if (transport == "usb")
    return AuthenticatorTransport::USB;
  if (transport == "nfc")
    return AuthenticatorTransport::NFC;
  if (transport == "ble")
    return AuthenticatorTransport::BLE;
  // "cable" is the old name for "hybrid" and we accept either.
  if (transport == "cable" || transport == "hybrid")
    return AuthenticatorTransport::HYBRID;
  if (transport == "internal")
    return AuthenticatorTransport::INTERNAL;
  return absl::nullopt;
}

// static
String TypeConverter<String, AuthenticatorTransport>::Convert(
    const AuthenticatorTransport& transport) {
  if (transport == AuthenticatorTransport::USB)
    return "usb";
  if (transport == AuthenticatorTransport::NFC)
    return "nfc";
  if (transport == AuthenticatorTransport::BLE)
    return "ble";
  if (transport == AuthenticatorTransport::HYBRID)
    return "hybrid";
  if (transport == AuthenticatorTransport::INTERNAL)
    return "internal";
  NOTREACHED();
  return "usb";
}

// static
absl::optional<blink::mojom::blink::ResidentKeyRequirement>
TypeConverter<absl::optional<blink::mojom::blink::ResidentKeyRequirement>,
              String>::Convert(const String& requirement) {
  if (requirement == "discouraged")
    return ResidentKeyRequirement::DISCOURAGED;
  if (requirement == "preferred")
    return ResidentKeyRequirement::PREFERRED;
  if (requirement == "required")
    return ResidentKeyRequirement::REQUIRED;

  // AuthenticatorSelection.resident_key is defined as DOMString expressing a
  // ResidentKeyRequirement and unknown values must be treated as if the
  // property were unset.
  return absl::nullopt;
}

// static
absl::optional<UserVerificationRequirement>
TypeConverter<absl::optional<UserVerificationRequirement>, String>::Convert(
    const String& requirement) {
  if (requirement == "required")
    return UserVerificationRequirement::REQUIRED;
  if (requirement == "preferred")
    return UserVerificationRequirement::PREFERRED;
  if (requirement == "discouraged")
    return UserVerificationRequirement::DISCOURAGED;
  return absl::nullopt;
}

// static
absl::optional<AttestationConveyancePreference>
TypeConverter<absl::optional<AttestationConveyancePreference>, String>::Convert(
    const String& preference) {
  if (preference == "none")
    return AttestationConveyancePreference::NONE;
  if (preference == "indirect")
    return AttestationConveyancePreference::INDIRECT;
  if (preference == "direct")
    return AttestationConveyancePreference::DIRECT;
  if (preference == "enterprise")
    return AttestationConveyancePreference::ENTERPRISE;
  return absl::nullopt;
}

// static
absl::optional<AuthenticatorAttachment> TypeConverter<
    absl::optional<AuthenticatorAttachment>,
    absl::optional<String>>::Convert(const absl::optional<String>& attachment) {
  if (!attachment.has_value())
    return AuthenticatorAttachment::NO_PREFERENCE;
  if (attachment.value() == "platform")
    return AuthenticatorAttachment::PLATFORM;
  if (attachment.value() == "cross-platform")
    return AuthenticatorAttachment::CROSS_PLATFORM;
  return absl::nullopt;
}

// static
LargeBlobSupport
TypeConverter<LargeBlobSupport, absl::optional<String>>::Convert(
    const absl::optional<String>& large_blob_support) {
  if (large_blob_support) {
    if (*large_blob_support == "required")
      return LargeBlobSupport::REQUIRED;
    if (*large_blob_support == "preferred")
      return LargeBlobSupport::PREFERRED;
  }

  // Unknown values are treated as preferred.
  return LargeBlobSupport::PREFERRED;
}

// static
AuthenticatorSelectionCriteriaPtr
TypeConverter<AuthenticatorSelectionCriteriaPtr,
              blink::AuthenticatorSelectionCriteria>::
    Convert(const blink::AuthenticatorSelectionCriteria& criteria) {
  auto mojo_criteria =
      blink::mojom::blink::AuthenticatorSelectionCriteria::New();

  mojo_criteria->authenticator_attachment =
      AuthenticatorAttachment::NO_PREFERENCE;
  if (criteria.hasAuthenticatorAttachment()) {
    absl::optional<String> attachment = criteria.authenticatorAttachment();
    auto maybe_attachment =
        ConvertTo<absl::optional<AuthenticatorAttachment>>(attachment);
    if (maybe_attachment) {
      mojo_criteria->authenticator_attachment = *maybe_attachment;
    }
  }

  absl::optional<ResidentKeyRequirement> resident_key;
  if (criteria.hasResidentKey()) {
    resident_key = ConvertTo<absl::optional<ResidentKeyRequirement>>(
        criteria.residentKey());
  }
  if (resident_key) {
    mojo_criteria->resident_key = *resident_key;
  } else {
    mojo_criteria->resident_key = criteria.requireResidentKey()
                                      ? ResidentKeyRequirement::REQUIRED
                                      : ResidentKeyRequirement::DISCOURAGED;
  }

  mojo_criteria->user_verification = UserVerificationRequirement::PREFERRED;
  if (criteria.hasUserVerification()) {
    absl::optional<UserVerificationRequirement> user_verification =
        ConvertTo<absl::optional<UserVerificationRequirement>>(
            criteria.userVerification());
    if (user_verification) {
      mojo_criteria->user_verification = *user_verification;
    }
  }
  return mojo_criteria;
}

// static
LogoutRpsRequestPtr
TypeConverter<LogoutRpsRequestPtr, blink::IdentityCredentialLogoutRPsRequest>::
    Convert(const blink::IdentityCredentialLogoutRPsRequest& request) {
  auto mojo_request = LogoutRpsRequest::New();

  mojo_request->url = blink::KURL(request.url());
  mojo_request->account_id = request.accountId();
  return mojo_request;
}

// static
PublicKeyCredentialUserEntityPtr
TypeConverter<PublicKeyCredentialUserEntityPtr,
              blink::PublicKeyCredentialUserEntity>::
    Convert(const blink::PublicKeyCredentialUserEntity& user) {
  auto entity = PublicKeyCredentialUserEntity::New();
  // PublicKeyCredentialEntity
  entity->name = user.name();
  // PublicKeyCredentialUserEntity
  entity->id = ConvertTo<Vector<uint8_t>>(user.id());
  entity->display_name = user.displayName();
  return entity;
}

// static
PublicKeyCredentialRpEntityPtr
TypeConverter<PublicKeyCredentialRpEntityPtr,
              blink::PublicKeyCredentialRpEntity>::
    Convert(const blink::PublicKeyCredentialRpEntity& rp) {
  auto entity = PublicKeyCredentialRpEntity::New();
  // PublicKeyCredentialEntity
  if (!rp.name()) {
    return nullptr;
  }
  entity->name = rp.name();
  // PublicKeyCredentialRpEntity
  if (rp.hasId()) {
    entity->id = rp.id();
  }

  return entity;
}

// static
PublicKeyCredentialDescriptorPtr
TypeConverter<PublicKeyCredentialDescriptorPtr,
              blink::PublicKeyCredentialDescriptor>::
    Convert(const blink::PublicKeyCredentialDescriptor& descriptor) {
  auto mojo_descriptor = PublicKeyCredentialDescriptor::New();

  mojo_descriptor->type = ConvertTo<PublicKeyCredentialType>(
      blink::IDLEnumAsString(descriptor.type()));
  mojo_descriptor->id = ConvertTo<Vector<uint8_t>>(descriptor.id());
  if (descriptor.hasTransports() && !descriptor.transports().empty()) {
    for (const auto& transport : descriptor.transports()) {
      auto maybe_transport(
          ConvertTo<absl::optional<AuthenticatorTransport>>(transport));
      if (maybe_transport) {
        mojo_descriptor->transports.push_back(*maybe_transport);
      }
    }
  } else {
    mojo_descriptor->transports = {
        AuthenticatorTransport::USB, AuthenticatorTransport::BLE,
        AuthenticatorTransport::NFC, AuthenticatorTransport::HYBRID,
        AuthenticatorTransport::INTERNAL};
  }
  return mojo_descriptor;
}

// static
PublicKeyCredentialParametersPtr
TypeConverter<PublicKeyCredentialParametersPtr,
              blink::PublicKeyCredentialParameters>::
    Convert(const blink::PublicKeyCredentialParameters& parameter) {
  auto mojo_parameter = PublicKeyCredentialParameters::New();
  mojo_parameter->type = ConvertTo<PublicKeyCredentialType>(
      blink::IDLEnumAsString(parameter.type()));

  // A COSEAlgorithmIdentifier's value is a number identifying a cryptographic
  // algorithm. Values are registered in the IANA COSE Algorithms registry.
  // https://www.iana.org/assignments/cose/cose.xhtml#algorithms
  mojo_parameter->algorithm_identifier = parameter.alg();
  return mojo_parameter;
}

// static
PublicKeyCredentialCreationOptionsPtr
TypeConverter<PublicKeyCredentialCreationOptionsPtr,
              blink::PublicKeyCredentialCreationOptions>::
    Convert(const blink::PublicKeyCredentialCreationOptions& options) {
  auto mojo_options =
      blink::mojom::blink::PublicKeyCredentialCreationOptions::New();
  mojo_options->relying_party =
      PublicKeyCredentialRpEntity::From(*options.rp());
  mojo_options->user = PublicKeyCredentialUserEntity::From(*options.user());
  if (!mojo_options->relying_party || !mojo_options->user) {
    return nullptr;
  }
  mojo_options->challenge = ConvertTo<Vector<uint8_t>>(options.challenge());

  // Steps 7 and 8 of https://w3c.github.io/webauthn/#sctn-createCredential
  Vector<PublicKeyCredentialParametersPtr> parameters;
  if (options.pubKeyCredParams().size() == 0) {
    parameters.push_back(CreatePublicKeyCredentialParameter(kCoseEs256));
    parameters.push_back(CreatePublicKeyCredentialParameter(kCoseRs256));
  } else {
    for (auto& parameter : options.pubKeyCredParams()) {
      PublicKeyCredentialParametersPtr normalized_parameter =
          PublicKeyCredentialParameters::From(*parameter);
      if (normalized_parameter) {
        parameters.push_back(std::move(normalized_parameter));
      }
    }
    if (parameters.empty()) {
      return nullptr;
    }
  }
  mojo_options->public_key_parameters = std::move(parameters);

  if (options.hasTimeout()) {
    mojo_options->timeout = base::Milliseconds(options.timeout());
  }

  // Adds the excludeCredentials members
  for (auto& descriptor : options.excludeCredentials()) {
    PublicKeyCredentialDescriptorPtr mojo_descriptor =
        PublicKeyCredentialDescriptor::From(*descriptor);
    if (mojo_descriptor) {
      mojo_options->exclude_credentials.push_back(std::move(mojo_descriptor));
    }
  }

  if (options.hasAuthenticatorSelection()) {
    mojo_options->authenticator_selection =
        AuthenticatorSelectionCriteria::From(*options.authenticatorSelection());
  }

  mojo_options->attestation = AttestationConveyancePreference::NONE;
  if (options.hasAttestation()) {
    absl::optional<AttestationConveyancePreference> attestation =
        ConvertTo<absl::optional<AttestationConveyancePreference>>(
            options.attestation());
    if (attestation) {
      mojo_options->attestation = *attestation;
    }
  }

  mojo_options->protection_policy = blink::mojom::ProtectionPolicy::UNSPECIFIED;
  mojo_options->enforce_protection_policy = false;
  if (options.hasExtensions()) {
    auto* extensions = options.extensions();
    if (extensions->hasAppidExclude()) {
      mojo_options->appid_exclude = extensions->appidExclude();
    }
    if (extensions->hasHmacCreateSecret()) {
      mojo_options->hmac_create_secret = extensions->hmacCreateSecret();
    }
    if (extensions->hasCredentialProtectionPolicy()) {
      const auto& policy = extensions->credentialProtectionPolicy();
      if (policy == "userVerificationOptional") {
        mojo_options->protection_policy = blink::mojom::ProtectionPolicy::NONE;
      } else if (policy == "userVerificationOptionalWithCredentialIDList") {
        mojo_options->protection_policy =
            blink::mojom::ProtectionPolicy::UV_OR_CRED_ID_REQUIRED;
      } else if (policy == "userVerificationRequired") {
        mojo_options->protection_policy =
            blink::mojom::ProtectionPolicy::UV_REQUIRED;
      } else {
        return nullptr;
      }
    }
    if (extensions->hasEnforceCredentialProtectionPolicy() &&
        extensions->enforceCredentialProtectionPolicy()) {
      mojo_options->enforce_protection_policy = true;
    }
    if (extensions->credProps()) {
      mojo_options->cred_props = true;
    }
    if (extensions->hasLargeBlob()) {
      absl::optional<WTF::String> support;
      if (extensions->largeBlob()->hasSupport()) {
        support = extensions->largeBlob()->support();
      }
      mojo_options->large_blob_enable = ConvertTo<LargeBlobSupport>(support);
    }
    if (extensions->hasCredBlob()) {
      mojo_options->cred_blob =
          ConvertTo<Vector<uint8_t>>(extensions->credBlob());
    }
    if (extensions->hasPayment() && extensions->payment()->hasIsPayment() &&
        extensions->payment()->isPayment()) {
      mojo_options->is_payment_credential_creation = true;
    }
    if (extensions->hasMinPinLength() && extensions->minPinLength()) {
      mojo_options->min_pin_length_requested = true;
    }
    if (extensions->hasRemoteDesktopClientOverride()) {
      mojo_options->remote_desktop_client_override =
          RemoteDesktopClientOverride::From(
              *extensions->remoteDesktopClientOverride());
    }
    if (extensions->hasDevicePubKey()) {
      mojo_options->device_public_key =
          DevicePublicKeyRequest::From(*extensions->devicePubKey());
    }
    if (extensions->hasPrf()) {
      mojo_options->prf_enable = true;
    }
  }

  return mojo_options;
}

// static
CableAuthenticationPtr
TypeConverter<CableAuthenticationPtr, blink::CableAuthenticationData>::Convert(
    const blink::CableAuthenticationData& data) {
  auto entity = CableAuthentication::New();
  entity->version = data.version();
  switch (entity->version) {
    case 1:
      entity->client_eid = ConvertFixedSizeArray(data.clientEid(), 16);
      entity->authenticator_eid =
          ConvertFixedSizeArray(data.authenticatorEid(), 16);
      entity->session_pre_key = ConvertFixedSizeArray(data.sessionPreKey(), 32);
      if (entity->client_eid->empty() || entity->authenticator_eid->empty() ||
          entity->session_pre_key->empty()) {
        return nullptr;
      }
      break;

    case 2:
      entity->server_link_data =
          ConvertTo<Vector<uint8_t>>(data.sessionPreKey());
      if (entity->server_link_data->empty()) {
        return nullptr;
      }
      entity->experiments = ConvertTo<Vector<uint8_t>>(data.clientEid());
      break;

    default:
      return nullptr;
  }

  return entity;
}

// static
PublicKeyCredentialRequestOptionsPtr
TypeConverter<PublicKeyCredentialRequestOptionsPtr,
              blink::PublicKeyCredentialRequestOptions>::
    Convert(const blink::PublicKeyCredentialRequestOptions& options) {
  auto mojo_options =
      blink::mojom::blink::PublicKeyCredentialRequestOptions::New();
  mojo_options->challenge = ConvertTo<Vector<uint8_t>>(options.challenge());

  if (options.hasTimeout()) {
    mojo_options->timeout = base::Milliseconds(options.timeout());
  }

  if (options.hasRpId()) {
    mojo_options->relying_party_id = options.rpId();
  }

  // Adds the allowList members
  for (auto descriptor : options.allowCredentials()) {
    PublicKeyCredentialDescriptorPtr mojo_descriptor =
        PublicKeyCredentialDescriptor::From(*descriptor);
    if (mojo_descriptor) {
      mojo_options->allow_credentials.push_back(std::move(mojo_descriptor));
    }
  }

  mojo_options->user_verification = UserVerificationRequirement::PREFERRED;
  if (options.hasUserVerification()) {
    absl::optional<UserVerificationRequirement> user_verification =
        ConvertTo<absl::optional<UserVerificationRequirement>>(
            options.userVerification());
    if (user_verification) {
      mojo_options->user_verification = *user_verification;
    }
  }

  if (options.hasExtensions()) {
    auto* extensions = options.extensions();
    if (extensions->hasAppid()) {
      mojo_options->appid = extensions->appid();
    }
    if (extensions->hasCableAuthentication()) {
      Vector<CableAuthenticationPtr> mojo_data;
      for (auto& data : extensions->cableAuthentication()) {
        if (data->version() < 1 || data->version() > 2) {
          continue;
        }
        CableAuthenticationPtr mojo_cable = CableAuthentication::From(*data);
        if (mojo_cable) {
          mojo_data.push_back(std::move(mojo_cable));
        }
      }
      if (mojo_data.size() > 0) {
        mojo_options->cable_authentication_data = std::move(mojo_data);
      }
    }
#if BUILDFLAG(IS_ANDROID)
    if (extensions->hasUvm()) {
      mojo_options->user_verification_methods = extensions->uvm();
    }
#endif
    if (extensions->hasLargeBlob()) {
      if (extensions->largeBlob()->hasRead()) {
        mojo_options->large_blob_read = extensions->largeBlob()->read();
      }
      if (extensions->largeBlob()->hasWrite()) {
        mojo_options->large_blob_write =
            ConvertTo<Vector<uint8_t>>(extensions->largeBlob()->write());
      }
    }
    if (extensions->hasGetCredBlob() && extensions->getCredBlob()) {
      mojo_options->get_cred_blob = true;
    }
    if (extensions->hasRemoteDesktopClientOverride()) {
      mojo_options->remote_desktop_client_override =
          RemoteDesktopClientOverride::From(
              *extensions->remoteDesktopClientOverride());
    }
    if (extensions->hasDevicePubKey()) {
      mojo_options->device_public_key =
          DevicePublicKeyRequest::From(*extensions->devicePubKey());
    }
    if (extensions->hasPrf()) {
      mojo_options->prf = true;
      mojo_options->prf_inputs =
          ConvertTo<Vector<PRFValuesPtr>>(*extensions->prf());
    }
  }

  return mojo_options;
}

// static
RemoteDesktopClientOverridePtr
TypeConverter<RemoteDesktopClientOverridePtr,
              blink::RemoteDesktopClientOverride>::
    Convert(const blink::RemoteDesktopClientOverride& blink_value) {
  return RemoteDesktopClientOverride::New(
      blink::SecurityOrigin::CreateFromString(blink_value.origin()),
      blink_value.sameOriginWithAncestors());
}

// static
IdentityProviderConfigPtr
TypeConverter<IdentityProviderConfigPtr, blink::IdentityProviderConfig>::
    Convert(const blink::IdentityProviderConfig& provider) {
  auto mojo_provider = IdentityProviderConfig::New();

  mojo_provider->config_url = blink::KURL(provider.configURL());
  mojo_provider->client_id = provider.clientId();
  mojo_provider->nonce = provider.getNonceOr("");
  if (blink::RuntimeEnabledFeatures::FedCmLoginHintEnabled()) {
    mojo_provider->login_hint = provider.getLoginHintOr("");
  } else {
    mojo_provider->login_hint = "";
  }

  if (blink::RuntimeEnabledFeatures::FedCmAuthzEnabled()) {
    if (provider.hasScope()) {
      mojo_provider->scope = provider.scope();
    }
    if (provider.hasResponseType()) {
      mojo_provider->responseType = provider.responseType();
    }
    if (provider.hasParams()) {
      HashMap<String, String> params;
      for (const auto& pair : provider.params()) {
        params.Set(pair.first, pair.second);
      }
      mojo_provider->params = std::move(params);
    }
  }

  return mojo_provider;
}

// static
IdentityProviderPtr
TypeConverter<IdentityProviderPtr, blink::IdentityProviderConfig>::Convert(
    const blink::IdentityProviderConfig& provider) {
  if (provider.hasMdoc() &&
      blink::RuntimeEnabledFeatures::WebIdentityMDocsEnabled() &&
      // TODO(https://crbug.com/1416939): make sure the MDocs API
      // works well with the Multiple IdP API.
      !blink::RuntimeEnabledFeatures::FedCmMultipleIdentityProvidersEnabled()) {
    WTF::Vector<MDocElementPtr> requested_elements;
    for (auto element : provider.mdoc()->requestedElements()) {
      auto requested_element =
          MDocElement::New(element->elementNamespace(), element->name());
      requested_elements.push_back(std::move(requested_element));
    }
    auto mdoc = MDocProvider::New(provider.mdoc()->documentType(),
                                  provider.mdoc()->readerPublicKey(),
                                  std::move(requested_elements));
    return IdentityProvider::NewMdoc(std::move(mdoc));
  } else {
    auto config = IdentityProviderConfig::From(provider);
    return IdentityProvider::NewFederated(std::move(config));
  }
}

// static
RpContext
TypeConverter<RpContext, blink::V8IdentityCredentialRequestOptionsContext>::
    Convert(const blink::V8IdentityCredentialRequestOptionsContext& context) {
  switch (context.AsEnum()) {
    case blink::V8IdentityCredentialRequestOptionsContext::Enum::kSignin:
      return RpContext::kSignIn;
    case blink::V8IdentityCredentialRequestOptionsContext::Enum::kSignup:
      return RpContext::kSignUp;
    case blink::V8IdentityCredentialRequestOptionsContext::Enum::kUse:
      return RpContext::kUse;
    case blink::V8IdentityCredentialRequestOptionsContext::Enum::kContinue:
      return RpContext::kContinue;
  }
}

IdentityUserInfoPtr
TypeConverter<IdentityUserInfoPtr, blink::IdentityUserInfo>::Convert(
    const blink::IdentityUserInfo& user_info) {
  DCHECK(blink::RuntimeEnabledFeatures::FedCmUserInfoEnabled());
  auto mojo_user_info = IdentityUserInfo::New();

  mojo_user_info->email = user_info.email();
  mojo_user_info->given_name = user_info.givenName();
  mojo_user_info->name = user_info.name();
  mojo_user_info->picture = user_info.picture();
  return mojo_user_info;
}

// static
DevicePublicKeyRequestPtr
TypeConverter<DevicePublicKeyRequestPtr,
              blink::AuthenticationExtensionsDevicePublicKeyInputs>::
    Convert(const blink::AuthenticationExtensionsDevicePublicKeyInputs&
                device_public_key) {
  auto ret = DevicePublicKeyRequest::New();
  ret->attestation = ConvertTo<absl::optional<AttestationConveyancePreference>>(
                         device_public_key.attestation())
                         .value_or(AttestationConveyancePreference::NONE);
  ret->attestation_formats = device_public_key.attestationFormats();
  return ret;
}

// static
PRFValuesPtr
TypeConverter<PRFValuesPtr, blink::AuthenticationExtensionsPRFValues>::Convert(
    const blink::AuthenticationExtensionsPRFValues& values) {
  PRFValuesPtr ret = PRFValues::New();
  ret->first = HashPRFValue(values.first());
  if (values.hasSecond()) {
    ret->second = HashPRFValue(values.second());
  }
  return ret;
}

// static
Vector<PRFValuesPtr>
TypeConverter<Vector<PRFValuesPtr>, blink::AuthenticationExtensionsPRFInputs>::
    Convert(const blink::AuthenticationExtensionsPRFInputs& prf) {
  Vector<PRFValuesPtr> ret;
  if (prf.hasEval()) {
    ret.push_back(ConvertTo<PRFValuesPtr>(*prf.eval()));
  }
  if (prf.hasEvalByCredential()) {
    for (const auto& pair : prf.evalByCredential()) {
      Vector<char> cred_id;
      // The fact that this decodes successfully has already been tested.
      CHECK(WTF::Base64UnpaddedURLDecode(pair.first, cred_id));

      PRFValuesPtr values = ConvertTo<PRFValuesPtr>(*pair.second);
      values->id = Vector<uint8_t>(base::as_bytes(base::make_span(cred_id)));
      ret.emplace_back(std::move(values));
    }
  }

  std::sort(ret.begin(), ret.end(), SortPRFValuesByCredentialId);
  return ret;
}

}  // namespace mojo
