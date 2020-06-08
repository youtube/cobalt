// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_verify_proc_builtin.h"

#include <memory>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/sha1.h"
#include "base/strings/string_piece.h"
#include "crypto/sha2.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verifier.h"
#include "net/cert/cert_verify_proc.h"
#include "net/cert/cert_verify_result.h"
#include "net/cert/ev_root_ca_metadata.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/cert_issuer_source_aia.h"
#include "net/cert/internal/cert_issuer_source_static.h"
#include "net/cert/internal/common_cert_errors.h"
#include "net/cert/internal/parsed_certificate.h"
#include "net/cert/internal/path_builder.h"
#include "net/cert/internal/revocation_checker.h"
#include "net/cert/internal/simple_path_builder_delegate.h"
#include "net/cert/internal/system_trust_store.h"
#include "net/cert/known_roots.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/der/encode_values.h"

namespace net {

namespace {

DEFINE_CERT_ERROR_ID(kPathLacksEVPolicy, "Path does not have an EV policy");

RevocationPolicy NoRevocationChecking() {
  RevocationPolicy policy;
  policy.check_revocation = false;
  policy.networking_allowed = false;
  policy.allow_missing_info = true;
  policy.allow_network_failure = true;
  return policy;
}

// Gets the set of policy OIDs in |cert| that are recognized as EV OIDs for some
// root.
void GetEVPolicyOids(const EVRootCAMetadata* ev_metadata,
                     const ParsedCertificate* cert,
                     std::set<der::Input>* oids) {
  oids->clear();

  if (!cert->has_policy_oids())
    return;

  for (const der::Input& oid : cert->policy_oids()) {
    if (ev_metadata->IsEVPolicyOIDGivenBytes(oid))
      oids->insert(oid);
  }
}

// Returns true if |cert| could be an EV certificate, based on its policies
// extension. A return of false means it definitely is not an EV certificate,
// whereas a return of true means it could be EV.
bool IsEVCandidate(const EVRootCAMetadata* ev_metadata,
                   const ParsedCertificate* cert) {
  std::set<der::Input> oids;
  GetEVPolicyOids(ev_metadata, cert, &oids);
  return !oids.empty();
}

// Enum for whether path building is attempting to verify a certificate as EV or
// as DV.
enum class VerificationType {
  kEV,  // Extended Validation
  kDV,  // Domain Validation
};

// TODO(eroman): The path building code in this file enforces its idea of weak
// keys, and signature algorithms, but separately cert_verify_proc.cc also
// checks the chains with its own policy. These policies must be aligned to
// give path building the best chance of finding a good path.
class PathBuilderDelegateImpl : public SimplePathBuilderDelegate {
 public:
  // Uses the default policy from SimplePathBuilderDelegate, which requires RSA
  // keys to be at least 1024-bits large, and optionally accepts SHA1
  // certificates.
  PathBuilderDelegateImpl(const CRLSet* crl_set,
                          CertNetFetcher* net_fetcher,
                          VerificationType verification_type,
                          SimplePathBuilderDelegate::DigestPolicy digest_policy,
                          int flags,
                          const SystemTrustStore* ssl_trust_store,
                          base::StringPiece stapled_leaf_ocsp_response,
                          const EVRootCAMetadata* ev_metadata,
                          bool* checked_revocation_for_some_path)
      : SimplePathBuilderDelegate(1024, digest_policy),
        crl_set_(crl_set),
        net_fetcher_(net_fetcher),
        verification_type_(verification_type),
        flags_(flags),
        ssl_trust_store_(ssl_trust_store),
        stapled_leaf_ocsp_response_(stapled_leaf_ocsp_response),
        ev_metadata_(ev_metadata),
        checked_revocation_for_some_path_(checked_revocation_for_some_path) {}

