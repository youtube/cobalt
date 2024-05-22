// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/cert_verify_tool/verify_using_path_builder.h"

#include <iostream>
#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "crypto/sha2.h"
#include "net/cert/cert_net_fetcher.h"
#include "net/cert/internal/cert_issuer_source_aia.h"
#include "net/cert/internal/system_trust_store.h"
#include "net/cert/pki/cert_issuer_source_static.h"
#include "net/cert/pki/parse_name.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/pki/path_builder.h"
#include "net/cert/pki/simple_path_builder_delegate.h"
#include "net/cert/pki/trust_store_collection.h"
#include "net/cert/pki/trust_store_in_memory.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/tools/cert_verify_tool/cert_verify_tool_util.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

namespace {

// Converts a base::Time::Exploded to a net::der::GeneralizedTime.
// TODO(mattm): This function exists in cast_cert_validator.cc also. Dedupe it?
net::der::GeneralizedTime ConvertExplodedTime(
    const base::Time::Exploded& exploded) {
  net::der::GeneralizedTime result;
  result.year = exploded.year;
  result.month = exploded.month;
  result.day = exploded.day_of_month;
  result.hours = exploded.hour;
  result.minutes = exploded.minute;
  result.seconds = exploded.second;
  return result;
}

bool AddPemEncodedCert(const net::ParsedCertificate* cert,
                       std::vector<std::string>* pem_encoded_chain) {
  std::string der_cert(cert->der_cert().AsStringView());
  std::string pem;
  if (!net::X509Certificate::GetPEMEncodedFromDER(der_cert, &pem)) {
    std::cerr << "ERROR: GetPEMEncodedFromDER failed\n";
    return false;
  }
  pem_encoded_chain->push_back(pem);
  return true;
}

// Dumps a chain of ParsedCertificate objects to a PEM file.
bool DumpParsedCertificateChain(const base::FilePath& file_path,
                                const net::CertPathBuilderResultPath& path) {
  std::vector<std::string> pem_encoded_chain;
  for (const auto& cert : path.certs) {
    if (!AddPemEncodedCert(cert.get(), &pem_encoded_chain))
      return false;
  }

  return WriteToFile(file_path, base::JoinString(pem_encoded_chain, ""));
}

// Returns a hex-encoded sha256 of the DER-encoding of |cert|.
std::string FingerPrintParsedCertificate(const net::ParsedCertificate* cert) {
  std::string hash = crypto::SHA256HashString(cert->der_cert().AsStringView());
  return base::HexEncode(hash.data(), hash.size());
}

std::string SubjectToString(const net::RDNSequence& parsed_subject) {
  std::string subject_str;
  if (!net::ConvertToRFC2253(parsed_subject, &subject_str))
    return std::string();
  return subject_str;
}

// Returns a textual representation of the Subject of |cert|.
std::string SubjectFromParsedCertificate(const net::ParsedCertificate* cert) {
  net::RDNSequence parsed_subject;
  if (!net::ParseName(cert->tbs().subject_tlv, &parsed_subject))
    return std::string();
  return SubjectToString(parsed_subject);
}

// Dumps a ResultPath to std::cout.
void PrintResultPath(const net::CertPathBuilderResultPath* result_path,
                     size_t index,
                     bool is_best) {
  std::cout << "path " << index << " "
            << (result_path->IsValid() ? "valid" : "invalid")
            << (is_best ? " (best)" : "") << "\n";

  // Print the certificate chain.
  for (const auto& cert : result_path->certs) {
    std::cout << " " << FingerPrintParsedCertificate(cert.get()) << " "
              << SubjectFromParsedCertificate(cert.get()) << "\n";
  }

  // Print the certificate policies.
  if (!result_path->user_constrained_policy_set.empty()) {
    std::cout << "Certificate policies:\n";
    for (const auto& policy : result_path->user_constrained_policy_set) {
      CBS cbs;
      CBS_init(&cbs, policy.UnsafeData(), policy.Length());
      bssl::UniquePtr<char> policy_text(CBS_asn1_oid_to_text(&cbs));
      if (policy_text) {
        std::cout << " " << policy_text.get() << "\n";
      } else {
        std::cout << " (invalid OID)\n";
      }
    }
  }

  // Print the errors/warnings if there were any.
  std::string errors_str =
      result_path->errors.ToDebugString(result_path->certs);
  if (!errors_str.empty()) {
    std::cout << "Errors:\n";
    std::cout << errors_str << "\n";
  }
}

std::shared_ptr<const net::ParsedCertificate> ParseCertificate(
    const CertInput& input) {
  net::CertErrors errors;
  std::shared_ptr<const net::ParsedCertificate> cert =
      net::ParsedCertificate::Create(
          net::x509_util::CreateCryptoBuffer(input.der_cert), {}, &errors);
  if (!cert) {
    PrintCertError("ERROR: ParsedCertificate failed:", input);
    std::cout << errors.ToDebugString() << "\n";
  }

  // TODO(crbug.com/634443): Print errors if there are any on success too (i.e.
  //                         warnings).

  return cert;
}

}  // namespace

