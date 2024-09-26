// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/certificate_transparency/chrome_require_ct_delegate.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/containers/cxx20_erase.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "components/url_formatter/url_fixer.h"
#include "components/url_matcher/url_matcher.h"
#include "crypto/sha2.h"
#include "net/base/hash_value.h"
#include "net/base/host_port_pair.h"
#include "net/cert/asn1_util.h"
#include "net/cert/known_roots.h"
#include "net/cert/pki/name_constraints.h"
#include "net/cert/pki/parse_name.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"

namespace certificate_transparency {

namespace {

// Helper that takes a given net::RDNSequence and returns only the
// organizationName net::X509NameAttributes.
class OrgAttributeFilter {
 public:
  // Creates a new OrgAttributeFilter for |sequence| that begins iterating at
  // |head|. Note that |head| can be equal to |sequence.end()|, in which case,
  // there are no organizationName attributes.
  explicit OrgAttributeFilter(const net::RDNSequence& sequence)
      : sequence_head_(sequence.begin()), sequence_end_(sequence.end()) {
    if (sequence_head_ != sequence_end_) {
      rdn_it_ = sequence_head_->begin();
      AdvanceIfNecessary();
    }
  }

  bool IsValid() const { return sequence_head_ != sequence_end_; }

  const net::X509NameAttribute& GetAttribute() const {
    DCHECK(IsValid());
    return *rdn_it_;
  }

  void Advance() {
    DCHECK(IsValid());
    ++rdn_it_;
    AdvanceIfNecessary();
  }

 private:
  // If the current field is an organization field, does nothing, otherwise,
  // advances the state to the next organization field, or, if no more are
  // present, the end of the sequence.
  void AdvanceIfNecessary() {
    while (sequence_head_ != sequence_end_) {
      while (rdn_it_ != sequence_head_->end()) {
        if (rdn_it_->type == net::der::Input(net::kTypeOrganizationNameOid))
          return;
        ++rdn_it_;
      }
      ++sequence_head_;
      if (sequence_head_ != sequence_end_) {
        rdn_it_ = sequence_head_->begin();
      }
    }
  }

