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
#include "net/base/x509_cert_types.h"
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

TransportSecurityState::Iterator::Iterator(const TransportSecurityState& state)
    : iterator_(state.enabled_hosts_.begin()),
      end_(state.enabled_hosts_.end()) {
}

TransportSecurityState::Iterator::~Iterator() {}

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

// static
bool TransportSecurityState::ParsePin(const std::string& value,
                                      HashValue* out) {
  StringPair slash = Split(Strip(value), '/');

  if (slash.first == "sha1")
    out->tag = HASH_VALUE_SHA1;
  else if (slash.first == "sha256")
    out->tag = HASH_VALUE_SHA256;
  else
    return false;

  std::string decoded;
  if (!base::Base64Decode(slash.second, &decoded) ||
      decoded.size() != out->size()) {
    return false;
  }

  memcpy(out->data(), decoded.data(), out->size());
  return true;
}

static bool ParseAndAppendPin(const std::string& value,
                              HashValueTag tag,
                              HashValueVector* hashes) {
  std::string unquoted = HttpUtil::Unquote(value);
  std::string decoded;

  // This code has to assume that 32 bytes is SHA-256 and 20 bytes is SHA-1.
  // Currently, those are the only two possibilities, so the assumption is
  // valid.
  if (!base::Base64Decode(unquoted, &decoded))
    return false;

  HashValue hash(tag);
  if (decoded.size() != hash.size())
    return false;

  memcpy(hash.data(), decoded.data(), hash.size());
  hashes->push_back(hash);
  return true;
}

struct HashValuesEqualPredicate {
  explicit HashValuesEqualPredicate(const HashValue& fingerprint) :
      fingerprint_(fingerprint) {}

  bool operator()(const HashValue& other) const {
    return fingerprint_.Equals(other);
  }

  const HashValue& fingerprint_;
};

// Returns true iff there is an item in |pins| which is not present in
// |from_cert_chain|. Such an SPKI hash is called a "backup pin".
static bool IsBackupPinPresent(const HashValueVector& pins,
                               const HashValueVector& from_cert_chain) {
  for (HashValueVector::const_iterator
       i = pins.begin(); i != pins.end(); ++i) {
    HashValueVector::const_iterator j =
        std::find_if(from_cert_chain.begin(), from_cert_chain.end(),
                     HashValuesEqualPredicate(*i));
      if (j == from_cert_chain.end())
        return true;
  }

  return false;
}

