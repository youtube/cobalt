// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/transport_security_state.h"

#if defined(USE_OPENSSL)
#include <openssl/ecdsa.h>
#include <openssl/ssl.h>
#else  // !defined(USE_OPENSSL)
#include <cryptohi.h>
#include <hasht.h>
#include <keyhi.h>
#include <pk11pub.h>
#include <nspr.h>
#endif

#include <algorithm>
#include <utility>

#include "base/base64.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/sha1.h"
#include "base/string_number_conversions.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "crypto/sha2.h"
#include "googleurl/src/gurl.h"
#include "net/base/dns_util.h"
#include "net/base/ssl_info.h"
#include "net/base/x509_certificate.h"
#include "net/http/http_util.h"

#if defined(USE_OPENSSL)
#include "crypto/openssl_util.h"
#endif

namespace net {

const long int TransportSecurityState::kMaxHSTSAgeSecs = 86400 * 365;  // 1 year

static std::string HashHost(const std::string& canonicalized_host) {
  char hashed[crypto::kSHA256Length];
  crypto::SHA256HashString(canonicalized_host, hashed, sizeof(hashed));
  return std::string(hashed, sizeof(hashed));
}

TransportSecurityState::TransportSecurityState()
  : delegate_(NULL) {
}

void TransportSecurityState::SetDelegate(
    TransportSecurityState::Delegate* delegate) {
  delegate_ = delegate;
}

void TransportSecurityState::EnableHost(const std::string& host,
                                        const DomainState& state) {
  DCHECK(CalledOnValidThread());

  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return;

  DomainState existing_state;

  // Use the original creation date if we already have this host. (But note
  // that statically-defined states have no |created| date. Therefore, we do
  // not bother to search the SNI-only static states.)
  DomainState state_copy(state);
  if (GetDomainState(host, false /* sni_enabled */, &existing_state) &&
      !existing_state.created.is_null()) {
    state_copy.created = existing_state.created;
  }

  // No need to store this value since it is redundant. (|canonicalized_host|
  // is the map key.)
  state_copy.domain.clear();

  enabled_hosts_[HashHost(canonicalized_host)] = state_copy;
  DirtyNotify();
}

bool TransportSecurityState::DeleteHost(const std::string& host) {
  DCHECK(CalledOnValidThread());

  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return false;

  std::map<std::string, DomainState>::iterator i = enabled_hosts_.find(
      HashHost(canonicalized_host));
  if (i != enabled_hosts_.end()) {
    enabled_hosts_.erase(i);
    DirtyNotify();
    return true;
  }
  return false;
}

bool TransportSecurityState::GetDomainState(const std::string& host,
                                            bool sni_enabled,
                                            DomainState* result) {
  DCHECK(CalledOnValidThread());

  DomainState state;
  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return false;

  bool has_preload = GetStaticDomainState(canonicalized_host, sni_enabled,
                                          &state);
  std::string canonicalized_preload = CanonicalizeHost(state.domain);

  base::Time current_time(base::Time::Now());

  for (size_t i = 0; canonicalized_host[i]; i += canonicalized_host[i] + 1) {
    std::string host_sub_chunk(&canonicalized_host[i],
                               canonicalized_host.size() - i);
    // Exact match of a preload always wins.
    if (has_preload && host_sub_chunk == canonicalized_preload) {
      *result = state;
      return true;
    }

    std::map<std::string, DomainState>::iterator j =
        enabled_hosts_.find(HashHost(host_sub_chunk));
    if (j == enabled_hosts_.end())
      continue;

    if (current_time > j->second.upgrade_expiry &&
        current_time > j->second.dynamic_spki_hashes_expiry) {
      enabled_hosts_.erase(j);
      DirtyNotify();
      continue;
    }

    state = j->second;
    state.domain = DNSDomainToString(host_sub_chunk);

    // Succeed if we matched the domain exactly or if subdomain matches are
    // allowed.
    if (i == 0 || j->second.include_subdomains) {
      *result = state;
      return true;
    }

    return false;
  }

  return false;
}

void TransportSecurityState::DeleteSince(const base::Time& time) {
  DCHECK(CalledOnValidThread());

  bool dirtied = false;

  std::map<std::string, DomainState>::iterator i = enabled_hosts_.begin();
  while (i != enabled_hosts_.end()) {
    if (i->second.created >= time) {
      dirtied = true;
      enabled_hosts_.erase(i++);
    } else {
      i++;
    }
  }

  if (dirtied)
    DirtyNotify();
}

// MaxAgeToInt converts a string representation of a number of seconds into a
// int. We use strtol in order to handle overflow correctly. The string may
// contain an arbitary number which we should truncate correctly rather than
// throwing a parse failure.
static bool MaxAgeToInt(std::string::const_iterator begin,
                        std::string::const_iterator end,
                        int* result) {
  const std::string s(begin, end);
  char* endptr;
  long int i = strtol(s.data(), &endptr, 10 /* base */);
  if (*endptr || i < 0)
    return false;
  if (i > TransportSecurityState::kMaxHSTSAgeSecs)
    i = TransportSecurityState::kMaxHSTSAgeSecs;
  *result = i;
  return true;
}

// Strip, Split, StringPair, and ParsePins are private implementation details
// of ParsePinsHeader(std::string&, DomainState&).
static std::string Strip(const std::string& source) {
  if (source.empty())
    return source;

  std::string::const_iterator start = source.begin();
  std::string::const_iterator end = source.end();
  HttpUtil::TrimLWS(&start, &end);
  return std::string(start, end);
}

typedef std::pair<std::string, std::string> StringPair;

static StringPair Split(const std::string& source, char delimiter) {
  StringPair pair;
  size_t point = source.find(delimiter);

  pair.first = source.substr(0, point);
  if (std::string::npos != point)
    pair.second = source.substr(point + 1);

  return pair;
}

// TODO(palmer): Support both sha256 and sha1. This will require additional
// infrastructure code changes and can come in a later patch.
//
// static
bool TransportSecurityState::ParsePin(const std::string& value,
                                      SHA1Fingerprint* out) {
  StringPair slash = Split(Strip(value), '/');
  if (slash.first != "sha1")
    return false;

  std::string decoded;
  if (!base::Base64Decode(slash.second, &decoded) ||
      decoded.size() != arraysize(out->data)) {
    return false;
  }

  memcpy(out->data, decoded.data(), arraysize(out->data));
  return true;
}

static bool ParseAndAppendPin(const std::string& value,
                      FingerprintVector* fingerprints) {
  // The base64'd fingerprint MUST be a quoted-string. 20 bytes base64'd is 28
  // characters; 32 bytes base64'd is 44 characters. TODO(palmer): Support
  // SHA256.
  size_t size = value.size();
  if (size != 30 || value[0] != '"' || value[size - 1] != '"')
    return false;

  std::string unquoted = HttpUtil::Unquote(value);
  std::string decoded;
  SHA1Fingerprint fp;

  if (!base::Base64Decode(unquoted, &decoded) ||
      decoded.size() != arraysize(fp.data)) {
    return false;
  }

  memcpy(fp.data, decoded.data(), arraysize(fp.data));
  fingerprints->push_back(fp);
  return true;
}

struct FingerprintsEqualPredicate {
  explicit FingerprintsEqualPredicate(const SHA1Fingerprint& fingerprint) :
      fingerprint_(fingerprint) {}