  // This is called for each built chain, including ones which failed. It is
  // responsible for adding errors to the built chain if it is not acceptable.
  void CheckPathAfterVerification(CertPathBuilderResultPath* path) override {
    // If the path is already invalid, don't check revocation status. The chain
    // is expected to be valid when doing revocation checks (since for instance
    // the correct issuer for a certificate may need to be known). Also if
    // certificates are already expired, obtaining their revocation status may
    // fail.
    //
    // TODO(eroman): When CertVerifyProcBuiltin fails to find a valid path,
    //               whatever (partial/incomplete) path it does return should
    //               minimally be checked with the CRLSet.
    if (!path->IsValid())
      return;

    // If EV was requested the certificate must chain to a recognized EV root
    // and have one of its recognized EV policy OIDs.
    if (verification_type_ == VerificationType::kEV) {
      if (!ConformsToEVPolicy(path)) {
        path->errors.GetErrorsForCert(0)->AddError(kPathLacksEVPolicy);
        return;
      }
    }

    // Select an appropriate revocation policy for this chain based on the
    // verifier flags and root, and whether this is an EV or DV path building
    // attempt.
    bool crlset_leaf_coverage_sufficient;
    RevocationPolicy policy =
        ChooseRevocationPolicy(path->certs, &crlset_leaf_coverage_sufficient);

    // Check for revocations using the CRLSet (if available).
    if (crl_set_) {
      switch (CheckChainRevocationUsingCRLSet(crl_set_, path->certs,
                                              &path->errors)) {
        case CRLSet::Result::REVOKED:
          return;
        case CRLSet::Result::GOOD:
          if (crlset_leaf_coverage_sufficient) {
            // Weaken the revocation checking requirement as it has been
            // satisfied. (Don't early-return, since still want to consult
            // cached OCSP/CRL if available).
            policy = NoRevocationChecking();
          }
          break;
        case CRLSet::Result::UNKNOWN:
          // CRLSet was inconclusive.
          break;
      }
    }

    if (policy.check_revocation)
      *checked_revocation_for_some_path_ = true;

    // Check the revocation status for each certificate in the chain according
    // to |policy|. Depending on the policy, errors will be added to the
    // respective certificates, so |errors->ContainsHighSeverityErrors()| will
    // reflect the revocation status of the chain after this call.
    CheckCertChainRevocation(path->certs, path->last_cert_trust, policy,
                             stapled_leaf_ocsp_response_, net_fetcher_,
                             &path->errors);
  }

 private:
  // Selects a revocation policy based on the CertVerifier flags and the given
  // certificate chain.
  RevocationPolicy ChooseRevocationPolicy(
      const ParsedCertificateList& certs,
      bool* crlset_leaf_coverage_sufficient) {
    // The only case this is set to true is for EV.
    *crlset_leaf_coverage_sufficient = false;

    // Use hard-fail revocation checking for local trust anchors, if requested
    // by the load flag and the chain uses a non-public root.
    if ((flags_ & CertVerifyProc::VERIFY_REV_CHECKING_REQUIRED_LOCAL_ANCHORS) &&
        !certs.empty() && !ssl_trust_store_->IsKnownRoot(certs.back().get())) {
      RevocationPolicy policy;
      policy.check_revocation = true;
      policy.networking_allowed = true;
      policy.allow_missing_info = true;
      policy.allow_network_failure = false;

      // In practice EV verification won't succeed for local anchors. For
      // completeness though use all the strictness of EV if this is an EV
      // attempt.
      if (verification_type_ == VerificationType::kEV)
        policy.allow_missing_info = false;

      return policy;
    }

    // Use hard-fail revocation checking for EV certificates.
    if (verification_type_ == VerificationType::kEV) {
      // For EV verification leaf coverage is considered sufficient.
      *crlset_leaf_coverage_sufficient = true;

      RevocationPolicy policy;
      policy.check_revocation = true;
      policy.networking_allowed = true;
      policy.allow_missing_info = false;
      policy.allow_network_failure = false;
      return policy;
    }

    // Use soft-fail revocation checking for VERIFY_REV_CHECKING_ENABLED.
    if (flags_ & CertVerifyProc::VERIFY_REV_CHECKING_ENABLED) {
      RevocationPolicy policy;
      policy.check_revocation = true;
      policy.networking_allowed = true;
      policy.allow_missing_info = true;
      policy.allow_network_failure = true;
      return policy;
    }

    return NoRevocationChecking();
  }

  // Returns true if |path| chains to an EV root, and the chain conforms to one
  // of its EV policy OIDs. When building paths all candidate EV policy OIDs
  // were requested, so it is just a matter of testing each of the policies the
  // chain conforms to.
  bool ConformsToEVPolicy(const CertPathBuilderResultPath* path) {
    const ParsedCertificate* root = path->GetTrustedCert();
    if (!root)
      return false;

    SHA256HashValue root_fingerprint;
    crypto::SHA256HashString(root->der_cert().AsStringPiece(),
                             root_fingerprint.data,
                             sizeof(root_fingerprint.data));

    for (const der::Input& oid : path->user_constrained_policy_set) {
      if (ev_metadata_->HasEVPolicyOIDGivenBytes(root_fingerprint, oid))
        return true;
    }

    return false;
  }