// Returns true if the intersection of |a| and |b| is not empty. If either
// |a| or |b| is empty, returns false.
static bool HashesIntersect(const HashValueVector& a,
                            const HashValueVector& b) {
  for (HashValueVector::const_iterator i = a.begin(); i != a.end(); ++i) {
    HashValueVector::const_iterator j =
        std::find_if(b.begin(), b.end(), HashValuesEqualPredicate(*i));
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
static bool IsPinListValid(const HashValueVector& pins,
                           const SSLInfo& ssl_info) {
  // Fast fail: 1 live + 1 backup = at least 2 pins. (Check for actual
  // liveness and backupness below.)
  if (pins.size() < 2)
    return false;

  const HashValueVector& from_cert_chain = ssl_info.public_key_hashes;
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
  HashValueVector pins;

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
    } else if (StartsWithASCII(equals.first, "pin-", false)) {
      HashValueTag tag;
      if (LowerCaseEqualsASCII(equals.first, "pin-sha1")) {
        tag = HASH_VALUE_SHA1;
      } else if (LowerCaseEqualsASCII(equals.first, "pin-sha256")) {
        tag = HASH_VALUE_SHA256;
      } else {
        LOG(WARNING) << "Ignoring pin of unknown type: " << equals.first;
        return false;
      }
      if (!ParseAndAppendPin(equals.second, tag, &pins))
        return false;
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
    for (HashValueVector::const_iterator i = pins.begin();
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
                    HashValueVector* out) {
  HashValue hash;

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

  DOMAIN_TOR2WEB_ORG,

  DOMAIN_YOUTU_BE,
  DOMAIN_GOOGLECOMMERCE_COM,
  DOMAIN_URCHIN_COM,
  DOMAIN_GOO_GL,
  DOMAIN_G_CO,
  DOMAIN_GOOGLE_AC,
  DOMAIN_GOOGLE_AD,
  DOMAIN_GOOGLE_AE,
  DOMAIN_GOOGLE_AF,
  DOMAIN_GOOGLE_AG,
  DOMAIN_GOOGLE_AM,
  DOMAIN_GOOGLE_AS,
  DOMAIN_GOOGLE_AT,
  DOMAIN_GOOGLE_AZ,
  DOMAIN_GOOGLE_BA,
  DOMAIN_GOOGLE_BE,
  DOMAIN_GOOGLE_BF,
  DOMAIN_GOOGLE_BG,
  DOMAIN_GOOGLE_BI,
  DOMAIN_GOOGLE_BJ,
  DOMAIN_GOOGLE_BS,
  DOMAIN_GOOGLE_BY,
  DOMAIN_GOOGLE_CA,
  DOMAIN_GOOGLE_CAT,
  DOMAIN_GOOGLE_CC,
  DOMAIN_GOOGLE_CD,
  DOMAIN_GOOGLE_CF,
  DOMAIN_GOOGLE_CG,
  DOMAIN_GOOGLE_CH,
  DOMAIN_GOOGLE_CI,
  DOMAIN_GOOGLE_CL,
  DOMAIN_GOOGLE_CM,
  DOMAIN_GOOGLE_CN,
  DOMAIN_CO_AO,
  DOMAIN_CO_BW,
  DOMAIN_CO_CK,
  DOMAIN_CO_CR,
  DOMAIN_CO_HU,
  DOMAIN_CO_ID,
  DOMAIN_CO_IL,
  DOMAIN_CO_IM,
  DOMAIN_CO_IN,
  DOMAIN_CO_JE,
  DOMAIN_CO_JP,
  DOMAIN_CO_KE,
  DOMAIN_CO_KR,
  DOMAIN_CO_LS,
  DOMAIN_CO_MA,
  DOMAIN_CO_MZ,
  DOMAIN_CO_NZ,
  DOMAIN_CO_TH,
  DOMAIN_CO_TZ,
  DOMAIN_CO_UG,
  DOMAIN_CO_UK,
  DOMAIN_CO_UZ,
  DOMAIN_CO_VE,
  DOMAIN_CO_VI,
  DOMAIN_CO_ZA,
  DOMAIN_CO_ZM,
  DOMAIN_CO_ZW,
  DOMAIN_COM_AF,
  DOMAIN_COM_AG,
  DOMAIN_COM_AI,
  DOMAIN_COM_AR,
  DOMAIN_COM_AU,
  DOMAIN_COM_BD,
  DOMAIN_COM_BH,
  DOMAIN_COM_BN,
  DOMAIN_COM_BO,
  DOMAIN_COM_BR,
  DOMAIN_COM_BY,
  DOMAIN_COM_BZ,
  DOMAIN_COM_CN,
  DOMAIN_COM_CO,
  DOMAIN_COM_CU,
  DOMAIN_COM_CY,
  DOMAIN_COM_DO,
  DOMAIN_COM_EC,
  DOMAIN_COM_EG,
  DOMAIN_COM_ET,
  DOMAIN_COM_FJ,
  DOMAIN_COM_GE,
  DOMAIN_COM_GH,
  DOMAIN_COM_GI,
  DOMAIN_COM_GR,
  DOMAIN_COM_GT,
  DOMAIN_COM_HK,
  DOMAIN_COM_IQ,
  DOMAIN_COM_JM,
  DOMAIN_COM_JO,
  DOMAIN_COM_KH,
  DOMAIN_COM_KW,
  DOMAIN_COM_LB,
  DOMAIN_COM_LY,
  DOMAIN_COM_MT,
  DOMAIN_COM_MX,
  DOMAIN_COM_MY,
  DOMAIN_COM_NA,
  DOMAIN_COM_NF,
  DOMAIN_COM_NG,
  DOMAIN_COM_NI,
  DOMAIN_COM_NP,
  DOMAIN_COM_NR,
  DOMAIN_COM_OM,
  DOMAIN_COM_PA,
  DOMAIN_COM_PE,
  DOMAIN_COM_PH,
  DOMAIN_COM_PK,
  DOMAIN_COM_PL,
  DOMAIN_COM_PR,
  DOMAIN_COM_PY,
  DOMAIN_COM_QA,
  DOMAIN_COM_RU,
  DOMAIN_COM_SA,
  DOMAIN_COM_SB,
  DOMAIN_COM_SG,
  DOMAIN_COM_SL,
  DOMAIN_COM_SV,
  DOMAIN_COM_TJ,
  DOMAIN_COM_TN,
  DOMAIN_COM_TR,
  DOMAIN_COM_TW,
  DOMAIN_COM_UA,
  DOMAIN_COM_UY,
  DOMAIN_COM_VC,
  DOMAIN_COM_VE,
  DOMAIN_COM_VN,
  DOMAIN_GOOGLE_CV,
  DOMAIN_GOOGLE_CZ,
  DOMAIN_GOOGLE_DE,
  DOMAIN_GOOGLE_DJ,
  DOMAIN_GOOGLE_DK,
  DOMAIN_GOOGLE_DM,
  DOMAIN_GOOGLE_DZ,
  DOMAIN_GOOGLE_EE,
  DOMAIN_GOOGLE_ES,
  DOMAIN_GOOGLE_FI,
  DOMAIN_GOOGLE_FM,
  DOMAIN_GOOGLE_FR,
  DOMAIN_GOOGLE_GA,
  DOMAIN_GOOGLE_GE,
  DOMAIN_GOOGLE_GG,
  DOMAIN_GOOGLE_GL,
  DOMAIN_GOOGLE_GM,
  DOMAIN_GOOGLE_GP,
  DOMAIN_GOOGLE_GR,
  DOMAIN_GOOGLE_GY,
  DOMAIN_GOOGLE_HK,
  DOMAIN_GOOGLE_HN,
  DOMAIN_GOOGLE_HR,
  DOMAIN_GOOGLE_HT,
  DOMAIN_GOOGLE_HU,
  DOMAIN_GOOGLE_IE,
  DOMAIN_GOOGLE_IM,
  DOMAIN_GOOGLE_INFO,
  DOMAIN_GOOGLE_IQ,
  DOMAIN_GOOGLE_IS,
  DOMAIN_GOOGLE_IT,
  DOMAIN_IT_AO,
  DOMAIN_GOOGLE_JE,
  DOMAIN_GOOGLE_JO,
  DOMAIN_GOOGLE_JOBS,
  DOMAIN_GOOGLE_JP,
  DOMAIN_GOOGLE_KG,
  DOMAIN_GOOGLE_KI,
  DOMAIN_GOOGLE_KZ,
  DOMAIN_GOOGLE_LA,
  DOMAIN_GOOGLE_LI,
  DOMAIN_GOOGLE_LK,
  DOMAIN_GOOGLE_LT,
  DOMAIN_GOOGLE_LU,
  DOMAIN_GOOGLE_LV,
  DOMAIN_GOOGLE_MD,
  DOMAIN_GOOGLE_ME,
  DOMAIN_GOOGLE_MG,
  DOMAIN_GOOGLE_MK,
  DOMAIN_GOOGLE_ML,
  DOMAIN_GOOGLE_MN,
  DOMAIN_GOOGLE_MS,
  DOMAIN_GOOGLE_MU,
  DOMAIN_GOOGLE_MV,
  DOMAIN_GOOGLE_MW,
  DOMAIN_GOOGLE_NE,
  DOMAIN_NE_JP,
  DOMAIN_GOOGLE_NET,
  DOMAIN_GOOGLE_NL,
  DOMAIN_GOOGLE_NO,
  DOMAIN_GOOGLE_NR,
  DOMAIN_GOOGLE_NU,
  DOMAIN_OFF_AI,
  DOMAIN_GOOGLE_PK,
  DOMAIN_GOOGLE_PL,
  DOMAIN_GOOGLE_PN,
  DOMAIN_GOOGLE_PS,
  DOMAIN_GOOGLE_PT,
  DOMAIN_GOOGLE_RO,
  DOMAIN_GOOGLE_RS,
  DOMAIN_GOOGLE_RU,
  DOMAIN_GOOGLE_RW,
  DOMAIN_GOOGLE_SC,
  DOMAIN_GOOGLE_SE,
  DOMAIN_GOOGLE_SH,
  DOMAIN_GOOGLE_SI,
  DOMAIN_GOOGLE_SK,
  DOMAIN_GOOGLE_SM,
  DOMAIN_GOOGLE_SN,
  DOMAIN_GOOGLE_SO,
  DOMAIN_GOOGLE_ST,
  DOMAIN_GOOGLE_TD,
  DOMAIN_GOOGLE_TG,
  DOMAIN_GOOGLE_TK,
  DOMAIN_GOOGLE_TL,
  DOMAIN_GOOGLE_TM,
  DOMAIN_GOOGLE_TN,
  DOMAIN_GOOGLE_TO,
  DOMAIN_GOOGLE_TP,
  DOMAIN_GOOGLE_TT,
  DOMAIN_GOOGLE_US,
  DOMAIN_GOOGLE_UZ,
  DOMAIN_GOOGLE_VG,
  DOMAIN_GOOGLE_VU,
  DOMAIN_GOOGLE_WS,

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
  char dns_name[34];
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

  if (!entry) {
    // We don't care to report pin failures for dynamic pins.
    return;
  }

  DCHECK(entry);
  DCHECK(entry->pins.required_hashes);
  DCHECK(entry->second_level_domain_name != DOMAIN_NOT_PINNED);

  UMA_HISTOGRAM_ENUMERATION("Net.PublicKeyPinFailureDomain",
                            entry->second_level_domain_name, DOMAIN_NUM_EVENTS);
}

// static
const char* TransportSecurityState::HashValueLabel(
    const HashValue& hash_value) {
  switch (hash_value.tag) {
    case HASH_VALUE_SHA1:
      return "sha1/";
    case HASH_VALUE_SHA256:
      return "sha256/";
    default:
      NOTREACHED();
      LOG(WARNING) << "Invalid fingerprint of unknown type " << hash_value.tag;
      return "unknown/";
  }
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
    const HashValueVector& hashes) {
  std::vector<std::string> hashes_strs;
  for (HashValueVector::const_iterator
       i = hashes.begin(); i != hashes.end(); i++) {
    std::string s;
    const std::string hash_str(reinterpret_cast<const char*>(i->data()),
                               i->size());
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
    const HashValueVector& hashes) const {
  // Validate that hashes is not empty. By the time this code is called (in
  // production), that should never happen, but it's good to be defensive.
  // And, hashes *can* be empty in some test scenarios.
  if (hashes.empty()) {
    LOG(ERROR) << "Rejecting empty public key chain for public-key-pinned "
                  "domain " << domain;
    return false;
  }

  if (HashesIntersect(bad_static_spki_hashes, hashes)) {
    LOG(ERROR) << "Rejecting public key chain for domain " << domain
               << ". Validated chain: " << HashesToBase64String(hashes)
               << ", matches one or more bad hashes: "
               << HashesToBase64String(bad_static_spki_hashes);
    return false;
  }

  // If there are no pins, then any valid chain is acceptable.
  if (dynamic_spki_hashes.empty() && static_spki_hashes.empty())
    return true;

  if (HashesIntersect(dynamic_spki_hashes, hashes) ||
      HashesIntersect(static_spki_hashes, hashes)) {
    return true;
  }

  LOG(ERROR) << "Rejecting public key chain for domain " << domain
             << ". Validated chain: " << HashesToBase64String(hashes)
             << ", expected: " << HashesToBase64String(dynamic_spki_hashes)
             << " or: " << HashesToBase64String(static_spki_hashes);
  return false;
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