  bool operator()(const SHA1Fingerprint& other) const {
    return fingerprint_.Equals(other);
  }

  const SHA1Fingerprint& fingerprint_;
};

// Returns true iff there is an item in |pins| which is not present in
// |from_cert_chain|. Such an SPKI hash is called a "backup pin".
static bool IsBackupPinPresent(const FingerprintVector& pins,
                               const FingerprintVector& from_cert_chain) {
  for (FingerprintVector::const_iterator
       i = pins.begin(); i != pins.end(); ++i) {
    FingerprintVector::const_iterator j =
        std::find_if(from_cert_chain.begin(), from_cert_chain.end(),
                     FingerprintsEqualPredicate(*i));
      if (j == from_cert_chain.end())
        return true;
  }

  return false;
}

static bool HashesIntersect(const FingerprintVector& a,
                            const FingerprintVector& b) {
  for (FingerprintVector::const_iterator
       i = a.begin(); i != a.end(); ++i) {
    FingerprintVector::const_iterator j =
        std::find_if(b.begin(), b.end(), FingerprintsEqualPredicate(*i));
      if (j != b.end())
        return true;
  }

  return false;
}

// Returns true iff |pins| contains both a live and a backup pin. A live pin
// is a pin whose SPKI is present in the certificate chain in |ssl_info|. A
// backup pin is a pin intended for disaster recovery, not day-to-day use, and
// thus must be absent from the certificate chain. The Public-Key-Pins header
// specification requires both.
static bool IsPinListValid(const FingerprintVector& pins,
                           const SSLInfo& ssl_info) {
  if (pins.size() < 2)
    return false;

  const FingerprintVector& from_cert_chain = ssl_info.public_key_hashes;
  if (from_cert_chain.empty())
    return false;

  return IsBackupPinPresent(pins, from_cert_chain) &&
      HashesIntersect(pins, from_cert_chain);
}

// "Public-Key-Pins" ":"
//     "max-age" "=" delta-seconds ";"
//     "pin-" algo "=" base64 [ ";" ... ]
bool TransportSecurityState::DomainState::ParsePinsHeader(
    const base::Time& now,
    const std::string& value,
    const SSLInfo& ssl_info) {
  bool parsed_max_age = false;
  int max_age_candidate = 0;
  FingerprintVector pins;

  std::string source = value;

  while (!source.empty()) {
    StringPair semicolon = Split(source, ';');
    semicolon.first = Strip(semicolon.first);
    semicolon.second = Strip(semicolon.second);
    StringPair equals = Split(semicolon.first, '=');
    equals.first = Strip(equals.first);
    equals.second = Strip(equals.second);

    if (LowerCaseEqualsASCII(equals.first, "max-age")) {
      if (equals.second.empty() ||
          !MaxAgeToInt(equals.second.begin(), equals.second.end(),
                       &max_age_candidate)) {
        return false;
      }
      if (max_age_candidate > kMaxHSTSAgeSecs)
        max_age_candidate = kMaxHSTSAgeSecs;
      parsed_max_age = true;
    } else if (LowerCaseEqualsASCII(equals.first, "pin-sha1")) {
      if (!ParseAndAppendPin(equals.second, &pins))
        return false;
    } else if (LowerCaseEqualsASCII(equals.first, "pin-sha256")) {
      // TODO(palmer)
    } else {
      // Silently ignore unknown directives for forward compatibility.
    }

    source = semicolon.second;
  }

  if (!parsed_max_age || !IsPinListValid(pins, ssl_info))
    return false;

  dynamic_spki_hashes_expiry =
      now + base::TimeDelta::FromSeconds(max_age_candidate);

  dynamic_spki_hashes.clear();
  if (max_age_candidate > 0) {
    for (FingerprintVector::const_iterator i = pins.begin();
         i != pins.end(); ++i) {
      dynamic_spki_hashes.push_back(*i);
    }
  }

  return true;
}

// "Strict-Transport-Security" ":"
//     "max-age" "=" delta-seconds [ ";" "includeSubDomains" ]
bool TransportSecurityState::DomainState::ParseSTSHeader(
    const base::Time& now,
    const std::string& value) {
  int max_age_candidate = 0;

  enum ParserState {
    START,
    AFTER_MAX_AGE_LABEL,
    AFTER_MAX_AGE_EQUALS,
    AFTER_MAX_AGE,
    AFTER_MAX_AGE_INCLUDE_SUB_DOMAINS_DELIMITER,
    AFTER_INCLUDE_SUBDOMAINS,
  } state = START;

  StringTokenizer tokenizer(value, " \t=;");
  tokenizer.set_options(StringTokenizer::RETURN_DELIMS);
  while (tokenizer.GetNext()) {
    DCHECK(!tokenizer.token_is_delim() || tokenizer.token().length() == 1);
    switch (state) {
      case START:
        if (IsAsciiWhitespace(*tokenizer.token_begin()))
          continue;
        if (!LowerCaseEqualsASCII(tokenizer.token(), "max-age"))
          return false;
        state = AFTER_MAX_AGE_LABEL;
        break;

      case AFTER_MAX_AGE_LABEL:
        if (IsAsciiWhitespace(*tokenizer.token_begin()))
          continue;
        if (*tokenizer.token_begin() != '=')
          return false;
        DCHECK_EQ(tokenizer.token().length(), 1U);
        state = AFTER_MAX_AGE_EQUALS;
        break;

      case AFTER_MAX_AGE_EQUALS:
        if (IsAsciiWhitespace(*tokenizer.token_begin()))
          continue;
        if (!MaxAgeToInt(tokenizer.token_begin(),
                         tokenizer.token_end(),
                         &max_age_candidate))
          return false;
        state = AFTER_MAX_AGE;
        break;

      case AFTER_MAX_AGE:
        if (IsAsciiWhitespace(*tokenizer.token_begin()))
          continue;
        if (*tokenizer.token_begin() != ';')
          return false;
        state = AFTER_MAX_AGE_INCLUDE_SUB_DOMAINS_DELIMITER;
        break;

      case AFTER_MAX_AGE_INCLUDE_SUB_DOMAINS_DELIMITER:
        if (IsAsciiWhitespace(*tokenizer.token_begin()))
          continue;
        if (!LowerCaseEqualsASCII(tokenizer.token(), "includesubdomains"))
          return false;
        state = AFTER_INCLUDE_SUBDOMAINS;
        break;

      case AFTER_INCLUDE_SUBDOMAINS:
        if (!IsAsciiWhitespace(*tokenizer.token_begin()))
          return false;
        break;

      default:
        NOTREACHED();
    }
  }

  // We've consumed all the input.  Let's see what state we ended up in.
  switch (state) {
    case START:
    case AFTER_MAX_AGE_LABEL:
    case AFTER_MAX_AGE_EQUALS:
      return false;
    case AFTER_MAX_AGE:
      upgrade_expiry =
          now + base::TimeDelta::FromSeconds(max_age_candidate);
      include_subdomains = false;
      upgrade_mode = MODE_FORCE_HTTPS;
      return true;
    case AFTER_MAX_AGE_INCLUDE_SUB_DOMAINS_DELIMITER:
      return false;
    case AFTER_INCLUDE_SUBDOMAINS:
      upgrade_expiry =
          now + base::TimeDelta::FromSeconds(max_age_candidate);
      include_subdomains = true;
      upgrade_mode = MODE_FORCE_HTTPS;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

static bool AddHash(const std::string& type_and_base64,
                    FingerprintVector* out) {
  SHA1Fingerprint hash;

  if (!TransportSecurityState::ParsePin(type_and_base64, &hash))
    return false;

  out->push_back(hash);
  return true;
}

TransportSecurityState::~TransportSecurityState() {}

void TransportSecurityState::DirtyNotify() {
  DCHECK(CalledOnValidThread());

  if (delegate_)
    delegate_->StateIsDirty(this);
}

// static
std::string TransportSecurityState::CanonicalizeHost(const std::string& host) {
  // We cannot perform the operations as detailed in the spec here as |host|
  // has already undergone IDN processing before it reached us. Thus, we check
  // that there are no invalid characters in the host and lowercase the result.

  std::string new_host;
  if (!DNSDomainFromDot(host, &new_host)) {
    // DNSDomainFromDot can fail if any label is > 63 bytes or if the whole
    // name is >255 bytes. However, search terms can have those properties.
    return std::string();
  }

  for (size_t i = 0; new_host[i]; i += new_host[i] + 1) {
    const unsigned label_length = static_cast<unsigned>(new_host[i]);
    if (!label_length)
      break;

    for (size_t j = 0; j < label_length; ++j) {
      // RFC 3490, 4.1, step 3
      if (!IsSTD3ASCIIValidCharacter(new_host[i + 1 + j]))
        return std::string();

      new_host[i + 1 + j] = tolower(new_host[i + 1 + j]);
    }

    // step 3(b)
    if (new_host[i + 1] == '-' ||
        new_host[i + label_length] == '-') {
      return std::string();
    }
  }

  return new_host;
}

// |ReportUMAOnPinFailure| uses these to report which domain was associated
// with the public key pinning failure.
//
// DO NOT CHANGE THE ORDERING OF THESE NAMES OR REMOVE ANY OF THEM. Add new
// domains at the END of the listing (but before DOMAIN_NUM_EVENTS).
enum SecondLevelDomainName {
  DOMAIN_NOT_PINNED,

  DOMAIN_GOOGLE_COM,
  DOMAIN_ANDROID_COM,
  DOMAIN_GOOGLE_ANALYTICS_COM,
  DOMAIN_GOOGLEPLEX_COM,
  DOMAIN_YTIMG_COM,
  DOMAIN_GOOGLEUSERCONTENT_COM,
  DOMAIN_YOUTUBE_COM,
  DOMAIN_GOOGLEAPIS_COM,
  DOMAIN_GOOGLEADSERVICES_COM,
  DOMAIN_GOOGLECODE_COM,
  DOMAIN_APPSPOT_COM,
  DOMAIN_GOOGLESYNDICATION_COM,
  DOMAIN_DOUBLECLICK_NET,
  DOMAIN_GSTATIC_COM,
  DOMAIN_GMAIL_COM,
  DOMAIN_GOOGLEMAIL_COM,
  DOMAIN_GOOGLEGROUPS_COM,

  DOMAIN_TORPROJECT_ORG,

  DOMAIN_TWITTER_COM,
  DOMAIN_TWIMG_COM,

  DOMAIN_AKAMAIHD_NET,

  // Boundary value for UMA_HISTOGRAM_ENUMERATION:
  DOMAIN_NUM_EVENTS
};

// PublicKeyPins contains a number of SubjectPublicKeyInfo hashes for a site.
// The validated certificate chain for the site must not include any of
// |excluded_hashes| and must include one or more of |required_hashes|.
struct PublicKeyPins {
  const char* const* required_hashes;
  const char* const* excluded_hashes;
};

struct HSTSPreload {
  uint8 length;
  bool include_subdomains;
  char dns_name[30];
  bool https_required;
  PublicKeyPins pins;
  SecondLevelDomainName second_level_domain_name;
};

static bool HasPreload(const struct HSTSPreload* entries, size_t num_entries,
                       const std::string& canonicalized_host, size_t i,
                       TransportSecurityState::DomainState* out, bool* ret) {
  for (size_t j = 0; j < num_entries; j++) {
    if (entries[j].length == canonicalized_host.size() - i &&
        memcmp(entries[j].dns_name, &canonicalized_host[i],
               entries[j].length) == 0) {
      if (!entries[j].include_subdomains && i != 0) {
        *ret = false;
      } else {
        out->include_subdomains = entries[j].include_subdomains;
        *ret = true;
        if (!entries[j].https_required)
          out->upgrade_mode = TransportSecurityState::DomainState::MODE_DEFAULT;
        if (entries[j].pins.required_hashes) {
          const char* const* hash = entries[j].pins.required_hashes;
          while (*hash) {
            bool ok = AddHash(*hash, &out->static_spki_hashes);
            DCHECK(ok) << " failed to parse " << *hash;
            hash++;
          }
        }
        if (entries[j].pins.excluded_hashes) {
          const char* const* hash = entries[j].pins.excluded_hashes;
          while (*hash) {
            bool ok = AddHash(*hash, &out->bad_static_spki_hashes);
            DCHECK(ok) << " failed to parse " << *hash;
            hash++;
          }
        }
      }
      return true;
    }
  }
  return false;
}

#include "net/base/transport_security_state_static.h"

// Returns the HSTSPreload entry for the |canonicalized_host| in |entries|,
// or NULL if there is none. Prefers exact hostname matches to those that
// match only because HSTSPreload.include_subdomains is true.
//
// |canonicalized_host| should be the hostname as canonicalized by
// CanonicalizeHost.
static const struct HSTSPreload* GetHSTSPreload(
    const std::string& canonicalized_host,
    const struct HSTSPreload* entries,
    size_t num_entries) {
  for (size_t i = 0; canonicalized_host[i]; i += canonicalized_host[i] + 1) {
    for (size_t j = 0; j < num_entries; j++) {
      const struct HSTSPreload* entry = entries + j;

      if (i != 0 && !entry->include_subdomains)
        continue;

      if (entry->length == canonicalized_host.size() - i &&
          memcmp(entry->dns_name, &canonicalized_host[i], entry->length) == 0) {
        return entry;
      }
    }
  }

  return NULL;
}

// static
bool TransportSecurityState::IsGooglePinnedProperty(const std::string& host,
                                                    bool sni_enabled) {
  std::string canonicalized_host = CanonicalizeHost(host);
  const struct HSTSPreload* entry =
      GetHSTSPreload(canonicalized_host, kPreloadedSTS, kNumPreloadedSTS);

  if (entry && entry->pins.required_hashes == kGoogleAcceptableCerts)
    return true;

  if (sni_enabled) {
    entry = GetHSTSPreload(canonicalized_host, kPreloadedSNISTS,
                           kNumPreloadedSNISTS);
    if (entry && entry->pins.required_hashes == kGoogleAcceptableCerts)
      return true;
  }

  return false;
}

// static
void TransportSecurityState::ReportUMAOnPinFailure(const std::string& host) {
  std::string canonicalized_host = CanonicalizeHost(host);

  const struct HSTSPreload* entry =
      GetHSTSPreload(canonicalized_host, kPreloadedSTS, kNumPreloadedSTS);

  if (!entry) {
    entry = GetHSTSPreload(canonicalized_host, kPreloadedSNISTS,
                           kNumPreloadedSNISTS);
  }

  DCHECK(entry);
  DCHECK(entry->pins.required_hashes);
  DCHECK(entry->second_level_domain_name != DOMAIN_NOT_PINNED);

  UMA_HISTOGRAM_ENUMERATION("Net.PublicKeyPinFailureDomain",
                            entry->second_level_domain_name, DOMAIN_NUM_EVENTS);
}

bool TransportSecurityState::GetStaticDomainState(
    const std::string& canonicalized_host,
    bool sni_enabled,
    DomainState* out) {
  DCHECK(CalledOnValidThread());

  out->upgrade_mode = DomainState::MODE_FORCE_HTTPS;
  out->include_subdomains = false;

  for (size_t i = 0; canonicalized_host[i]; i += canonicalized_host[i] + 1) {
    std::string host_sub_chunk(&canonicalized_host[i],
                               canonicalized_host.size() - i);
    out->domain = DNSDomainToString(host_sub_chunk);
    std::string hashed_host(HashHost(host_sub_chunk));
    if (forced_hosts_.find(hashed_host) != forced_hosts_.end()) {
      *out = forced_hosts_[hashed_host];
      out->domain = DNSDomainToString(host_sub_chunk);
      return true;
    }
    bool ret;
    if (HasPreload(kPreloadedSTS, kNumPreloadedSTS, canonicalized_host, i, out,
                   &ret)) {
      return ret;
    }
    if (sni_enabled &&
        HasPreload(kPreloadedSNISTS, kNumPreloadedSNISTS, canonicalized_host, i,
                   out, &ret)) {
      return ret;
    }
  }

  return false;
}

void TransportSecurityState::AddOrUpdateEnabledHosts(
    const std::string& hashed_host, const DomainState& state) {
  enabled_hosts_[hashed_host] = state;
}

void TransportSecurityState::AddOrUpdateForcedHosts(
    const std::string& hashed_host, const DomainState& state) {
  forced_hosts_[hashed_host] = state;
}

static std::string HashesToBase64String(
    const FingerprintVector& hashes) {
  std::vector<std::string> hashes_strs;
  for (FingerprintVector::const_iterator
       i = hashes.begin(); i != hashes.end(); i++) {
    std::string s;
    const std::string hash_str(reinterpret_cast<const char*>(i->data),
                               sizeof(i->data));
    base::Base64Encode(hash_str, &s);
    hashes_strs.push_back(s);
  }

  return JoinString(hashes_strs, ',');
}

TransportSecurityState::DomainState::DomainState()
    : upgrade_mode(MODE_FORCE_HTTPS),
      created(base::Time::Now()),
      include_subdomains(false) {
}

TransportSecurityState::DomainState::~DomainState() {
}

bool TransportSecurityState::DomainState::IsChainOfPublicKeysPermitted(
    const FingerprintVector& hashes) const {
  if (HashesIntersect(bad_static_spki_hashes, hashes)) {
    LOG(ERROR) << "Rejecting public key chain for domain " << domain
               << ". Validated chain: " << HashesToBase64String(hashes)
               << ", matches one or more bad hashes: "
               << HashesToBase64String(bad_static_spki_hashes);
    return false;
  }

  if (!(dynamic_spki_hashes.empty() && static_spki_hashes.empty()) &&
      !HashesIntersect(dynamic_spki_hashes, hashes) &&
      !HashesIntersect(static_spki_hashes, hashes)) {
    LOG(ERROR) << "Rejecting public key chain for domain " << domain
               << ". Validated chain: " << HashesToBase64String(hashes)
               << ", expected: " << HashesToBase64String(dynamic_spki_hashes)
               << " or: " << HashesToBase64String(static_spki_hashes);

    return false;
  }

  return true;
}

bool TransportSecurityState::DomainState::ShouldRedirectHTTPToHTTPS() const {
  return upgrade_mode == MODE_FORCE_HTTPS;
}

bool TransportSecurityState::DomainState::Equals(
    const DomainState& other) const {
  // TODO(palmer): Implement this
  (void) other;
  return true;
}

bool TransportSecurityState::DomainState::HasPins() const {
  return static_spki_hashes.size() > 0 ||
         bad_static_spki_hashes.size() > 0 ||
         dynamic_spki_hashes.size() > 0;
}

}  // namespace
