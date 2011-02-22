// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/transport_security_state.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/sha2.h"
#include "base/string_number_conversions.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"
#include "net/base/dns_util.h"

namespace net {

const long int TransportSecurityState::kMaxHSTSAgeSecs = 86400 * 365;  // 1 year

TransportSecurityState::TransportSecurityState()
    : delegate_(NULL) {
}

void TransportSecurityState::EnableHost(const std::string& host,
                                        const DomainState& state) {
  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return;

  bool temp;
  if (IsPreloadedSTS(canonicalized_host, &temp))
    return;

  char hashed[base::SHA256_LENGTH];
  base::SHA256HashString(canonicalized_host, hashed, sizeof(hashed));

  // Use the original creation date if we already have this host.
  DomainState state_copy(state);
  DomainState existing_state;
  if (IsEnabledForHost(&existing_state, host))
    state_copy.created = existing_state.created;

  // We don't store these values.
  state_copy.preloaded = false;
  state_copy.domain.clear();

  enabled_hosts_[std::string(hashed, sizeof(hashed))] = state_copy;
  DirtyNotify();
}

bool TransportSecurityState::DeleteHost(const std::string& host) {
  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return false;

  char hashed[base::SHA256_LENGTH];
  base::SHA256HashString(canonicalized_host, hashed, sizeof(hashed));

  std::map<std::string, DomainState>::iterator i = enabled_hosts_.find(
      std::string(hashed, sizeof(hashed)));
  if (i != enabled_hosts_.end()) {
    enabled_hosts_.erase(i);
    DirtyNotify();
    return true;
  }
  return false;
}

// IncludeNUL converts a char* to a std::string and includes the terminating
// NUL in the result.
static std::string IncludeNUL(const char* in) {
  return std::string(in, strlen(in) + 1);
}

bool TransportSecurityState::IsEnabledForHost(DomainState* result,
                                              const std::string& host) {
  *result = DomainState();

  const std::string canonicalized_host = CanonicalizeHost(host);
  if (canonicalized_host.empty())
    return false;

  bool include_subdomains;
  if (IsPreloadedSTS(canonicalized_host, &include_subdomains)) {
    result->created = result->expiry = base::Time::FromTimeT(0);
    result->mode = DomainState::MODE_STRICT;
    result->include_subdomains = include_subdomains;
    result->preloaded = true;
    return true;
  }

  result->preloaded = false;
  base::Time current_time(base::Time::Now());

  for (size_t i = 0; canonicalized_host[i]; i += canonicalized_host[i] + 1) {
    char hashed_domain[base::SHA256_LENGTH];

    base::SHA256HashString(IncludeNUL(&canonicalized_host[i]), &hashed_domain,
                           sizeof(hashed_domain));
    std::map<std::string, DomainState>::iterator j =
        enabled_hosts_.find(std::string(hashed_domain, sizeof(hashed_domain)));
    if (j == enabled_hosts_.end())
      continue;

    if (current_time > j->second.expiry) {
      enabled_hosts_.erase(j);
      DirtyNotify();
      continue;
    }

    *result = j->second;
    result->domain = DNSDomainToString(
        canonicalized_host.substr(i, canonicalized_host.size() - i));

    // If we matched the domain exactly, it doesn't matter what the value of
    // include_subdomains is.
    if (i == 0)
      return true;

    return j->second.include_subdomains;
  }

  return false;
}

void TransportSecurityState::DeleteSince(const base::Time& time) {
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
        DCHECK(tokenizer.token().length() ==  1);
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

void TransportSecurityState::SetDelegate(
    TransportSecurityState::Delegate* delegate) {
  delegate_ = delegate;
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
      out.size() != base::SHA256_LENGTH) {
    return std::string();
  }

  return out;
}

bool TransportSecurityState::Serialise(std::string* output) {
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
      case DomainState::MODE_OPPORTUNISTIC:
        state->SetString("mode", "opportunistic");
        break;
      case DomainState::MODE_SPDY_ONLY:
        state->SetString("mode", "spdy-only");
        break;
      default:
        NOTREACHED() << "DomainState with unknown mode";
        delete state;
        continue;
    }

    toplevel.Set(HashedDomainToExternalString(i->first), state);
  }

  base::JSONWriter::Write(&toplevel, true /* pretty print */, output);
  return true;
}

bool TransportSecurityState::Deserialise(const std::string& input,
                                         bool* dirty) {
  enabled_hosts_.clear();

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

    DomainState::Mode mode;
    if (mode_string == "strict") {
      mode = DomainState::MODE_STRICT;
    } else if (mode_string == "opportunistic") {
      mode = DomainState::MODE_OPPORTUNISTIC;
    } else if (mode_string == "spdy-only") {
      mode = DomainState::MODE_SPDY_ONLY;
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
    if (hashed.empty())
      continue;

    DomainState new_state;
    new_state.mode = mode;
    new_state.created = created_time;
    new_state.expiry = expiry_time;
    new_state.include_subdomains = include_subdomains;
    enabled_hosts_[hashed] = new_state;
  }

  *dirty = dirtied;
  return true;
}

TransportSecurityState::~TransportSecurityState() {
}

void TransportSecurityState::DirtyNotify() {
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

// IsPreloadedSTS returns true if the canonicalized hostname should always be
// considered to have STS enabled.
// static
bool TransportSecurityState::IsPreloadedSTS(
    const std::string& canonicalized_host, bool *include_subdomains) {
  // In the medium term this list is likely to just be hardcoded here. This,
  // slightly odd, form removes the need for additional relocations records.
  static const struct {
    uint8 length;
    bool include_subdomains;
    char dns_name[30];
  } kPreloadedSTS[] = {
    {16, false, "\003www\006paypal\003com"},
    {16, false, "\003www\006elanex\003biz"},
    {12, true,  "\006jottit\003com"},
    {19, true,  "\015sunshinepress\003org"},
    {21, false, "\003www\013noisebridge\003net"},
    {10, false, "\004neg9\003org"},
    {12, true, "\006riseup\003net"},
    {11, false, "\006factor\002cc"},
    {22, false, "\007members\010mayfirst\003org"},
    {22, false, "\007support\010mayfirst\003org"},
    {17, false, "\002id\010mayfirst\003org"},
    {20, false, "\005lists\010mayfirst\003org"},
    {19, true, "\015splendidbacon\003com"},
    {19, true, "\006health\006google\003com"},
    {21, true, "\010checkout\006google\003com"},
    {19, true, "\006chrome\006google\003com"},
    {28, false, "\016aladdinschools\007appspot\003com"},
  };
  static const size_t kNumPreloadedSTS = ARRAYSIZE_UNSAFE(kPreloadedSTS);

  for (size_t i = 0; canonicalized_host[i]; i += canonicalized_host[i] + 1) {
    for (size_t j = 0; j < kNumPreloadedSTS; j++) {
      if (kPreloadedSTS[j].length == canonicalized_host.size() - i &&
          (kPreloadedSTS[j].include_subdomains || i == 0) &&
          memcmp(kPreloadedSTS[j].dns_name, &canonicalized_host[i],
                 kPreloadedSTS[j].length) == 0) {
        *include_subdomains = kPreloadedSTS[j].include_subdomains;
        return true;
      }
    }
  }

  return false;
}

}  // namespace