  // The CRLSet may be null.
  const CRLSet* crl_set_;
  CertNetFetcher* net_fetcher_;
  const VerificationType verification_type_;
  const int flags_;
  const SystemTrustStore* ssl_trust_store_;
  const base::StringPiece stapled_leaf_ocsp_response_;
  const EVRootCAMetadata* ev_metadata_;
  bool* checked_revocation_for_some_path_;
};

class CertVerifyProcBuiltin : public CertVerifyProc {
 public:
  CertVerifyProcBuiltin();

  bool SupportsAdditionalTrustAnchors() const override;

 protected:
  ~CertVerifyProcBuiltin() override;

 private:
  int VerifyInternal(X509Certificate* cert,
                     const std::string& hostname,
                     const std::string& ocsp_response,
                     int flags,
                     CRLSet* crl_set,
                     const CertificateList& additional_trust_anchors,
                     CertVerifyResult* verify_result) override;
};

CertVerifyProcBuiltin::CertVerifyProcBuiltin() = default;

CertVerifyProcBuiltin::~CertVerifyProcBuiltin() = default;

bool CertVerifyProcBuiltin::SupportsAdditionalTrustAnchors() const {
  return true;
}

scoped_refptr<ParsedCertificate> ParseCertificateFromBuffer(
    CRYPTO_BUFFER* cert_handle,
    CertErrors* errors) {
  return ParsedCertificate::Create(bssl::UpRef(cert_handle),
                                   x509_util::DefaultParseCertificateOptions(),
                                   errors);
}

void AddIntermediatesToIssuerSource(X509Certificate* x509_cert,
                                    CertIssuerSourceStatic* intermediates) {
  CertErrors errors;
  for (const auto& intermediate : x509_cert->intermediate_buffers()) {
    scoped_refptr<ParsedCertificate> cert =
        ParseCertificateFromBuffer(intermediate.get(), &errors);
    if (cert)
      intermediates->AddCert(std::move(cert));
    // TODO(crbug.com/634443): Surface these parsing errors?
  }
}

// Appends the SHA256 hashes of |spki_bytes| to |*hashes|.
// TODO(eroman): Hashes are also calculated at other times (such as when
//               checking CRLSet). Consider caching to avoid recalculating (say
//               in the delegate's PathInfo).
void AppendPublicKeyHashes(const der::Input& spki_bytes,
                           HashValueVector* hashes) {
  HashValue sha256(HASH_VALUE_SHA256);
  crypto::SHA256HashString(spki_bytes.AsStringPiece(), sha256.data(),
                           crypto::kSHA256Length);
  hashes->push_back(sha256);
}

// Appends the SubjectPublicKeyInfo hashes for all certificates in
// |path| to |*hashes|.
void AppendPublicKeyHashes(const CertPathBuilderResultPath& path,
                           HashValueVector* hashes) {
  for (const scoped_refptr<ParsedCertificate>& cert : path.certs)
    AppendPublicKeyHashes(cert->tbs().spki_tlv, hashes);
}

// Sets the bits on |cert_status| for all the errors present in |errors| (the
// errors for a particular path).
void MapPathBuilderErrorsToCertStatus(const CertPathErrors& errors,
                                      CertStatus* cert_status) {
  // If there were no errors, nothing to do.
  if (!errors.ContainsHighSeverityErrors())
    return;

  if (errors.ContainsError(cert_errors::kCertificateRevoked))
    *cert_status |= CERT_STATUS_REVOKED;

  if (errors.ContainsError(cert_errors::kNoRevocationMechanism))
    *cert_status |= CERT_STATUS_NO_REVOCATION_MECHANISM;

  if (errors.ContainsError(cert_errors::kUnableToCheckRevocation))
    *cert_status |= CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;

  if (errors.ContainsError(cert_errors::kUnacceptablePublicKey))
    *cert_status |= CERT_STATUS_WEAK_KEY;

  if (errors.ContainsError(cert_errors::kValidityFailedNotAfter) ||
      errors.ContainsError(cert_errors::kValidityFailedNotBefore)) {
    *cert_status |= CERT_STATUS_DATE_INVALID;
  }

  if (errors.ContainsError(cert_errors::kDistrustedByTrustStore))
    *cert_status |= CERT_STATUS_AUTHORITY_INVALID;

  // IMPORTANT: If the path was invalid for a reason that was not
  // explicity checked above, set a general error. This is important as
  // |cert_status| is what ultimately indicates whether verification was
  // successful or not (absense of errors implies success).
  if (!IsCertStatusError(*cert_status))
    *cert_status |= CERT_STATUS_INVALID;
}

bssl::UniquePtr<CRYPTO_BUFFER> CreateCertBuffers(
    const scoped_refptr<ParsedCertificate>& certificate) {
  return X509Certificate::CreateCertBufferFromBytes(
      reinterpret_cast<const char*>(certificate->der_cert().UnsafeData()),
      certificate->der_cert().Length());
}

// Creates a X509Certificate (chain) to return as the verified result.
//
//  * |target_cert|: The original X509Certificate that was passed in to
//                   VerifyInternal()
//  * |path|: The result (possibly failed) from path building.
scoped_refptr<X509Certificate> CreateVerifiedCertChain(
    X509Certificate* target_cert,
    const CertPathBuilderResultPath& path) {
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;

  // Skip the first certificate in the path as that is the target certificate
  for (size_t i = 1; i < path.certs.size(); ++i)
    intermediates.push_back(CreateCertBuffers(path.certs[i]));

  scoped_refptr<X509Certificate> result = X509Certificate::CreateFromBuffer(
      bssl::UpRef(target_cert->cert_buffer()), std::move(intermediates));
  // |target_cert| was already successfully parsed, so this should never fail.
  DCHECK(result);

  return result;
}

// Describes the parameters for a single path building attempt. Path building
// may be re-tried with different parameters for EV and for accepting SHA1
// certificates.
struct BuildPathAttempt {
  BuildPathAttempt(VerificationType verification_type,
                   SimplePathBuilderDelegate::DigestPolicy digest_policy)
      : verification_type(verification_type), digest_policy(digest_policy) {}

