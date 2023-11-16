// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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
#include "base/strings/string_number_conversions.h"
#include "cobalt/network/socket_address_parser.h"
#include "net/base/escape.h"
#include "starboard/common/socket.h"
#include "url/url_canon_ip.h"
#include "url/url_constants.h"

namespace cobalt {
namespace csp {

namespace {

bool IsSourceListNone(const char* begin, const char* end) {
  SkipWhile<base::IsAsciiWhitespace>(&begin, end);

  const char* position = begin;
  SkipWhile<IsSourceCharacter>(&position, end);
  size_t len = static_cast<size_t>(position - begin);
  if (strncasecmp("'none'", begin, len) != 0) {
    return false;
  }

  SkipWhile<base::IsAsciiWhitespace>(&position, end);
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

bool IsInsecureScheme(const GURL& url) {
  return url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kWsScheme);
}

}  // namespace

SourceList::SourceList(const LocalNetworkCheckerInterface* checker,
                       ContentSecurityPolicy* policy,
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
      local_network_checker_(checker) {}

SourceList::SourceList(const LocalNetworkCheckerInterface* checker,
                       ContentSecurityPolicy* policy, const SourceList& other)
    : policy_(policy),
      directive_name_(other.directive_name_),
      allow_self_(other.allow_self_),
      allow_star_(other.allow_star_),
      allow_inline_(other.allow_inline_),
      allow_eval_(other.allow_eval_),
      allow_insecure_connections_to_local_network_(
          other.allow_insecure_connections_to_local_network_),
      allow_insecure_connections_to_localhost_(
          other.allow_insecure_connections_to_localhost_),
      allow_insecure_connections_to_private_range_(
          other.allow_insecure_connections_to_private_range_),
      hash_algorithms_used_(0),
      local_network_checker_(checker) {
  for (Source source : other.list_) {
    list_.push_back(Source(policy, source));
  }
}

bool SourceList::Matches(const GURL& url,
                         RedirectStatus redirect_status) const {
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

  const url::Parsed& parsed = url.parsed_for_possibly_invalid_spec();

  if (!url.has_host() || !parsed.host.is_valid() ||
      !parsed.host.is_nonempty()) {
    return false;
  }

  if (!IsInsecureScheme(url)) {
    return false;
  }

  if (allow_insecure_connections_to_localhost_) {
    std::string host;
    // This will be our host string if we are not using IPV6.
    host.append(valid_spec.c_str() + parsed.host.begin,
                valid_spec.c_str() + parsed.host.begin + parsed.host.len);
    if (SbSocketIsIpv6Supported()) host = url.HostNoBrackets();
    if (net::HostStringIsLocalhost(host)) {
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
  if (!cobalt::network::ParseSocketAddress(valid_spec.c_str(), parsed.host,
                                           &destination)) {
    return false;
  }

  if (allow_insecure_connections_to_private_range_ &&
      local_network_checker_->IsIPInPrivateRange(destination)) {
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
    SkipWhile<base::IsAsciiWhitespace>(&position, end);
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

    DCHECK(position == end || base::IsAsciiWhitespace(*position));
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

  if (base::LowerCaseEqualsASCII(begin, end, "'none'")) {
    return false;
  }

  base::StringPiece begin_piece(begin, static_cast<size_t>(end - begin));
  if (begin_piece.length() == 1 && begin_piece[0] == '*') {
    AddSourceStar();
    return true;
  }
  if (base::LowerCaseEqualsASCII(begin, end, "'self'")) {
    AddSourceSelf();
    return true;
  }

  if (base::LowerCaseEqualsASCII(begin, end, "'unsafe-inline'")) {
    AddSourceUnsafeInline();
    return true;
  }

  if (base::LowerCaseEqualsASCII(begin, end, "'unsafe-eval'")) {
    AddSourceUnsafeEval();
    return true;
  }

  if (directive_name_.compare(ContentSecurityPolicy::kConnectSrc) == 0) {
    if (base::LowerCaseEqualsASCII(begin, end, "'cobalt-insecure-localhost'")) {
      AddSourceLocalhost();
      return true;
    }
    if (base::LowerCaseEqualsASCII(begin, end,
                                   "'cobalt-insecure-local-network'")) {
      AddSourceLocalNetwork();
      return true;
    }
    if (base::LowerCaseEqualsASCII(begin, end,
                                   "'cobalt-insecure-private-range'")) {
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
    config->port = url::PORT_UNSPECIFIED;
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
      strncasecmp(prefix, begin, strlen(prefix)) != 0) {
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
// static
bool SourceList::ParseHash(const char* begin, const char* end,
                           DigestValue* hash, HashAlgorithm* hash_algorithm) {
  std::string prefix;
  *hash_algorithm = kHashAlgorithmNone;
  size_t hash_length = static_cast<size_t>(end - begin);

  for (size_t i = 0; i < arraysize(kSupportedPrefixes); ++i) {
    const HashPrefix& algorithm = kSupportedPrefixes[i];
    if (hash_length > strlen(algorithm.prefix) &&
        strncasecmp(algorithm.prefix, begin, strlen(algorithm.prefix)) == 0) {
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

  if (!SkipExactly<base::IsAsciiAlpha>(&position, end)) {
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
      net::UnescapeRule::SPACES | net::UnescapeRule::PATH_SEPARATORS |
          net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);

  DCHECK_LE(position, end);
  DCHECK(position == end || (*position == '#' || *position == '?'));
  return true;
}

// port              = ":" ( 1*DIGIT / "*" )
//
bool SourceList::ParsePort(const char* begin, const char* end, int* port,
                           SourceConfig::WildcardDisposition* port_wildcard) {
  DCHECK_LE(begin, end);
  DCHECK(port && *port == url::PORT_UNSPECIFIED);
  DCHECK(port_wildcard && *port_wildcard == SourceConfig::kNoWildcard);

  if (!SkipExactly(&begin, end, ':')) {
    NOTREACHED();
  }

  if (begin == end) {
    return false;
  }

  if (end - begin == 1 && *begin == '*') {
    *port = url::PORT_UNSPECIFIED;
    *port_wildcard = SourceConfig::kHasWildcard;
    return true;
  }

  const char* position = begin;
  SkipWhile<base::IsAsciiDigit>(&position, end);

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