// Verifies |target_der_cert| using CertPathBuilder.
bool VerifyUsingPathBuilder(
    const CertInput& target_der_cert,
    const std::vector<CertInput>& intermediate_der_certs,
    const std::vector<CertInputWithTrustSetting>& der_certs_with_trust_settings,
    const base::Time at_time,
    const base::FilePath& dump_prefix_path,
    scoped_refptr<net::CertNetFetcher> cert_net_fetcher,
    net::SystemTrustStore* system_trust_store) {
  base::Time::Exploded exploded_time;
  at_time.UTCExplode(&exploded_time);
  net::der::GeneralizedTime time = ConvertExplodedTime(exploded_time);

  net::TrustStoreInMemory additional_roots;
  for (const auto& cert_input_with_trust : der_certs_with_trust_settings) {
    std::shared_ptr<const net::ParsedCertificate> cert =
        ParseCertificate(cert_input_with_trust.cert_input);
    if (cert) {
      additional_roots.AddCertificate(std::move(cert),
                                      cert_input_with_trust.trust);
    }
  }
  net::TrustStoreCollection trust_store;
  trust_store.AddTrustStore(&additional_roots);
  trust_store.AddTrustStore(system_trust_store->GetTrustStore());

  if (!system_trust_store->UsesSystemTrustStore() &&
      der_certs_with_trust_settings.empty()) {
    std::cerr << "NOTE: CertPathBuilder does not currently use OS trust "
                 "settings (--roots must be specified).\n";
  }
  net::CertIssuerSourceStatic intermediate_cert_issuer_source;
  for (const auto& der_cert : intermediate_der_certs) {
    std::shared_ptr<const net::ParsedCertificate> cert =
        ParseCertificate(der_cert);
    if (cert)
      intermediate_cert_issuer_source.AddCert(cert);
  }

  std::shared_ptr<const net::ParsedCertificate> target_cert =
      ParseCertificate(target_der_cert);
  if (!target_cert)
    return false;

  // Verify the chain.
  net::SimplePathBuilderDelegate delegate(
      2048, net::SimplePathBuilderDelegate::DigestPolicy::kWeakAllowSha1);
  net::CertPathBuilder path_builder(
      target_cert, &trust_store, &delegate, time, net::KeyPurpose::SERVER_AUTH,
      net::InitialExplicitPolicy::kFalse, {net::der::Input(net::kAnyPolicyOid)},
      net::InitialPolicyMappingInhibit::kFalse,
      net::InitialAnyPolicyInhibit::kFalse);
  path_builder.AddCertIssuerSource(&intermediate_cert_issuer_source);

  std::unique_ptr<net::CertIssuerSourceAia> aia_cert_issuer_source;
  if (cert_net_fetcher.get()) {
    aia_cert_issuer_source =
        std::make_unique<net::CertIssuerSourceAia>(std::move(cert_net_fetcher));
    path_builder.AddCertIssuerSource(aia_cert_issuer_source.get());
  }

  // TODO(mattm): should this be a command line flag?
  path_builder.SetExploreAllPaths(true);

  // Run the path builder.
  net::CertPathBuilder::Result result = path_builder.Run();

  // TODO(crbug.com/634443): Display any errors/warnings associated with path
  //                         building that were not part of a particular
  //                         PathResult.
  std::cout << "CertPathBuilder result: "
            << (result.HasValidPath() ? "SUCCESS" : "FAILURE") << "\n";

  PrintDebugData(&result);

  for (size_t i = 0; i < result.paths.size(); ++i) {
    PrintResultPath(result.paths[i].get(), i, i == result.best_result_index);
  }

  // TODO(mattm): add flag to dump all paths, not just the final one?
  if (!dump_prefix_path.empty() && !result.paths.empty()) {
    if (!DumpParsedCertificateChain(
            dump_prefix_path.AddExtension(
                FILE_PATH_LITERAL(".CertPathBuilder.pem")),
            *result.GetBestPathPossiblyInvalid())) {
      return false;
    }
  }

  return result.HasValidPath();
}