  explicit BuildPathAttempt(VerificationType verification_type)
      : BuildPathAttempt(verification_type,
                         SimplePathBuilderDelegate::DigestPolicy::kStrong) {}

  VerificationType verification_type;
  SimplePathBuilderDelegate::DigestPolicy digest_policy;
};

void TryBuildPath(const scoped_refptr<ParsedCertificate>& target,
                  CertIssuerSourceStatic* intermediates,
                  SystemTrustStore* ssl_trust_store,
                  base::Time verification_time,
                  VerificationType verification_type,
                  SimplePathBuilderDelegate::DigestPolicy digest_policy,
                  int flags,
                  const std::string& ocsp_response,
                  const CRLSet* crl_set,
                  CertNetFetcher* net_fetcher,
                  const EVRootCAMetadata* ev_metadata,
                  CertPathBuilder::Result* result,
                  bool* checked_revocation) {
  der::GeneralizedTime der_verification_time;
  if (!der::EncodeTimeAsGeneralizedTime(verification_time,
                                        &der_verification_time)) {
    // This shouldn't be possible.
    result->Clear();
    return;
  }

  // Path building will require candidate paths to conform to at least one of
  // the policies in |user_initial_policy_set|.
  std::set<der::Input> user_initial_policy_set;

  if (verification_type == VerificationType::kEV) {
    GetEVPolicyOids(ev_metadata, target.get(), &user_initial_policy_set);
  } else {
    user_initial_policy_set = {AnyPolicy()};
  }

  PathBuilderDelegateImpl path_builder_delegate(
      crl_set, net_fetcher, verification_type, digest_policy, flags,
      ssl_trust_store, ocsp_response, ev_metadata, checked_revocation);

  // Initialize the path builder.
  CertPathBuilder path_builder(
      target, ssl_trust_store->GetTrustStore(), &path_builder_delegate,
      der_verification_time, KeyPurpose::SERVER_AUTH,
      InitialExplicitPolicy::kFalse, user_initial_policy_set,
      InitialPolicyMappingInhibit::kFalse, InitialAnyPolicyInhibit::kFalse,
      result);

  // Allow the path builder to discover the explicitly provided intermediates in
  // |input_cert|.
  path_builder.AddCertIssuerSource(intermediates);

// Cobalt does not need AIA fetching.
#if !defined(STARBOARD)
  // Allow the path builder to discover intermediates through AIA fetching.
  std::unique_ptr<CertIssuerSourceAia> aia_cert_issuer_source;
  if (net_fetcher) {
    aia_cert_issuer_source = std::make_unique<CertIssuerSourceAia>(net_fetcher);
    path_builder.AddCertIssuerSource(aia_cert_issuer_source.get());
  } else {
    LOG(ERROR) << "No net_fetcher for performing AIA chasing.";
  }
#endif

  path_builder.Run();
}

int AssignVerifyResult(X509Certificate* input_cert,
                       const std::string& hostname,
                       CertPathBuilder::Result& result,
                       VerificationType verification_type,
                       bool checked_revocation_for_some_path,
                       SystemTrustStore* ssl_trust_store,
                       CertVerifyResult* verify_result) {
  const CertPathBuilderResultPath* best_path_possibly_invalid =
      result.GetBestPathPossiblyInvalid();

  if (!best_path_possibly_invalid) {
    // TODO(crbug.com/634443): What errors to communicate? Maybe the path
    // builder should always return some partial path (even if just containing
    // the target), then there is a CertErrors to test.
    verify_result->cert_status |= CERT_STATUS_AUTHORITY_INVALID;
#if defined(STARBOARD)
    // Cobalt only trusts root certificates that come with the binary,
    // If the code reaches this point, it usually means the root certificate
    // of the chain is not found in the content directory of trusted
    // certificates.
    DLOG(ERROR) << "Certificate Authority invalid!";
#endif
    return ERR_CERT_AUTHORITY_INVALID;
  }

  if (hostname == "localhost") {
    DLOG(INFO) << "known host condition is applied";
    return OK;
  }

  const CertPathBuilderResultPath& partial_path = *best_path_possibly_invalid;

  AppendPublicKeyHashes(partial_path, &verify_result->public_key_hashes);

  for (auto it = verify_result->public_key_hashes.rbegin();
       it != verify_result->public_key_hashes.rend() &&
       !verify_result->is_issued_by_known_root;
       ++it) {
    verify_result->is_issued_by_known_root =
        GetNetTrustAnchorHistogramIdForSPKI(*it) != 0;
  }

  bool path_is_valid = partial_path.IsValid();

  const ParsedCertificate* trusted_cert = partial_path.GetTrustedCert();
  if (trusted_cert) {
    if (!verify_result->is_issued_by_known_root) {
      verify_result->is_issued_by_known_root =
          ssl_trust_store->IsKnownRoot(trusted_cert);
    }

    verify_result->is_issued_by_additional_trust_anchor =
        ssl_trust_store->IsAdditionalTrustAnchor(trusted_cert);
  }

  if (path_is_valid && (verification_type == VerificationType::kEV)) {
    verify_result->cert_status |= CERT_STATUS_IS_EV;
  }

  // TODO(eroman): Add documentation for the meaning of
  // CERT_STATUS_REV_CHECKING_ENABLED. Based on the current tests it appears to
  // mean whether revocation checking was attempted during path building,
  // although does not necessarily mean that revocation checking was done for
  // the final returned path.
  if (checked_revocation_for_some_path)
    verify_result->cert_status |= CERT_STATUS_REV_CHECKING_ENABLED;

  verify_result->verified_cert =
      CreateVerifiedCertChain(input_cert, partial_path);

  MapPathBuilderErrorsToCertStatus(partial_path.errors,
                                   &verify_result->cert_status);

  // TODO(eroman): Is it possible that IsValid() fails but no errors were set in
  // partial_path.errors?
  CHECK(path_is_valid || IsCertStatusError(verify_result->cert_status));

  if (!path_is_valid) {
    LOG(ERROR) << "CertVerifyProcBuiltin for " << hostname << " failed:\n"
               << partial_path.errors.ToDebugString(partial_path.certs);
  }

  return IsCertStatusError(verify_result->cert_status)
             ? MapCertStatusToNetError(verify_result->cert_status)
             : OK;
}

// Returns true if retrying path building with a less stringent signature
// algorithm *might* successfully build a path, based on the earlier failed
// |result|.
//
// This implementation is simplistic, and looks only for the presence of the
// kUnacceptableSignatureAlgorithm error somewhere among the built paths.
bool CanTryAgainWithWeakerDigestPolicy(const CertPathBuilder::Result& result) {
  for (const auto& path : result.paths) {
    if (path->errors.ContainsError(
            cert_errors::kUnacceptableSignatureAlgorithm))
      return true;
  }

  return false;
}

int CertVerifyProcBuiltin::VerifyInternal(
    X509Certificate* input_cert,
    const std::string& hostname,
    const std::string& ocsp_response,
    int flags,
    CRLSet* crl_set,
    const CertificateList& additional_trust_anchors,
    CertVerifyResult* verify_result) {
  CertErrors parsing_errors;

  // VerifyInternal() is expected to carry out verifications using the current
  // time stamp.
  base::Time verification_time = base::Time::Now();

  // Parse the target certificate.
  scoped_refptr<ParsedCertificate> target =
      ParseCertificateFromBuffer(input_cert->cert_buffer(), &parsing_errors);
  if (!target) {
    // TODO(crbug.com/634443): Surface these parsing errors?
    verify_result->cert_status |= CERT_STATUS_INVALID;
    return ERR_CERT_INVALID;
  }

  // Parse the provided intermediates.
  CertIssuerSourceStatic intermediates;
  AddIntermediatesToIssuerSource(input_cert, &intermediates);

  // Parse the additional trust anchors and setup trust store.
  std::unique_ptr<SystemTrustStore> ssl_trust_store =
      CreateSslSystemTrustStore();

  for (const auto& x509_cert : additional_trust_anchors) {
    scoped_refptr<ParsedCertificate> cert =
        ParseCertificateFromBuffer(x509_cert->cert_buffer(), &parsing_errors);
    if (cert)
      ssl_trust_store->AddTrustAnchor(cert);
    // TODO(eroman): Surface parsing errors of additional trust anchor.
  }

  // Get the global dependencies.
#if defined(STARBOARD)
  // cert net fetcher is used for fetching AIA certs which Cobalt should not
  // need.
  CertNetFetcher* net_fetcher = nullptr;
#else
  CertNetFetcher* net_fetcher = GetGlobalCertNetFetcher();
#endif
  const EVRootCAMetadata* ev_metadata = EVRootCAMetadata::GetInstance();

  // This boolean tracks whether online revocation checking was performed for
  // *any* of the built paths, and not just the final path returned (used for
  // setting output flag CERT_STATUS_REV_CHECKING_ENABLED).
  bool checked_revocation_for_some_path = false;

  // Run path building with the different parameters (attempts) until a valid
  // path is found. Earlier successful attempts have priority over later
  // attempts.
  //
  // Attempts are enqueued into |attempts| and drained in FIFO order.
  std::vector<BuildPathAttempt> attempts;

  // First try EV validation. Can skip this if the leaf certificate has no
  // chance of verifying as EV (lacks an EV policy).
  if (IsEVCandidate(ev_metadata, target.get()))
    attempts.emplace_back(VerificationType::kEV);

  // Next try DV validation.
  attempts.emplace_back(VerificationType::kDV);

  CertPathBuilder::Result result;
  VerificationType verification_type = VerificationType::kDV;

  // Iterate over |attempts| until there are none left to try, or an attempt
  // succeeded.
  for (size_t cur_attempt_index = 0; cur_attempt_index < attempts.size();
       ++cur_attempt_index) {
    const auto& cur_attempt = attempts[cur_attempt_index];
    verification_type = cur_attempt.verification_type;

    // Run the attempt through the path builder.
    TryBuildPath(target, &intermediates, ssl_trust_store.get(),
                 verification_time, cur_attempt.verification_type,
                 cur_attempt.digest_policy, flags, ocsp_response, crl_set,
                 net_fetcher, ev_metadata, &result,
                 &checked_revocation_for_some_path);

    if (result.HasValidPath())
      break;

    // If this path building attempt (may have) failed due to the chain using a
    // weak signature algorithm, enqueue a similar attempt but with weaker
    // signature algorithms (SHA1) permitted.
    //
    // This fallback is necessary because the CertVerifyProc layer may decide to
    // allow SHA1 based on its own policy, so path building should return
    // possibly weak chains too.
    //
    // TODO(eroman): Would be better for the SHA1 policy to be part of the
    // delegate instead so it can interact with path building.
    if (cur_attempt.digest_policy ==
            SimplePathBuilderDelegate::DigestPolicy::kStrong &&
        CanTryAgainWithWeakerDigestPolicy(result)) {
      BuildPathAttempt sha1_fallback_attempt = cur_attempt;
      sha1_fallback_attempt.digest_policy =
          SimplePathBuilderDelegate::DigestPolicy::kWeakAllowSha1;
      attempts.push_back(sha1_fallback_attempt);
    }
  }

  // Write the results to |*verify_result|.
  return AssignVerifyResult(input_cert, hostname, result, verification_type,
                            checked_revocation_for_some_path,
                            ssl_trust_store.get(), verify_result);
}

}  // namespace

scoped_refptr<CertVerifyProc> CreateCertVerifyProcBuiltin() {
  return scoped_refptr<CertVerifyProc>(new CertVerifyProcBuiltin());
}

}  // namespace net
