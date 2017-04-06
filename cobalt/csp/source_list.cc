// Copyright 2015 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/csp/source_list.h"

#include <algorithm>

#include "base/base64.h"
#include "base/string_number_conversions.h"
#include "googleurl/src/url_canon_ip.h"
#include "googleurl/src/url_constants.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"
#include "starboard/socket.h"

namespace cobalt {
namespace csp {

namespace {

// Even though StringPieces do not own the data, pointing to a C literal
// should be OK.
static const base::StringPiece kLocalHostRepresentations[] = {
    base::StringPiece("localhost"), base::StringPiece("localhost.localdomain"),
#if SB_HAS(IPV6)
    base::StringPiece("::1"),
#endif
    base::StringPiece("127.0.0.1")};

bool IsSourceListNone(const char* begin, const char* end) {
  SkipWhile<IsAsciiWhitespace>(&begin, end);

  const char* position = begin;
  SkipWhile<IsSourceCharacter>(&position, end);
  size_t len = static_cast<size_t>(position - begin);
  if (base::strncasecmp("'none'", begin, len) != 0) {
    return false;
  }

  SkipWhile<IsAsciiWhitespace>(&position, end);
  if (position != end) {
    return false;
  }

  return true;
}

struct HashPrefix {
  const char* prefix;
  HashAlgorithm type;
};

HashPrefix kSupportedPrefixes[] = {
    {"'sha256-", kHashAlgorithmSha256},  {"'sha384-", kHashAlgorithmSha384},
    {"'sha512-", kHashAlgorithmSha512},  {"'sha-256-", kHashAlgorithmSha256},
    {"'sha-384-", kHashAlgorithmSha384}, {"'sha-512-", kHashAlgorithmSha512},
};

bool SchemeCanMatchStar(const GURL& url) {
  return !(url.SchemeIs("blob") || url.SchemeIs("data") ||
           url.SchemeIs("filesystem"));
}

bool GetDestinationForHost(const char* spec,
                           const url_parse::Component& host_component,
                           SbSocketAddress* out_destination_address) {
  if (!out_destination_address) {
    NOTREACHED();
    return false;
  }

  COMPILE_ASSERT(net::kIPv6AddressSize >= net::kIPv4AddressSize,
                 bad_ip_address_length);
  unsigned char address[net::kIPv6AddressSize];

  int num_ipv4_components = 0;

  // IPv4AddressToNumber will return either IPV4, BROKEN, or NEUTRAL.  If the
  // input IP address is IPv6, then NEUTRAL will be returned, and a different
  // function will be used to covert the hostname to IPv6 address.
  // If the return value is IPV4, then address will be in network byte order.
  url_canon::CanonHostInfo::Family family = url_canon::IPv4AddressToNumber(
      spec, host_component, address, &num_ipv4_components);

  switch (family) {
    case url_canon::CanonHostInfo::IPV4: {
      if (num_ipv4_components != net::kIPv4AddressSize) {
        break;
      }

      SbMemorySet(out_destination_address, 0, sizeof(SbSocketAddress));
      out_destination_address->type = kSbSocketAddressTypeIpv4;
      DCHECK_GE(sizeof(address), static_cast<std::size_t>(num_ipv4_components));
      SbMemoryCopy(out_destination_address->address, address,
                   num_ipv4_components);

      return true;
    }
    case url_canon::CanonHostInfo::NEUTRAL: {
#if SB_HAS(IPV6)
      if (!url_canon::IPv6AddressToNumber(spec, host_component, address)) {
        break;
      }

      SbMemorySet(out_destination_address, 0, sizeof(SbSocketAddress));
      out_destination_address->type = kSbSocketAddressTypeIpv6;
      COMPILE_ASSERT(sizeof(address), kIPv6AddressLength);
      SbMemoryCopy(out_destination_address->address, address, sizeof(address));
      return true;
#endif
      break;
    }
    case url_canon::CanonHostInfo::BROKEN:
      break;
    case url_canon::CanonHostInfo::IPV6:
      NOTREACHED() << "Invalid return value from IPv4AddressToNumber";
      break;
  }

  return false;
}

bool IsIPInPrivateRange(const SbSocketAddress& ip) {
  if (ip.type == kSbSocketAddressTypeIpv4) {
    if (ip.address[0] == 10) {
      // IP is in range 10.0.0.0 - 10.255.255.255 (10/8 prefix).
      return true;
    }
    if ((ip.address[0] == 192) && (ip.address[1] == 168)) {
      // IP is in range 192.168.0.0 - 192.168.255.255 (192.168/16 prefix).
      return true;
    }
    if ((ip.address[0] == 172) &&
        ((ip.address[1] >= 16) || (ip.address[1] <= 31))) {
      // IP is in range 172.16.0.0 - 172.31.255.255 (172.16/12 prefix).
      return true;
    }
  }
#if SB_HAS(IPV6)
  if (ip.type == kSbSocketAddressTypeIpv6) {
    // Unique Local Addresses for IPv6 are fd00::/8.
    return ip.address[0] == 0xfd && ip.address[1] == 0;
  }
#endif

  return false;
}

bool IsInsecureScheme(const GURL& url) {
  return url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kWsScheme);
}

}  // namespace

SourceList::SourceList(ContentSecurityPolicy* policy,
                       const std::string& directive_name)
    : policy_(policy),
      directive_name_(directive_name),
      allow_self_(false),
      allow_star_(false),
      allow_inline_(false),
      allow_eval_(false),
      allow_insecure_connections_to_local_network_(false),
      allow_insecure_connections_to_localhost_(false),
      allow_insecure_connections_to_private_range_(false),
      hash_algorithms_used_(0),
      local_network_checker_(new LocalNetworkChecker()) {}

bool SourceList::Matches(
    const GURL& url,
    ContentSecurityPolicy::RedirectStatus redirect_status) const {
  if (allow_star_ && SchemeCanMatchStar(url)) {
    return true;
  }

  if (allow_self_ && policy_->UrlMatchesSelf(url)) {
    return true;
  }

  for (size_t i = 0; i < list_.size(); ++i) {
    if (list_[i].Matches(url, redirect_status)) {
      return true;
    }
  }

  if (!url.is_valid() || url.is_empty()) {
    return false;
  }
  // Since url is valid, we can now use possibly_invalid_spec() below.
  const std::string& valid_spec = url.possibly_invalid_spec();

  const url_parse::Parsed& parsed = url.parsed_for_possibly_invalid_spec();

  if (!url.has_host() || !parsed.host.is_valid() ||
      !parsed.host.is_nonempty()) {
    return false;
  }

  const char* valid_spec_cstr = valid_spec.c_str();
  base::StringPiece host(valid_spec_cstr + parsed.host.begin, parsed.host.len);

  if (!IsInsecureScheme(url)) {
    return false;
  }

  if (allow_insecure_connections_to_localhost_) {
    const base::StringPiece* begin = kLocalHostRepresentations;
    const base::StringPiece* end = begin + arraysize(kLocalHostRepresentations);
    if (std::find(begin, end, host) != end) {
      return true;
    }
  }

  /* allow mixed content for local network or private ranges? */
  if (!(allow_insecure_connections_to_local_network_ ||
        allow_insecure_connections_to_private_range_)) {
    return false;
  }

  SbSocketAddress destination;
  // Note that GetDestinationForHost will only pass if host is a numeric IP.
  if (!GetDestinationForHost(valid_spec_cstr, parsed.host, &destination)) {
    return false;
  }

  if (allow_insecure_connections_to_private_range_ &&
      IsIPInPrivateRange(destination)) {
    return true;
  }

  if (allow_insecure_connections_to_local_network_ &&
      local_network_checker_->IsIPInLocalNetwork(destination)) {
    return true;
  }

  return false;
}

bool SourceList::AllowInline() const { return allow_inline_; }

bool SourceList::AllowEval() const { return allow_eval_; }

bool SourceList::AllowNonce(const std::string& nonce) const {
  return !nonce.empty() && nonces_.find(nonce) != nonces_.end();
}

bool SourceList::AllowHash(const HashValue& hash_value) const {
  return hashes_.find(hash_value) != hashes_.end();
}

bool SourceList::hash_or_nonce_present() const {
  return nonces_.size() > 0 || hash_algorithms_used_ != kHashAlgorithmNone;
}

// source-list       = *WSP [ source *( 1*WSP source ) *WSP ]
//                   / *WSP "'none'" *WSP
//
void SourceList::Parse(const base::StringPiece& str) {
  // We represent 'none' as an empty list_.
  const char* begin = str.begin();
  const char* end = str.end();

  if (IsSourceListNone(begin, end)) {
    return;
  }

  const char* position = begin;
  while (position < end) {
    SkipWhile<IsAsciiWhitespace>(&position, end);
    if (position == end) {
      return;
    }

    const char* begin_source = position;
    SkipWhile<IsSourceCharacter>(&position, end);

    SourceConfig config;
    if (ParseSource(begin_source, position, &config)) {
      // Wildcard hosts and keyword sources ('self', 'unsafe-inline',
      // etc.) aren't stored in m_list, but as attributes on the source
      // list itself.
      if (config.scheme.empty() && config.host.empty()) {
        continue;
      }
      if (policy_->IsDirectiveName(config.host)) {
        policy_->ReportDirectiveAsSourceExpression(directive_name_,
                                                   config.host);
      }
      list_.push_back(Source(policy_, config));
    } else {
      policy_->ReportInvalidSourceExpression(directive_name_,
                                             ToString(begin_source, position));
    }

    DCHECK(position == end || IsAsciiWhitespace(*position));
  }
}

// source            = scheme ":"
//                   / ( [ scheme "://" ] host [ port ] [ path ] )
//                   / "'self'"
bool SourceList::ParseSource(const char* begin, const char* end,
                             SourceConfig* config) {
  if (begin == end) {
    return false;
  }

  if (LowerCaseEqualsASCII(begin, end, "'none'")) {
    return false;
  }

  base::StringPiece begin_piece(begin, static_cast<size_t>(end - begin));
  if (begin_piece.length() == 1 && begin_piece[0] == '*') {
    AddSourceStar();
    return true;
  }
  if (LowerCaseEqualsASCII(begin, end, "'self'")) {
    AddSourceSelf();
    return true;
  }

  if (LowerCaseEqualsASCII(begin, end, "'unsafe-inline'")) {
    AddSourceUnsafeInline();
    return true;
  }

  if (LowerCaseEqualsASCII(begin, end, "'unsafe-eval'")) {
    AddSourceUnsafeEval();
    return true;
  }

  if (directive_name_.compare(ContentSecurityPolicy::kConnectSrc) == 0) {
    if (LowerCaseEqualsASCII(begin, end, "'cobalt-insecure-localhost'")) {
      AddSourceLocalhost();
      return true;
    }
    if (LowerCaseEqualsASCII(begin, end, "'cobalt-insecure-local-network'")) {
      AddSourceLocalNetwork();
      return true;
    }
    if (LowerCaseEqualsASCII(begin, end, "'cobalt-insecure-private-range'")) {
      AddSourcePrivateRange();
      return true;
    }
  }

  std::string nonce;
  if (!ParseNonce(begin, end, &nonce)) {
    return false;
  }

  if (!nonce.empty()) {
    AddSourceNonce(nonce);
    return true;
  }

  DigestValue hash;
  HashAlgorithm algorithm = kHashAlgorithmNone;
  if (!ParseHash(begin, end, &hash, &algorithm)) {
    return false;
  }

  if (hash.size() > 0) {
    AddSourceHash(algorithm, hash);
    return true;
  }

  const char* position = begin;
  const char* begin_host = begin;
  const char* begin_path = end;
  const char* begin_port = 0;

  SkipWhile<IsNotColonOrSlash>(&position, end);

  if (position == end) {
    // host
    //     ^
    return ParseHost(begin_host, position, &config->host,
                     &config->host_wildcard);
  }

  if (position < end && *position == '/') {
    // host/path || host/ || /
    //     ^            ^    ^
    return ParseHost(begin_host, position, &config->host,
                     &config->host_wildcard) &&
           ParsePath(position, end, &config->path);
  }

  if (position < end && *position == ':') {
    if (end - position == 1) {
      // scheme:
      //       ^
      return ParseScheme(begin, position, &config->scheme);
    }

    if (position[1] == '/') {
      // scheme://host || scheme://
      //       ^                ^
      if (!ParseScheme(begin, position, &config->scheme) ||
          !SkipExactly(&position, end, ':') ||
          !SkipExactly(&position, end, '/') ||
          !SkipExactly(&position, end, '/')) {
        return false;
      }
      if (position == end) {
        return false;
      }
      begin_host = position;
      SkipWhile<IsNotColonOrSlash>(&position, end);
    }

    if (position < end && *position == ':') {
      // host:port || scheme://host:port
      //     ^                     ^
      begin_port = position;
      SkipUntil(&position, end, '/');
    }
  }

  if (position < end && *position == '/') {
    // scheme://host/path || scheme://host:port/path
    //              ^                          ^
    if (position == begin_host) {
      return false;
    }
    begin_path = position;
  }

  if (!ParseHost(begin_host, begin_port ? begin_port : begin_path,
                 &config->host, &config->host_wildcard)) {
    return false;
  }

  if (begin_port) {
    if (!ParsePort(begin_port, begin_path, &config->port,
                   &config->port_wildcard)) {
      return false;
    }
  } else {
    config->port = url_parse::PORT_UNSPECIFIED;
  }

  if (begin_path != end) {
    if (!ParsePath(begin_path, end, &config->path)) {
      return false;
    }
  }

  return true;
}

// nonce-source      = "'nonce-" nonce-value "'"
// nonce-value        = 1*( ALPHA / DIGIT / "+" / "/" / "=" )
//
bool SourceList::ParseNonce(const char* begin, const char* end,
                            std::string* nonce) {
  size_t nonce_length = static_cast<size_t>(end - begin);
  const char* prefix = "'nonce-";

  if (nonce_length <= strlen(prefix) ||
      base::strncasecmp(prefix, begin, strlen(prefix)) != 0) {
    return true;
  }

  const char* position = begin + strlen(prefix);
  const char* nonce_begin = position;

  DCHECK_LT(position, end);
  SkipWhile<IsNonceCharacter>(&position, end);
  DCHECK_LE(nonce_begin, position);

  if (position + 1 != end || *position != '\'' || position == nonce_begin) {
    return false;
  }

  *nonce = ToString(nonce_begin, position);
  return true;
}

// hash-source       = "'" hash-algorithm "-" hash-value "'"
// hash-algorithm    = "sha1" / "sha256" / "sha384" / "sha512"
// hash-value        = 1*( ALPHA / DIGIT / "+" / "/" / "=" )
//
bool SourceList::ParseHash(const char* begin, const char* end,
                           DigestValue* hash, HashAlgorithm* hash_algorithm) {
  std::string prefix;
  *hash_algorithm = kHashAlgorithmNone;
  size_t hash_length = static_cast<size_t>(end - begin);

  for (size_t i = 0; i < arraysize(kSupportedPrefixes); ++i) {
    const HashPrefix& algorithm = kSupportedPrefixes[i];
    if (hash_length > strlen(algorithm.prefix) &&
        base::strncasecmp(algorithm.prefix, begin, strlen(algorithm.prefix)) ==
            0) {
      prefix = algorithm.prefix;
      *hash_algorithm = algorithm.type;
      break;
    }
  }

  if (*hash_algorithm == kHashAlgorithmNone) {
    return true;
  }

  const char* position = begin + prefix.length();
  const char* hash_begin = position;

  DCHECK_LT(position, end);
  SkipWhile<IsBase64EncodedCharacter>(&position, end);
  DCHECK_LE(hash_begin, position);

  // Base64 encodings may end with exactly one or two '=' characters
  if (position < end) {
    SkipExactly(&position, position + 1, '=');
  }
  if (position < end) {
    SkipExactly(&position, position + 1, '=');
  }

  if (position + 1 != end || *position != '\'' || position == hash_begin) {
    return false;
  }

  // We accept base64url-encoded data here by normalizing it to base64.
  // TODO: Blink has a NormalizeToBase64() step here.
  std::string hash_vector;
  base::Base64Decode(
      base::StringPiece(hash_begin, static_cast<size_t>(position - hash_begin)),
      &hash_vector);
  if (hash_vector.size() > kMaxDigestSize) {
    return false;
  }
  hash->assign(hash_vector.begin(), hash_vector.end());
  return true;
}

//                     ; <scheme> production from RFC 3986
// scheme      = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
//
bool SourceList::ParseScheme(const char* begin, const char* end,
                             std::string* scheme) {
  DCHECK_LE(begin, end);
  DCHECK(scheme && scheme->empty());

  if (begin == end) {
    return false;
  }

  const char* position = begin;

  if (!SkipExactly<IsAsciiAlpha>(&position, end)) {
    return false;
  }

  SkipWhile<IsSchemeContinuationCharacter>(&position, end);

  if (position != end) {
    return false;
  }

  *scheme = ToString(begin, end);
  return true;
}

// host              = [ "*." ] 1*host-char *( "." 1*host-char )
//                   / "*"
// host-char         = ALPHA / DIGIT / "-"
//
bool SourceList::ParseHost(const char* begin, const char* end,
                           std::string* host,
                           SourceConfig::WildcardDisposition* host_wildcard) {
  DCHECK_LE(begin, end);
  DCHECK(host && host->empty());
  DCHECK(host_wildcard && *host_wildcard == SourceConfig::kNoWildcard);

  if (begin == end) {
    return false;
  }
  const char* position = begin;

  if (SkipExactly(&position, end, '*')) {
    *host_wildcard = SourceConfig::kHasWildcard;

    if (position == end) {
      return true;
    }

    if (!SkipExactly(&position, end, '.')) {
      return false;
    }
  }

  const char* host_begin = position;

  while (position < end) {
    if (!SkipExactly<IsHostCharacter>(&position, end)) {
      return false;
    }

    SkipWhile<IsHostCharacter>(&position, end);

    if (position < end && !SkipExactly(&position, end, '.')) {
      return false;
    }
  }

  DCHECK_EQ(position, end);
  *host = ToString(host_begin, end);
  return true;
}

bool SourceList::ParsePath(const char* begin, const char* end,
                           std::string* path) {
  DCHECK_LE(begin, end);
  DCHECK(path && path->empty());

  const char* position = begin;
  SkipWhile<IsPathComponentCharacter>(&position, end);
  // path/to/file.js?query=string || path/to/file.js#anchor
  //                ^                               ^
  if (position < end) {
    policy_->ReportInvalidPathCharacter(directive_name_, ToString(begin, end),
                                        *position);
  }

  *path = net::UnescapeURLComponent(
      ToString(begin, position),
      net::UnescapeRule::SPACES | net::UnescapeRule::URL_SPECIAL_CHARS);

  DCHECK_LE(position, end);
  DCHECK(position == end || (*position == '#' || *position == '?'));
  return true;
}

// port              = ":" ( 1*DIGIT / "*" )
//
bool SourceList::ParsePort(const char* begin, const char* end, int* port,
                           SourceConfig::WildcardDisposition* port_wildcard) {
  DCHECK_LE(begin, end);
  DCHECK(port && *port == url_parse::PORT_UNSPECIFIED);
  DCHECK(port_wildcard && *port_wildcard == SourceConfig::kNoWildcard);

  if (!SkipExactly(&begin, end, ':')) {
    NOTREACHED();
  }

  if (begin == end) {
    return false;
  }

  if (end - begin == 1 && *begin == '*') {
    *port = url_parse::PORT_UNSPECIFIED;
    *port_wildcard = SourceConfig::kHasWildcard;
    return true;
  }

  const char* position = begin;
  SkipWhile<IsAsciiDigit>(&position, end);

  if (position != end) {
    return false;
  }

  bool ok = base::StringToInt(
      base::StringPiece(begin, static_cast<size_t>(end - begin)), port);
  return ok;
}

void SourceList::AddSourceLocalhost() {
  allow_insecure_connections_to_localhost_ = true;
}

void SourceList::AddSourceLocalNetwork() {
  allow_insecure_connections_to_local_network_ = true;
}

void SourceList::AddSourcePrivateRange() {
  allow_insecure_connections_to_private_range_ = true;
}

void SourceList::AddSourceSelf() { allow_self_ = true; }

void SourceList::AddSourceStar() { allow_star_ = true; }

void SourceList::AddSourceUnsafeInline() { allow_inline_ = true; }

void SourceList::AddSourceUnsafeEval() { allow_eval_ = true; }

void SourceList::AddSourceNonce(const std::string& nonce) {
  nonces_.insert(nonce);
}

void SourceList::AddSourceHash(const HashAlgorithm& algorithm,
                               const DigestValue& hash) {
  hashes_.insert(HashValue(algorithm, hash));
  hash_algorithms_used_ |= algorithm;
}

}  // namespace csp
}  // namespace cobalt
