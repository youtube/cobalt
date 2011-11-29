// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/transport_security_state.h"

#if defined(USE_OPENSSL)
#include <openssl/ecdsa.h>
#include <openssl/ssl.h>
#else  // !defined(USE_OPENSSL)
#include <nspr.h>

#include <cryptohi.h>
#include <hasht.h>
#include <keyhi.h>
#include <pk11pub.h>
#endif

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/sha1.h"
#include "base/string_number_conversions.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "crypto/sha2.h"
#include "googleurl/src/gurl.h"
#include "net/base/dns_util.h"
#include "net/base/public_key_hashes.h"

#if defined(USE_OPENSSL)
#include "crypto/openssl_util.h"
#endif

namespace net {

const long int TransportSecurityState::kMaxHSTSAgeSecs = 86400 * 365;  // 1 year

TransportSecurityState::TransportSecurityState(const std::string& hsts_hosts)
    : delegate_(NULL) {
  if (!hsts_hosts.empty()) {
    bool dirty;
    Deserialise(hsts_hosts, &dirty, &forced_hosts_);
  }
}

static std::string HashHost(const std::string& canonicalized_host) {
  char hashed[crypto::kSHA256Length];
  crypto::SHA256HashString(canonicalized_host, hashed, sizeof(hashed));
  return std::string(hashed, sizeof(hashed));
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

  // TODO(cevans) -- we likely want to permit a host to override a built-in,
  // for at least the case where the override is stricter (i.e. includes
  // subdomains, or includes certificate pinning).
  DomainState out;
  if (IsPreloadedSTS(canonicalized_host, true, &out) &&
      canonicalized_host == CanonicalizeHost(out.domain)) {
    return;
  }

  // Use the original creation date if we already have this host.
  DomainState state_copy(state);
  DomainState existing_state;
  if (GetDomainState(&existing_state, host, true) &&
      !existing_state.created.is_null()) {
    state_copy.created = existing_state.created;
  }

  // We don't store these values.
  state_copy.preloaded = false;
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

bool TransportSecurityState::HasPinsForHost(DomainState* result,
                                            const std::string& host,
                                            bool sni_available) {
  DCHECK(CalledOnValidThread());

  return HasMetadata(result, host, sni_available) &&
         !result->public_key_hashes.empty();
}

bool TransportSecurityState::GetDomainState(DomainState* result,
                                            const std::string& host,
                                            bool sni_available) {
  DCHECK(CalledOnValidThread());

  return HasMetadata(result, host, sni_available);
}

bool TransportSecurityState::HasMetadata(DomainState* result,
                                         const std::string& host,
                                         bool sni_available) {
  DCHECK(CalledOnValidThread());

  *result = DomainState();

  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return false;

  bool has_preload = IsPreloadedSTS(canonicalized_host, sni_available, result);
  std::string canonicalized_preload = CanonicalizeHost(result->domain);

  base::Time current_time(base::Time::Now());

  for (size_t i = 0; canonicalized_host[i]; i += canonicalized_host[i] + 1) {
    std::string host_sub_chunk(&canonicalized_host[i],
                               canonicalized_host.size() - i);
    // Exact match of a preload always wins.
    if (has_preload && host_sub_chunk == canonicalized_preload)
      return true;

    std::map<std::string, DomainState>::iterator j =
        enabled_hosts_.find(HashHost(host_sub_chunk));
    if (j == enabled_hosts_.end())
      continue;

    if (current_time > j->second.expiry) {
      enabled_hosts_.erase(j);
      DirtyNotify();
      continue;
    }

    *result = j->second;
    result->domain = DNSDomainToString(host_sub_chunk);

    // If we matched the domain exactly, it doesn't matter what the value of
    // include_subdomains is.
    if (i == 0)
      return true;

    return j->second.include_subdomains;
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

// "Strict-Transport-Security" ":"
//     "max-age" "=" delta-seconds [ ";" "includeSubDomains" ]
//
// static
bool TransportSecurityState::ParseHeader(const std::string& value,
                                         int* max_age,
                                         bool* include_subdomains) {
  DCHECK(max_age);
  DCHECK(include_subdomains);

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
      *max_age = max_age_candidate;
      *include_subdomains = false;
      return true;
    case AFTER_MAX_AGE_INCLUDE_SUB_DOMAINS_DELIMITER:
      return false;
    case AFTER_INCLUDE_SUBDOMAINS:
      *max_age = max_age_candidate;
      *include_subdomains = true;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

// Side pinning and superfluous certificates:
//
// In SSLClientSocketNSS::DoVerifyCertComplete we look for certificates with a
// Subject of CN=meta. When we find one we'll currently try and parse side
// pinned key from it.
//
// A side pin is a key which can be pinned to, but also can be kept offline and
// still held by the site owner. The CN=meta certificate is just a backwards
// compatiable method of carrying a lump of bytes to the client. (We could use
// a TLS extension just as well, but it's a lot easier for admins to add extra
// certificates to the chain.)

// A TagMap represents the simple key-value structure that we use. Keys are
// 32-bit ints. Values are byte strings.
typedef std::map<uint32, base::StringPiece> TagMap;

// ParseTags parses a list of key-value pairs from |in| to |out| and advances
// |in| past the data. The key-value pair data is:
//   u16le num_tags
//   u32le tag[num_tags]
//   u16le lengths[num_tags]
//   ...data...
static bool ParseTags(base::StringPiece* in, TagMap *out) {
  // Many part of Chrome already assume little-endian. This is just to help
  // anyone who should try to port it in the future.
#if defined(__BYTE_ORDER)
  // Linux check
  COMPILE_ASSERT(__BYTE_ORDER == __LITTLE_ENDIAN, assumes_little_endian);
#elif defined(__BIG_ENDIAN__)
  // Mac check
  #error assumes little endian
#endif

  if (in->size() < sizeof(uint16))
    return false;

  uint16 num_tags_16;
  memcpy(&num_tags_16, in->data(), sizeof(uint16));
  in->remove_prefix(sizeof(uint16));
  unsigned num_tags = num_tags_16;

  if (in->size() < 6 * num_tags)
    return false;

  const uint32* tags = reinterpret_cast<const uint32*>(in->data());
  const uint16* lens = reinterpret_cast<const uint16*>(
      in->data() + 4*num_tags);
  in->remove_prefix(6*num_tags);

  uint32 prev_tag = 0;
  for (unsigned i = 0; i < num_tags; i++) {
    size_t len = lens[i];
    uint32 tag = tags[i];

    if (in->size() < len)
      return false;
    // tags must be in ascending order.
    if (i > 0 && prev_tag >= tag)
      return false;
    (*out)[tag] = base::StringPiece(in->data(), len);
    in->remove_prefix(len);
    prev_tag = tag;
  }

  return true;
}

// GetTag extracts the data associated with |tag| in |tags|.
static bool GetTag(uint32 tag, const TagMap& tags, base::StringPiece* out) {
  TagMap::const_iterator i = tags.find(tag);
  if (i == tags.end())
    return false;

  *out = i->second;
  return true;
}

// kP256SubjectPublicKeyInfoPrefix can be prepended onto a P256 elliptic curve
// point in X9.62 format in order to make a valid SubjectPublicKeyInfo. The
// ASN.1 interpretation of these bytes is:
//
//     0:d=0  hl=2 l=  89 cons: SEQUENCE
//     2:d=1  hl=2 l=  19 cons: SEQUENCE
//     4:d=2  hl=2 l=   7 prim: OBJECT            :id-ecPublicKey
//    13:d=2  hl=2 l=   8 prim: OBJECT            :prime256v1
//    23:d=1  hl=2 l=  66 prim: BIT STRING
static const uint8 kP256SubjectPublicKeyInfoPrefix[] = {
  0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86,
  0x48, 0xce, 0x3d, 0x02, 0x01, 0x06, 0x08, 0x2a,
  0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03,
  0x42, 0x00,
};

// VerifySignature returns true iff |sig| is a valid signature of
// |hash| by |pubkey|. The actual implementation is crypto library
// specific.
static bool VerifySignature(const base::StringPiece& pubkey,
                            const base::StringPiece& sig,
                            const base::StringPiece& hash);

#if defined(USE_OPENSSL)

static EVP_PKEY* DecodeX962P256PublicKey(
    const base::StringPiece& pubkey_bytes) {
  // The public key is an X9.62 encoded P256 point.
  if (pubkey_bytes.size() != 1 + 2*32)
    return NULL;

  std::string pubkey_spki(
      reinterpret_cast<const char*>(kP256SubjectPublicKeyInfoPrefix),
      sizeof(kP256SubjectPublicKeyInfoPrefix));
  pubkey_spki += pubkey_bytes.as_string();

  EVP_PKEY* ret = NULL;
  const unsigned char* der_pubkey =
      reinterpret_cast<const unsigned char*>(pubkey_spki.data());
  d2i_PUBKEY(&ret, &der_pubkey, pubkey_spki.size());
  return ret;
}

static bool VerifySignature(const base::StringPiece& pubkey,
                            const base::StringPiece& sig,
                            const base::StringPiece& hash) {
  crypto::ScopedOpenSSL<EVP_PKEY, EVP_PKEY_free> secpubkey(
      DecodeX962P256PublicKey(pubkey));
  if (!secpubkey.get())
    return false;


  crypto::ScopedOpenSSL<EC_KEY, EC_KEY_free> ec_key(
      EVP_PKEY_get1_EC_KEY(secpubkey.get()));
  if (!ec_key.get())
    return false;

  return ECDSA_verify(0, reinterpret_cast<const unsigned char*>(hash.data()),
                      hash.size(),
                      reinterpret_cast<const unsigned char*>(sig.data()),
                      sig.size(), ec_key.get()) == 1;
}

#else

// DecodeX962P256PublicKey parses an uncompressed, X9.62 format, P256 elliptic
// curve point from |pubkey_bytes| and returns it as a SECKEYPublicKey.
static SECKEYPublicKey* DecodeX962P256PublicKey(
    const base::StringPiece& pubkey_bytes) {
  // The public key is an X9.62 encoded P256 point.
  if (pubkey_bytes.size() != 1 + 2*32)
    return NULL;

  std::string pubkey_spki(
      reinterpret_cast<const char*>(kP256SubjectPublicKeyInfoPrefix),
      sizeof(kP256SubjectPublicKeyInfoPrefix));
  pubkey_spki += pubkey_bytes.as_string();

  SECItem der;
  memset(&der, 0, sizeof(der));
  der.data = reinterpret_cast<uint8*>(const_cast<char*>(pubkey_spki.data()));
  der.len = pubkey_spki.size();

  CERTSubjectPublicKeyInfo* spki = SECKEY_DecodeDERSubjectPublicKeyInfo(&der);
  if (!spki)
    return NULL;
  SECKEYPublicKey* public_key = SECKEY_ExtractPublicKey(spki);
  SECKEY_DestroySubjectPublicKeyInfo(spki);

  return public_key;
}

static bool VerifySignature(const base::StringPiece& pubkey,
                            const base::StringPiece& sig,
                            const base::StringPiece& hash) {
  SECKEYPublicKey* secpubkey = DecodeX962P256PublicKey(pubkey);
  if (!secpubkey)
    return false;

  SECItem sigitem;
  memset(&sigitem, 0, sizeof(sigitem));
  sigitem.data = reinterpret_cast<uint8*>(const_cast<char*>(sig.data()));
  sigitem.len = sig.size();

  // |decoded_sigitem| is newly allocated, as is the data that it points to.
  SECItem* decoded_sigitem = DSAU_DecodeDerSigToLen(
      &sigitem, SECKEY_SignatureLen(secpubkey));

  if (!decoded_sigitem) {
    SECKEY_DestroyPublicKey(secpubkey);
    return false;
  }

  SECItem hashitem;
  memset(&hashitem, 0, sizeof(hashitem));
  hashitem.data = reinterpret_cast<unsigned char*>(
      const_cast<char*>(hash.data()));
  hashitem.len = hash.size();

  SECStatus rv = PK11_Verify(secpubkey, decoded_sigitem, &hashitem, NULL);
  SECKEY_DestroyPublicKey(secpubkey);
  SECITEM_FreeItem(decoded_sigitem, PR_TRUE);
  return rv == SECSuccess;
}

#endif  // !defined(USE_OPENSSL)

// These are the tag values that we use. Tags are little-endian on the wire and
// these values correspond to the ASCII of the name.
static const uint32 kTagALGO = 0x4f474c41;
static const uint32 kTagP256 = 0x36353250;
static const uint32 kTagPUBK = 0x4b425550;
static const uint32 kTagSIG = 0x474953;
static const uint32 kTagSPIN = 0x4e495053;

// static
bool TransportSecurityState::ParseSidePin(
    const base::StringPiece& leaf_spki,
    const base::StringPiece& in_side_info,
    std::vector<SHA1Fingerprint> *out_pub_key_hash) {
  base::StringPiece side_info(in_side_info);

  TagMap outer;
  if (!ParseTags(&side_info, &outer))
    return false;
  // trailing data is not allowed
  if (side_info.size())
    return false;

  base::StringPiece side_pin_bytes;
  if (!GetTag(kTagSPIN, outer, &side_pin_bytes))
    return false;

  bool have_parsed_a_key = false;
  uint8 leaf_spki_hash[crypto::kSHA256Length];
  bool have_leaf_spki_hash = false;

  while (side_pin_bytes.size() > 0) {
    TagMap side_pin;
    if (!ParseTags(&side_pin_bytes, &side_pin))
      return false;

    base::StringPiece algo, pubkey, sig;
    if (!GetTag(kTagALGO, side_pin, &algo) ||
        !GetTag(kTagPUBK, side_pin, &pubkey) ||
        !GetTag(kTagSIG, side_pin, &sig)) {
      return false;
    }

    if (algo.size() != sizeof(kTagP256) ||
        0 != memcmp(algo.data(), &kTagP256, sizeof(kTagP256))) {
      // We don't support anything but P256 at the moment.
      continue;
    }

    if (!have_leaf_spki_hash) {
      crypto::SHA256HashString(
          leaf_spki.as_string(), leaf_spki_hash, sizeof(leaf_spki_hash));
      have_leaf_spki_hash = true;
    }

    if (VerifySignature(pubkey, sig, base::StringPiece(
        reinterpret_cast<const char*>(leaf_spki_hash),
        sizeof(leaf_spki_hash)))) {
      SHA1Fingerprint fpr;
      base::SHA1HashBytes(
          reinterpret_cast<const uint8*>(pubkey.data()),
          pubkey.size(),
          fpr.data);
      out_pub_key_hash->push_back(fpr);
      have_parsed_a_key = true;
    }
  }

  return have_parsed_a_key;
}

// This function converts the binary hashes, which we store in
// |enabled_hosts_|, to a base64 string which we can include in a JSON file.
static std::string HashedDomainToExternalString(const std::string& hashed) {
  std::string out;
  CHECK(base::Base64Encode(hashed, &out));
  return out;
}

// This inverts |HashedDomainToExternalString|, above. It turns an external
// string (from a JSON file) into an internal (binary) string.
static std::string ExternalStringToHashedDomain(const std::string& external) {
  std::string out;
  if (!base::Base64Decode(external, &out) ||
      out.size() != crypto::kSHA256Length) {
    return std::string();
  }

  return out;
}

bool TransportSecurityState::Serialise(std::string* output) {
  DCHECK(CalledOnValidThread());

  DictionaryValue toplevel;
  for (std::map<std::string, DomainState>::const_iterator
       i = enabled_hosts_.begin(); i != enabled_hosts_.end(); ++i) {
    DictionaryValue* state = new DictionaryValue;
    state->SetBoolean("include_subdomains", i->second.include_subdomains);
    state->SetDouble("created", i->second.created.ToDoubleT());
    state->SetDouble("expiry", i->second.expiry.ToDoubleT());

    switch (i->second.mode) {
      case DomainState::MODE_STRICT:
        state->SetString("mode", "strict");
        break;
      case DomainState::MODE_SPDY_ONLY:
        state->SetString("mode", "spdy-only");
        break;
      default:
        NOTREACHED() << "DomainState with unknown mode";
        delete state;
        continue;
    }

    ListValue* pins = new ListValue;
    for (std::vector<SHA1Fingerprint>::const_iterator
         j = i->second.public_key_hashes.begin();
         j != i->second.public_key_hashes.end(); ++j) {
      std::string hash_str(reinterpret_cast<const char*>(j->data),
                           sizeof(j->data));
      std::string b64;
      base::Base64Encode(hash_str, &b64);
      pins->Append(new StringValue("sha1/" + b64));
    }
    state->Set("public_key_hashes", pins);

    toplevel.Set(HashedDomainToExternalString(i->first), state);
  }

  base::JSONWriter::Write(&toplevel, true /* pretty print */, output);
  return true;
}

bool TransportSecurityState::LoadEntries(const std::string& input,
                                         bool* dirty) {
  DCHECK(CalledOnValidThread());

  enabled_hosts_.clear();
  return Deserialise(input, dirty, &enabled_hosts_);
}

static bool AddHash(const std::string& type_and_base64,
                    std::vector<SHA1Fingerprint>* out) {
  std::string hash_str;
  if (type_and_base64.find("sha1/") == 0 &&
      base::Base64Decode(type_and_base64.substr(5, type_and_base64.size() - 5),
                         &hash_str) &&
      hash_str.size() == base::kSHA1Length) {
    SHA1Fingerprint hash;
    memcpy(hash.data, hash_str.data(), sizeof(hash.data));
    out->push_back(hash);
    return true;
  }
  return false;
}

// static
bool TransportSecurityState::Deserialise(
    const std::string& input,
    bool* dirty,
    std::map<std::string, DomainState>* out) {
  scoped_ptr<Value> value(
      base::JSONReader::Read(input, false /* do not allow trailing commas */));
  if (!value.get() || !value->IsType(Value::TYPE_DICTIONARY))
    return false;

  DictionaryValue* dict_value = reinterpret_cast<DictionaryValue*>(value.get());
  const base::Time current_time(base::Time::Now());
  bool dirtied = false;

  for (DictionaryValue::key_iterator i = dict_value->begin_keys();
       i != dict_value->end_keys(); ++i) {
    DictionaryValue* state;
    if (!dict_value->GetDictionaryWithoutPathExpansion(*i, &state))
      continue;

    bool include_subdomains;
    std::string mode_string;
    double created;
    double expiry;

    if (!state->GetBoolean("include_subdomains", &include_subdomains) ||
        !state->GetString("mode", &mode_string) ||
        !state->GetDouble("expiry", &expiry)) {
      continue;
    }

    ListValue* pins_list = NULL;
    std::vector<SHA1Fingerprint> public_key_hashes;
    if (state->GetList("public_key_hashes", &pins_list)) {
      size_t num_pins = pins_list->GetSize();
      for (size_t i = 0; i < num_pins; ++i) {
        std::string type_and_base64;
        if (pins_list->GetString(i, &type_and_base64))
          AddHash(type_and_base64, &public_key_hashes);
      }
    }

    DomainState::Mode mode;
    if (mode_string == "strict") {
      mode = DomainState::MODE_STRICT;
    } else if (mode_string == "spdy-only") {
      mode = DomainState::MODE_SPDY_ONLY;
    } else if (mode_string == "pinning-only") {
      mode = DomainState::MODE_PINNING_ONLY;
    } else {
      LOG(WARNING) << "Unknown TransportSecurityState mode string found: "
                   << mode_string;
      continue;
    }

    base::Time expiry_time = base::Time::FromDoubleT(expiry);
    base::Time created_time;
    if (state->GetDouble("created", &created)) {
      created_time = base::Time::FromDoubleT(created);
    } else {
      // We're migrating an old entry with no creation date. Make sure we
      // write the new date back in a reasonable time frame.
      dirtied = true;
      created_time = base::Time::Now();
    }

    if (expiry_time <= current_time) {
      // Make sure we dirty the state if we drop an entry.
      dirtied = true;
      continue;
    }

    std::string hashed = ExternalStringToHashedDomain(*i);
    if (hashed.empty()) {
      dirtied = true;
      continue;
    }

    DomainState new_state;
    new_state.mode = mode;
    new_state.created = created_time;
    new_state.expiry = expiry_time;
    new_state.include_subdomains = include_subdomains;
    new_state.public_key_hashes = public_key_hashes;
    (*out)[hashed] = new_state;
  }

  *dirty = dirtied;
  return true;
}

TransportSecurityState::~TransportSecurityState() {
}

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
          out->mode = TransportSecurityState::DomainState::MODE_PINNING_ONLY;
        if (entries[j].pins.required_hashes) {
          const char* const* hash = entries[j].pins.required_hashes;
          while (*hash) {
            bool ok = AddHash(*hash, &out->public_key_hashes);
            DCHECK(ok) << " failed to parse " << *hash;
            hash++;
          }
        }
        if (entries[j].pins.excluded_hashes) {
          const char* const* hash = entries[j].pins.excluded_hashes;
          while (*hash) {
            bool ok = AddHash(*hash, &out->bad_public_key_hashes);
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

// kNoRejectedPublicKeys is a placeholder for when no public keys are rejected.
static const char* const kNoRejectedPublicKeys[] = {
  NULL,
};

static const char* const kGoogleAcceptableCerts[] = {
  kSPKIHash_VeriSignClass3,
  kSPKIHash_VeriSignClass3_G3,
  kSPKIHash_Google1024,
  kSPKIHash_Google2048,
  kSPKIHash_EquifaxSecureCA,
  NULL,
};
static const char* const kGoogleRejectedCerts[] = {
  kSPKIHash_Aetna,
  kSPKIHash_GeoTrustGlobal,
  kSPKIHash_GeoTrustPrimary,
  kSPKIHash_Intel,
  kSPKIHash_TCTrustCenter,
  kSPKIHash_Vodafone,
  NULL,
};
static const PublicKeyPins kGooglePins = {
  kGoogleAcceptableCerts,
  kGoogleRejectedCerts,
};

static const char* const kTorAcceptableCerts[] = {
  kSPKIHash_RapidSSL,
  kSPKIHash_DigiCertEVRoot,
  kSPKIHash_Tor1,
  kSPKIHash_Tor2,
  kSPKIHash_Tor3,
  NULL,
};
static const PublicKeyPins kTorPins = {
  kTorAcceptableCerts,
  kNoRejectedPublicKeys,
};

static const char* const kTwitterComAcceptableCerts[] = {
  kSPKIHash_VeriSignClass1,
  kSPKIHash_VeriSignClass3,
  kSPKIHash_VeriSignClass3_G4,
  kSPKIHash_VeriSignClass4_G3,
  kSPKIHash_VeriSignClass3_G3,
  kSPKIHash_VeriSignClass1_G3,
  kSPKIHash_VeriSignClass2_G3,
  kSPKIHash_VeriSignClass3_G2,
  kSPKIHash_VeriSignClass2_G2,
  kSPKIHash_VeriSignClass3_G5,
  kSPKIHash_VeriSignUniversal,
  kSPKIHash_GeoTrustGlobal,
  kSPKIHash_GeoTrustGlobal2,
  kSPKIHash_GeoTrustUniversal,
  kSPKIHash_GeoTrustUniversal2,
  kSPKIHash_GeoTrustPrimary,
  kSPKIHash_GeoTrustPrimary_G2,
  kSPKIHash_GeoTrustPrimary_G3,
  kSPKIHash_Twitter1,
  NULL,
};
static const PublicKeyPins kTwitterComPins = {
  kTwitterComAcceptableCerts,
  kNoRejectedPublicKeys,
};

// kTestAcceptableCerts doesn't actually match any public keys and is used
// with "pinningtest.appspot.com", below, to test if pinning is active.
static const char* const kTestAcceptableCerts[] = {
  "sha1/AAAAAAAAAAAAAAAAAAAAAAAAAAA=",
  NULL,
};
static const PublicKeyPins kTestPins = {
  kTestAcceptableCerts,
  kNoRejectedPublicKeys,
};

static const PublicKeyPins kNoPins = {
  NULL, NULL,
};

#if defined(OS_CHROMEOS)
  static const bool kTwitterHSTS = true;
#else
  static const bool kTwitterHSTS = false;
#endif

// In the medium term this list is likely to just be hardcoded here. This
// slightly odd form removes the need for additional relocations records.
static const struct HSTSPreload kPreloadedSTS[] = {
  // (*.)google.com, iff using SSL must use an acceptable certificate.
  {12, true, "\006google\003com", false, kGooglePins,
      DOMAIN_GOOGLE_COM },
  {25, true, "\013pinningtest\007appspot\003com", false,
      kTestPins, DOMAIN_APPSPOT_COM },
  // Now we force HTTPS for subtrees of google.com.
  {19, true, "\006health\006google\003com", true, kGooglePins,
      DOMAIN_GOOGLE_COM  },
  {21, true, "\010checkout\006google\003com", true, kGooglePins,
      DOMAIN_GOOGLE_COM },
  {19, true, "\006chrome\006google\003com", true, kGooglePins,
      DOMAIN_GOOGLE_COM },
  {17, true, "\004docs\006google\003com", true, kGooglePins,
      DOMAIN_GOOGLE_COM },
  {18, true, "\005sites\006google\003com", true, kGooglePins,
      DOMAIN_GOOGLE_COM },
  {25, true, "\014spreadsheets\006google\003com", true,
      kGooglePins, DOMAIN_GOOGLE_COM },
  {22, false, "\011appengine\006google\003com", true,
      kGooglePins, DOMAIN_GOOGLE_COM },
  {22, true, "\011encrypted\006google\003com", true, kGooglePins,
      DOMAIN_GOOGLE_COM },
  {21, true, "\010accounts\006google\003com", true, kGooglePins,
      DOMAIN_GOOGLE_COM },
  {21, true, "\010profiles\006google\003com", true, kGooglePins,
      DOMAIN_GOOGLE_COM },
  {17, true, "\004mail\006google\003com", true, kGooglePins,
      DOMAIN_GOOGLE_COM },
  {23, true, "\012talkgadget\006google\003com", true,
      kGooglePins, DOMAIN_GOOGLE_COM },
  {17, true, "\004talk\006google\003com", true, kGooglePins,
      DOMAIN_GOOGLE_COM },
  {29, true, "\020hostedtalkgadget\006google\003com", true,
      kGooglePins, DOMAIN_GOOGLE_COM },
  {17, true, "\004plus\006google\003com", true, kGooglePins,
      DOMAIN_GOOGLE_COM },
  // Other Google-related domains that must use HTTPS.
  {20, true, "\006market\007android\003com", true, kGooglePins,
      DOMAIN_ANDROID_COM },
  {26, true, "\003ssl\020google-analytics\003com", true,
      kGooglePins, DOMAIN_GOOGLE_ANALYTICS_COM },
  {18, true, "\005drive\006google\003com", true, kGooglePins,
      DOMAIN_GOOGLE_COM },
  {16, true, "\012googleplex\003com", true, kGooglePins,
      DOMAIN_GOOGLEPLEX_COM },
  {19, true, "\006groups\006google\003com", true, kGooglePins,
      DOMAIN_GOOGLE_COM },
  // Other Google-related domains that must use an acceptable certificate
  // iff using SSL.
  {11, true, "\005ytimg\003com", false, kGooglePins,
      DOMAIN_YTIMG_COM },
  {23, true, "\021googleusercontent\003com", false, kGooglePins,
      DOMAIN_GOOGLEUSERCONTENT_COM },
  {13, true, "\007youtube\003com", false, kGooglePins,
      DOMAIN_YOUTUBE_COM },
  {16, true, "\012googleapis\003com", false, kGooglePins,
      DOMAIN_GOOGLEAPIS_COM },
  {22, true, "\020googleadservices\003com", false, kGooglePins,
      DOMAIN_GOOGLEADSERVICES_COM },
  {16, true, "\012googlecode\003com", false, kGooglePins,
      DOMAIN_GOOGLECODE_COM },
  {13, true, "\007appspot\003com", false, kGooglePins,
      DOMAIN_APPSPOT_COM },
  {23, true, "\021googlesyndication\003com", false, kGooglePins,
      DOMAIN_GOOGLESYNDICATION_COM },
  {17, true, "\013doubleclick\003net", false, kGooglePins,
      DOMAIN_DOUBLECLICK_NET },
  {17, true, "\003ssl\007gstatic\003com", false, kGooglePins,
      DOMAIN_GSTATIC_COM },
  // Exclude the learn.doubleclick.net subdomain because it uses a different
  // CA.
  {23, true, "\005learn\013doubleclick\003net", false, kNoPins, DOMAIN_NOT_PINNED },
  // Now we force HTTPS for other sites that have requested it.
  {16, false, "\003www\006paypal\003com", true, kNoPins, DOMAIN_NOT_PINNED },
  {16, false, "\003www\006elanex\003biz", true, kNoPins, DOMAIN_NOT_PINNED },
  {12, true,  "\006jottit\003com", true, kNoPins, DOMAIN_NOT_PINNED },
  {19, true,  "\015sunshinepress\003org", true, kNoPins, DOMAIN_NOT_PINNED },
  {21, false, "\003www\013noisebridge\003net", true, kNoPins,
      DOMAIN_NOT_PINNED },
  {10, false, "\004neg9\003org", true, kNoPins, DOMAIN_NOT_PINNED },
  {12, true, "\006riseup\003net", true, kNoPins, DOMAIN_NOT_PINNED },
  {11, false, "\006factor\002cc", true, kNoPins, DOMAIN_NOT_PINNED },
  {22, false, "\007members\010mayfirst\003org", true, kNoPins, DOMAIN_NOT_PINNED },
  {22, false, "\007support\010mayfirst\003org", true, kNoPins, DOMAIN_NOT_PINNED },
  {17, false, "\002id\010mayfirst\003org", true, kNoPins, DOMAIN_NOT_PINNED },
  {20, false, "\005lists\010mayfirst\003org", true, kNoPins, DOMAIN_NOT_PINNED },
  {19, true, "\015splendidbacon\003com", true, kNoPins, DOMAIN_NOT_PINNED },
  {28, false, "\016aladdinschools\007appspot\003com", true, kNoPins,
      DOMAIN_NOT_PINNED },
  {14, true, "\011ottospora\002nl", true, kNoPins, DOMAIN_NOT_PINNED },
  {25, false, "\003www\017paycheckrecords\003com", true, kNoPins,
      DOMAIN_NOT_PINNED },
  {14, false, "\010lastpass\003com", true, kNoPins, DOMAIN_NOT_PINNED },
  {18, false, "\003www\010lastpass\003com", true, kNoPins, DOMAIN_NOT_PINNED },
  {14, true, "\010keyerror\003com", true, kNoPins, DOMAIN_NOT_PINNED },
  {13, false, "\010entropia\002de", true, kNoPins, DOMAIN_NOT_PINNED },
  {17, false, "\003www\010entropia\002de", true, kNoPins, DOMAIN_NOT_PINNED },
  {11, true, "\005romab\003com", true, kNoPins, DOMAIN_NOT_PINNED },
  {16, false, "\012logentries\003com", true, kNoPins, DOMAIN_NOT_PINNED },
  {20, false, "\003www\012logentries\003com", true, kNoPins, DOMAIN_NOT_PINNED },
  {12, true, "\006stripe\003com", true, kNoPins, DOMAIN_NOT_PINNED },
  {27, true, "\025cloudsecurityalliance\003org", true, kNoPins,
      DOMAIN_NOT_PINNED },
  {15, true, "\005login\004sapo\002pt", true, kNoPins, DOMAIN_NOT_PINNED },
  {19, true, "\015mattmccutchen\003net", true, kNoPins, DOMAIN_NOT_PINNED },
  {11, true, "\006betnet\002fr", true, kNoPins, DOMAIN_NOT_PINNED },
  {13, true, "\010uprotect\002it", true, kNoPins, DOMAIN_NOT_PINNED },
  {14, false, "\010squareup\003com", true, kNoPins, DOMAIN_NOT_PINNED },
  {9, true, "\004cert\002se", true, kNoPins, DOMAIN_NOT_PINNED },
  {11, true, "\006crypto\002is", true, kNoPins, DOMAIN_NOT_PINNED },
  {20, true, "\005simon\007butcher\004name", true, kNoPins, DOMAIN_NOT_PINNED },
  {10, true, "\004linx\003net", true, kNoPins, DOMAIN_NOT_PINNED },
  {13, false, "\007dropcam\003com", true, kNoPins, DOMAIN_NOT_PINNED },
  {17, false, "\003www\007dropcam\003com", true, kNoPins, DOMAIN_NOT_PINNED },
  {30, true, "\010ebanking\014indovinabank\003com\002vn", true, kNoPins,
      DOMAIN_NOT_PINNED },
  {13, false, "\007epoxate\003com", true, kNoPins, DOMAIN_NOT_PINNED },
  {16, false, "\012torproject\003org", true, kTorPins,
      DOMAIN_TORPROJECT_ORG },
  {21, true, "\004blog\012torproject\003org", true, kTorPins,
      DOMAIN_TORPROJECT_ORG },
  {22, true, "\005check\012torproject\003org", true, kTorPins,
      DOMAIN_TORPROJECT_ORG },
  {20, true, "\003www\012torproject\003org", true, kTorPins,
      DOMAIN_TORPROJECT_ORG },
  {22, true, "\003www\014moneybookers\003com", true, kNoPins,
      DOMAIN_NOT_PINNED },
  {17, false, "\013ledgerscope\003net", true, kNoPins, DOMAIN_NOT_PINNED },
  {21, false, "\003www\013ledgerscope\003net", true, kNoPins,
      DOMAIN_NOT_PINNED },
  {10, false, "\004kyps\003net", true, kNoPins, DOMAIN_NOT_PINNED },
  {14, false, "\003www\004kyps\003net", true, kNoPins, DOMAIN_NOT_PINNED },
  {17, true, "\003app\007recurly\003com", true, kNoPins, DOMAIN_NOT_PINNED },
  {17, true, "\003api\007recurly\003com", true, kNoPins, DOMAIN_NOT_PINNED },
  {13, false, "\007greplin\003com", true, kNoPins, DOMAIN_NOT_PINNED },
  {17, false, "\003www\007greplin\003com", true, kNoPins, DOMAIN_NOT_PINNED },
  {27, true, "\006luneta\016nearbuysystems\003com", true, kNoPins,
      DOMAIN_NOT_PINNED },
  {12, true, "\006ubertt\003org", true, kNoPins, DOMAIN_NOT_PINNED },

#if 0
  // Twitter pins disabled in order to track down pinning failures --agl
  {13, false, "\007twitter\003com", kTwitterHSTS,
      kTwitterComPins, DOMAIN_TWITTER_COM },
  {17, true, "\003www\007twitter\003com", kTwitterHSTS,
      kTwitterComPins, DOMAIN_TWITTER_COM },
  {17, true, "\003api\007twitter\003com", kTwitterHSTS,
      kTwitterComPins, DOMAIN_TWITTER_COM },
  {19, true, "\005oauth\007twitter\003com", kTwitterHSTS,
      kTwitterComPins, DOMAIN_TWITTER_COM },
  {20, true, "\006mobile\007twitter\003com", kTwitterHSTS,
      kTwitterComPins, DOMAIN_TWITTER_COM },
  {17, true, "\003dev\007twitter\003com", kTwitterHSTS,
      kTwitterComPins, DOMAIN_TWITTER_COM },
  {22, true, "\010business\007twitter\003com", kTwitterHSTS,
      kTwitterComPins, DOMAIN_TWITTER_COM },
  {22, true, "\010platform\007twitter\003com", false,
      kTwitterCDNPins, DOMAIN_TWITTER_COM },
  {15, true, "\003si0\005twimg\003com", false, kTwitterCDNPins,
      DOMAIN_TWIMG_COM },
  {23, true, "\010twimg0-a\010akamaihd\003net", false,
      kTwitterCDNPins, DOMAIN_AKAMAIHD_NET },
#endif
};
static const size_t kNumPreloadedSTS = ARRAYSIZE_UNSAFE(kPreloadedSTS);

static const struct HSTSPreload kPreloadedSNISTS[] = {
  // These SNI-only domains must always use HTTPS.
  {11, false, "\005gmail\003com", true, kGooglePins,
      DOMAIN_GMAIL_COM },
  {16, false, "\012googlemail\003com", true, kGooglePins,
      DOMAIN_GOOGLEMAIL_COM },
  {15, false, "\003www\005gmail\003com", true, kGooglePins,
      DOMAIN_GMAIL_COM },
  {20, false, "\003www\012googlemail\003com", true, kGooglePins,
      DOMAIN_GOOGLEMAIL_COM },
  // These SNI-only domains must use an acceptable certificate iff using
  // HTTPS.
  {22, true, "\020google-analytics\003com", false, kGooglePins,
     DOMAIN_GOOGLE_ANALYTICS_COM },
  // www. requires SNI.
  {18, true, "\014googlegroups\003com", false, kGooglePins,
      DOMAIN_GOOGLEGROUPS_COM },
};
static const size_t kNumPreloadedSNISTS = ARRAYSIZE_UNSAFE(kPreloadedSNISTS);

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
                                                    bool sni_available) {
  std::string canonicalized_host = CanonicalizeHost(host);
  const struct HSTSPreload* entry =
      GetHSTSPreload(canonicalized_host, kPreloadedSTS, kNumPreloadedSTS);

  if (entry && entry->pins.required_hashes == kGoogleAcceptableCerts)
    return true;

  if (sni_available) {
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

// IsPreloadedSTS returns true if the canonicalized hostname should always be
// considered to have STS enabled.
bool TransportSecurityState::IsPreloadedSTS(
    const std::string& canonicalized_host,
    bool sni_available,
    DomainState* out) {
  DCHECK(CalledOnValidThread());

  out->preloaded = true;
  out->mode = DomainState::MODE_STRICT;
  out->include_subdomains = false;

  for (size_t i = 0; canonicalized_host[i]; i += canonicalized_host[i] + 1) {
    std::string host_sub_chunk(&canonicalized_host[i],
                               canonicalized_host.size() - i);
    out->domain = DNSDomainToString(host_sub_chunk);
    std::string hashed_host(HashHost(host_sub_chunk));
    if (forced_hosts_.find(hashed_host) != forced_hosts_.end()) {
      *out = forced_hosts_[hashed_host];
      out->domain = DNSDomainToString(host_sub_chunk);
      out->preloaded = true;
      return true;
    }
    bool ret;
    if (HasPreload(kPreloadedSTS, kNumPreloadedSTS, canonicalized_host, i, out,
                   &ret)) {
      return ret;
    }
    if (sni_available &&
        HasPreload(kPreloadedSNISTS, kNumPreloadedSNISTS, canonicalized_host, i,
                   out, &ret)) {
      return ret;
    }
  }

  return false;
}

static std::string HashesToBase64String(
    const std::vector<net::SHA1Fingerprint>& hashes) {
  std::vector<std::string> hashes_strs;
  for (std::vector<net::SHA1Fingerprint>::const_iterator
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
    : mode(MODE_STRICT),
      created(base::Time::Now()),
      include_subdomains(false),
      preloaded(false) {
}

TransportSecurityState::DomainState::~DomainState() {
}

bool TransportSecurityState::DomainState::IsChainOfPublicKeysPermitted(
    const std::vector<net::SHA1Fingerprint>& hashes) {
  for (std::vector<net::SHA1Fingerprint>::const_iterator
       i = hashes.begin(); i != hashes.end(); ++i) {
    for (std::vector<net::SHA1Fingerprint>::const_iterator
         j = bad_public_key_hashes.begin(); j != bad_public_key_hashes.end();
         ++j) {
      if (i->Equals(*j)) {
        LOG(ERROR) << "Rejecting public key chain for domain " << domain
                   << ". Validated chain: " << HashesToBase64String(hashes)
                   << ", matches one or more bad hashes: "
                   << HashesToBase64String(bad_public_key_hashes);
        return false;
      }
    }
  }

  if (public_key_hashes.empty())
    return true;

  for (std::vector<net::SHA1Fingerprint>::const_iterator
       i = hashes.begin(); i != hashes.end(); ++i) {
    for (std::vector<net::SHA1Fingerprint>::const_iterator
         j = public_key_hashes.begin(); j != public_key_hashes.end(); ++j) {
      if (i->Equals(*j))
        return true;
    }
  }

  LOG(ERROR) << "Rejecting public key chain for domain " << domain
             << ". Validated chain: " << HashesToBase64String(hashes)
             << ", expected: " << HashesToBase64String(public_key_hashes);

  return false;
}

bool TransportSecurityState::DomainState::ShouldCertificateErrorsBeFatal()
    const {
  return true;
}

bool TransportSecurityState::DomainState::ShouldRedirectHTTPToHTTPS()
    const {
  return mode == MODE_STRICT;
}

bool TransportSecurityState::DomainState::ShouldMixedScriptingBeBlocked()
    const {
  return true;
}

}  // namespace