  net::RDNSequence::const_iterator sequence_head_;
  net::RDNSequence::const_iterator sequence_end_;
  net::RelativeDistinguishedName::const_iterator rdn_it_;
};

// Returns true if |dn_without_sequence| identifies an
// organizationally-validated certificate, per the CA/Browser Forum's Baseline
// Requirements, storing the parsed RDNSequence in |*out|.
bool ParseOrganizationBoundName(net::der::Input dn_without_sequence,
                                net::RDNSequence* out) {
  if (!net::ParseNameValue(dn_without_sequence, out))
    return false;
  for (const auto& rdn : *out) {
    for (const auto& attribute_type_and_value : rdn) {
      if (attribute_type_and_value.type ==
          net::der::Input(net::kTypeOrganizationNameOid)) {
        return true;
      }
    }
  }
  return false;
}

// Returns true if the certificate identified by |leaf_rdn_sequence| is
// considered to be issued under the same organizational authority as
// |org_cert|.
bool AreCertsSameOrganization(const net::RDNSequence& leaf_rdn_sequence,
                              CRYPTO_BUFFER* org_cert) {
  std::shared_ptr<const net::ParsedCertificate> parsed_org =
      net::ParsedCertificate::Create(bssl::UpRef(org_cert),
                                     net::ParseCertificateOptions(), nullptr);
  if (!parsed_org)
    return false;

  // If the candidate cert has nameConstraints, see if it has a
  // permittedSubtrees nameConstraint over a DirectoryName that is
  // organizationally-bound. If so, the enforcement of nameConstraints is
  // sufficient to consider |org_cert| a match.
  if (parsed_org->has_name_constraints()) {
    const net::NameConstraints& nc = parsed_org->name_constraints();
    for (const auto& permitted_name : nc.permitted_subtrees().directory_names) {
      net::RDNSequence tmp;
      if (ParseOrganizationBoundName(permitted_name, &tmp))
        return true;
    }
  }

  net::RDNSequence org_rdn_sequence;
  if (!net::ParseNameValue(parsed_org->normalized_subject(), &org_rdn_sequence))
    return false;

  // Finally, try to match the organization fields within |leaf_rdn_sequence|
  // to |org_rdn_sequence|. As |leaf_rdn_sequence| has already been checked
  // for all the necessary fields, it's not necessary to check
  // |org_rdn_sequence|. Iterate through all of the organization fields in
  // each, doing a byte-for-byte equality check.
  // Note that this does permit differences in the SET encapsulations between
  // RelativeDistinguishedNames, although it does still require that the same
  // number of organization fields appear, and with the same overall ordering.
  // This is simply as an implementation simplification, and not done for
  // semantic or technical reasons.
  OrgAttributeFilter leaf_filter(leaf_rdn_sequence);
  OrgAttributeFilter org_filter(org_rdn_sequence);
  while (leaf_filter.IsValid() && org_filter.IsValid()) {
    if (leaf_filter.GetAttribute().type != org_filter.GetAttribute().type ||
        leaf_filter.GetAttribute().value_tag !=
            org_filter.GetAttribute().value_tag ||
        leaf_filter.GetAttribute().value != org_filter.GetAttribute().value) {
      return false;
    }
    leaf_filter.Advance();
    org_filter.Advance();
  }

  // Ensure all attributes were fully consumed.
  return !leaf_filter.IsValid() && !org_filter.IsValid();
}

}  // namespace

ChromeRequireCTDelegate::ChromeRequireCTDelegate()
    : url_matcher_(std::make_unique<url_matcher::URLMatcher>()), next_id_(0) {}

ChromeRequireCTDelegate::~ChromeRequireCTDelegate() {}

net::TransportSecurityState::RequireCTDelegate::CTRequirementLevel
ChromeRequireCTDelegate::IsCTRequiredForHost(
    const std::string& hostname,
    const net::X509Certificate* chain,
    const net::HashValueVector& spki_hashes) {
  bool ct_required = false;
  if (MatchHostname(hostname, &ct_required) ||
      MatchSPKI(chain, spki_hashes, &ct_required)) {
    return ct_required ? CTRequirementLevel::REQUIRED
                       : CTRequirementLevel::NOT_REQUIRED;
  }

  // Compute >= 2018-05-01, rather than deal with possible fractional
  // seconds.
  const base::Time kMay_1_2018 =
      base::Time::UnixEpoch() + base::Seconds(1525132800);
  if (chain->valid_start() >= kMay_1_2018)
    return CTRequirementLevel::REQUIRED;

  return CTRequirementLevel::DEFAULT;
}

void ChromeRequireCTDelegate::UpdateCTPolicies(
    const std::vector<std::string>& required_hosts,
    const std::vector<std::string>& excluded_hosts,
    const std::vector<std::string>& excluded_spkis,
    const std::vector<std::string>& excluded_legacy_spkis) {
  url_matcher_ = std::make_unique<url_matcher::URLMatcher>();
  filters_.clear();
  next_id_ = 0;

  url_matcher::URLMatcherConditionSet::Vector all_conditions;
  AddFilters(true, required_hosts, &all_conditions);
  AddFilters(false, excluded_hosts, &all_conditions);

  url_matcher_->AddConditionSets(all_conditions);

  ParseSpkiHashes(excluded_spkis, &spkis_);
  ParseSpkiHashes(excluded_legacy_spkis, &legacy_spkis_);

  // Filter out SPKIs that aren't for legacy CAs.
  base::EraseIf(legacy_spkis_, [](const net::HashValue& hash) {
    if (!net::IsLegacyPubliclyTrustedCA(hash)) {
      LOG(ERROR) << "Non-legacy SPKI configured " << hash.ToString();
      return true;
    }
    return false;
  });
}

bool ChromeRequireCTDelegate::MatchHostname(const std::string& hostname,
                                            bool* ct_required) const {
  if (url_matcher_->IsEmpty())
    return false;

  // Scheme and port are ignored by the policy, so it's OK to construct a
  // new GURL here. However, |hostname| is in network form, not URL form,
  // so it's necessary to wrap IPv6 addresses in brackets.
  std::set<base::MatcherStringPattern::ID> matching_ids =
      url_matcher_->MatchURL(
          GURL("https://" + net::HostPortPair(hostname, 443).HostForURL()));
  if (matching_ids.empty())
    return false;

  // Determine the overall policy by determining the most specific policy.
  auto it = filters_.begin();
  const Filter* active_filter = nullptr;
  for (const auto& match : matching_ids) {
    // Because both |filters_| and |matching_ids| are sorted on the ID,
    // treat both as forward-only iterators.
    while (it != filters_.end() && it->first < match)
      ++it;
    if (it == filters_.end()) {
      NOTREACHED();
      break;
    }

    if (!active_filter || FilterTakesPrecedence(it->second, *active_filter))
      active_filter = &it->second;
  }
  CHECK(active_filter);

  *ct_required = active_filter->ct_required;
  return true;
}

bool ChromeRequireCTDelegate::MatchSPKI(const net::X509Certificate* chain,
                                        const net::HashValueVector& hashes,
                                        bool* ct_required) const {
  // Try to scan legacy SPKIs first, if any, since they will only require
  // comparing hash values.
  if (!legacy_spkis_.empty()) {
    for (const auto& hash : hashes) {
      if (std::binary_search(legacy_spkis_.begin(), legacy_spkis_.end(),
                             hash)) {
        *ct_required = false;
        return true;
      }
    }
  }

  if (spkis_.empty())
    return false;

  // Scan the constrained SPKIs via |hashes| first, as an optimization. If
  // there are matches, the SPKI hash will have to be recomputed anyways to
  // find the matching certificate, but avoid recomputing all the hashes for
  // the case where there is no match.
  net::HashValueVector matches;
  for (const auto& hash : hashes) {
    if (std::binary_search(spkis_.begin(), spkis_.end(), hash)) {
      matches.push_back(hash);
    }
  }
  if (matches.empty())
    return false;

  CRYPTO_BUFFER* leaf_cert = chain->cert_buffer();

  // As an optimization, since the leaf is allowed to be listed as an SPKI,
  // a match on the leaf's SPKI hash can return early, without comparing
  // the organization information to itself.
  net::HashValue hash;
  if (net::x509_util::CalculateSha256SpkiHash(leaf_cert, &hash) &&
      base::Contains(matches, hash)) {
    *ct_required = false;
    return true;
  }

  // If there was a match (or multiple matches), it's necessary to recompute
  // the hashes to find the associated certificate.
  std::vector<CRYPTO_BUFFER*> candidates;
  for (const auto& buffer : chain->intermediate_buffers()) {
    if (net::x509_util::CalculateSha256SpkiHash(buffer.get(), &hash) &&
        base::Contains(matches, hash)) {
      candidates.push_back(buffer.get());
    }
  }

  if (candidates.empty())
    return false;

  std::shared_ptr<const net::ParsedCertificate> parsed_leaf =
      net::ParsedCertificate::Create(bssl::UpRef(leaf_cert),
                                     net::ParseCertificateOptions(), nullptr);
  if (!parsed_leaf)
    return false;
  // If the leaf is not organizationally-bound, it's not a match.
  net::RDNSequence leaf_rdn_sequence;
  if (!ParseOrganizationBoundName(parsed_leaf->normalized_subject(),
                                  &leaf_rdn_sequence)) {
    return false;
  }

  for (auto* cert : candidates) {
    if (AreCertsSameOrganization(leaf_rdn_sequence, cert)) {
      *ct_required = false;
      return true;
    }
  }

  return false;
}

void ChromeRequireCTDelegate::AddFilters(
    bool ct_required,
    const std::vector<std::string>& hosts,
    url_matcher::URLMatcherConditionSet::Vector* conditions) {
  for (const auto& pattern : hosts) {
    Filter filter;
    filter.ct_required = ct_required;

    // Parse the pattern just to the hostname, ignoring all other portions of
    // the URL.
    url::Parsed parsed;
    std::string ignored_scheme = url_formatter::SegmentURL(pattern, &parsed);
    if (parsed.host.is_empty())
      continue;  // If there is no host to match, can't apply the filter.

    std::string lc_host = base::ToLowerASCII(
        base::StringPiece(pattern).substr(parsed.host.begin, parsed.host.len));
    if (lc_host == "*") {
      // Wildcard hosts are not allowed and ignored.
      continue;
    } else if (lc_host[0] == '.') {
      // A leading dot means exact match and to not match subdomains.
      lc_host.erase(0, 1);
      filter.match_subdomains = false;
    } else {
      // Canonicalize the host to make sure it's an actual hostname, not an
      // IP address or a BROKEN canonical host, as matching subdomains is
      // not desirable for those.
      url::RawCanonOutputT<char> output;
      url::CanonHostInfo host_info;
      url::CanonicalizeHostVerbose(pattern.c_str(), parsed.host, &output,
                                   &host_info);
      // TODO(rsleevi): Use canonicalized form?
      if (host_info.family == url::CanonHostInfo::NEUTRAL) {
        // Match subdomains (implicit by the omission of '.'). Add in a
        // leading dot to make sure matches only happen at the domain
        // component boundary.
        lc_host.insert(lc_host.begin(), '.');
        filter.match_subdomains = true;
      } else {
        filter.match_subdomains = false;
      }
    }
    filter.host_length = lc_host.size();

    // Create a condition for the URLMatcher that matches the hostname (and/or
    // subdomains).
    url_matcher::URLMatcherConditionFactory* condition_factory =
        url_matcher_->condition_factory();
    std::set<url_matcher::URLMatcherCondition> condition_set;
    condition_set.insert(
        filter.match_subdomains
            ? condition_factory->CreateHostSuffixCondition(lc_host)
            : condition_factory->CreateHostEqualsCondition(lc_host));
    conditions->push_back(
        new url_matcher::URLMatcherConditionSet(next_id_, condition_set));
    filters_[next_id_] = filter;
    ++next_id_;
  }
}

void ChromeRequireCTDelegate::ParseSpkiHashes(
    const std::vector<std::string> spki_list,
    net::HashValueVector* hashes) const {
  hashes->clear();
  for (const auto& value : spki_list) {
    net::HashValue hash;
    if (!hash.FromString(value)) {
      continue;
    }
    hashes->push_back(std::move(hash));
  }
  std::sort(hashes->begin(), hashes->end());
}

bool ChromeRequireCTDelegate::FilterTakesPrecedence(const Filter& lhs,
                                                    const Filter& rhs) const {
  if (lhs.match_subdomains != rhs.match_subdomains)
    return !lhs.match_subdomains;  // Prefer the more explicit policy.

  if (lhs.host_length != rhs.host_length)
    return lhs.host_length > rhs.host_length;  // Prefer the longer host match.

  if (lhs.ct_required != rhs.ct_required)
    return lhs.ct_required;  // Prefer the policy that requires CT.

  return false;
}

}  // namespace certificate_transparency
