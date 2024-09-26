// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/url_pattern/url_pattern.h"

#include "base/strings/string_util.h"
#include "third_party/blink/renderer/bindings/core/v8/script_regexp.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_urlpatterninit_usvstring.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern_component_result.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern_options.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_url_pattern_result.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern_canon.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern_component.h"
#include "third_party/blink/renderer/core/url_pattern/url_pattern_parser.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_to_number.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/liburlpattern/pattern.h"
#include "third_party/liburlpattern/tokenize.h"
#include "third_party/liburlpattern/utils.h"

namespace blink {

using url_pattern::Component;
using url_pattern::ValueType;

namespace {

// Utility function to determine if a pathname is absolute or not.  For
// kURL values this mainly consists of a check for a leading slash.  For
// patterns we do some additional checking for escaped or grouped slashes.
bool IsAbsolutePathname(const String& pathname, ValueType type) {
  if (pathname.empty())
    return false;

  if (pathname[0] == '/')
    return true;

  if (type == ValueType::kURL)
    return false;

  if (pathname.length() < 2)
    return false;

  // Patterns treat escaped slashes and slashes within an explicit grouping as
  // valid leading slashes.  For example, "\/foo" or "{/foo}".  Patterns do
  // not consider slashes within a custom regexp group as valid for the leading
  // pathname slash for now.  To support that we would need to be able to
  // detect things like ":name_123(/foo)" as a valid leading group in a pattern,
  // but that is considered too complex for now.
  if ((pathname[0] == '\\' || pathname[0] == '{') && pathname[1] == '/') {
    return true;
  }

  return false;
}

// Utility function to determine if the default port for the given protocol
// matches the given port number.
bool IsProtocolDefaultPort(const String& protocol, const String& port) {
  if (protocol.empty() || port.empty())
    return false;

  bool port_ok = false;
  int port_number = port.Impl()->ToInt(WTF::NumberParsingOptions(), &port_ok);
  if (!port_ok)
    return false;

  StringUTF8Adaptor protocol_utf8(protocol);
  int default_port =
      url::DefaultPortForScheme(protocol_utf8.data(), protocol_utf8.size());
  return default_port != url::PORT_UNSPECIFIED && default_port == port_number;
}

// Base URL values that include pattern string characters should not blow
// up pattern parsing.  Automatically escape them.  We must not escape inputs
// for non-pattern base URLs, though.
String EscapeBaseURLString(const String& input, ValueType type) {
  if (type != ValueType::kPattern || !input.length())
    return input;

  std::string result;
  result.reserve(input.length());

  StringUTF8Adaptor utf8(input);
  liburlpattern::EscapePatternStringAndAppend(
      absl::string_view(utf8.data(), utf8.size()), result);

  return String::FromUTF8(result);
}

// A utility method that takes a URLPatternInit, splits it apart, and applies
// the individual component values in the given set of strings.  The strings
// are only applied if a value is present in the init structure.
void ApplyInit(const URLPatternInit* init,
               ValueType type,
               String& protocol,
               String& username,
               String& password,
               String& hostname,
               String& port,
               String& pathname,
               String& search,
               String& hash,
               ExceptionState& exception_state) {
  // If there is a baseURL we need to apply its component values first.  The
  // rest of the URLPatternInit structure will then later override these
  // values.  Note, the baseURL will always set either an empty string or
  // longer value for each considered component.  We do not allow null strings
  // to persist for these components past this phase since they should no
  // longer be treated as wildcards.
  KURL base_url;
  if (init->hasBaseURL()) {
    base_url = KURL(init->baseURL());
    if (!base_url.IsValid() || base_url.IsEmpty()) {
      exception_state.ThrowTypeError("Invalid baseURL '" + init->baseURL() +
                                     "'.");
      return;
    }

    protocol = base_url.Protocol()
                   ? EscapeBaseURLString(base_url.Protocol(), type)
                   : g_empty_string;
    username = base_url.User() ? EscapeBaseURLString(base_url.User(), type)
                               : g_empty_string;
    password = base_url.Pass() ? EscapeBaseURLString(base_url.Pass(), type)
                               : g_empty_string;
    hostname = base_url.Host() ? EscapeBaseURLString(base_url.Host(), type)
                               : g_empty_string;
    port =
        base_url.Port() > 0 ? String::Number(base_url.Port()) : g_empty_string;
    pathname = base_url.GetPath()
                   ? EscapeBaseURLString(base_url.GetPath(), type)
                   : g_empty_string;
    search = base_url.Query() ? EscapeBaseURLString(base_url.Query(), type)
                              : g_empty_string;
    hash = base_url.HasFragmentIdentifier()
               ? EscapeBaseURLString(base_url.FragmentIdentifier(), type)
               : g_empty_string;
  }

  // Apply the URLPatternInit component values on top of the default and
  // baseURL values.
  if (init->hasProtocol()) {
    protocol = url_pattern::CanonicalizeProtocol(init->protocol(), type,
                                                 exception_state);
    if (exception_state.HadException())
      return;
  }
  if (init->hasUsername() || init->hasPassword()) {
    String init_username = init->hasUsername() ? init->username() : String();
    String init_password = init->hasPassword() ? init->password() : String();
    url_pattern::CanonicalizeUsernameAndPassword(init_username, init_password,
                                                 type, username, password,
                                                 exception_state);
    if (exception_state.HadException())
      return;
  }
  if (init->hasHostname()) {
    hostname = url_pattern::CanonicalizeHostname(init->hostname(), type,
                                                 exception_state);
    if (exception_state.HadException())
      return;
  }
  if (init->hasPort()) {
    port = url_pattern::CanonicalizePort(init->port(), type, protocol,
                                         exception_state);
    if (exception_state.HadException())
      return;
  }
  if (init->hasPathname()) {
    pathname = init->pathname();
    if (base_url.IsValid() && base_url.IsHierarchical() &&
        !IsAbsolutePathname(pathname, type)) {
      // Find the last slash in the baseURL pathname.  Since the URL is
      // hierarchical it should have a slash to be valid, but we are cautious
      // and check.  If there is no slash then we cannot use resolve the
      // relative pathname and just treat the init pathname as an absolute
      // value.
      String base_path = EscapeBaseURLString(base_url.GetPath(), type);
      auto slash_index = base_path.ReverseFind("/");
      if (slash_index != kNotFound) {
        // Extract the baseURL path up to and including the first slash.  Append
        // the relative init pathname to it.
        pathname = base_path.Substring(0, slash_index + 1) + pathname;
      }
    }
    pathname = url_pattern::CanonicalizePathname(protocol, pathname, type,
                                                 exception_state);
    if (exception_state.HadException())
      return;
  }
  if (init->hasSearch()) {
    search =
        url_pattern::CanonicalizeSearch(init->search(), type, exception_state);
    if (exception_state.HadException())
      return;
  }
  if (init->hasHash()) {
    hash = url_pattern::CanonicalizeHash(init->hash(), type, exception_state);
    if (exception_state.HadException())
      return;
  }
}

URLPatternComponentResult* MakeURLPatternComponentResult(
    ScriptState* script_state,
    const String& input,
    const Vector<std::pair<String, String>>& group_values) {
  auto* result = URLPatternComponentResult::Create();
  result->setInput(input);

  // Convert null WTF::String values to v8::Undefined.  We have to do this
  // manually because the webidl compiler compiler does not currently
  // support `(USVString or undefined)` in a record value.
  // TODO(crbug.com/1293259): Use webidl `(USVString or undefined)` when
  //                          available in the webidl compiler.
  HeapVector<std::pair<String, ScriptValue>> v8_group_values;
  v8_group_values.reserve(group_values.size());
  for (const auto& pair : group_values) {
    v8::Local<v8::Value> v8_value;
    if (pair.second.IsNull()) {
      v8_value = v8::Undefined(script_state->GetIsolate());
    } else {
      v8_value = ToV8Traits<IDLUSVString>::ToV8(script_state, pair.second)
                     .ToLocalChecked();
    }
    v8_group_values.emplace_back(
        pair.first,
        ScriptValue(script_state->GetIsolate(), std::move(v8_value)));
  }

  result->setGroups(std::move(v8_group_values));
  return result;
}

}  // namespace

URLPattern* URLPattern::Create(const V8URLPatternInput* input,
                               const String& base_url,
                               const URLPatternOptions* options,
                               ExceptionState& exception_state) {
  if (input->GetContentType() ==
      V8URLPatternInput::ContentType::kURLPatternInit) {
    exception_state.ThrowTypeError(
        "Invalid second argument baseURL '" + base_url +
        "' provided with a URLPatternInit input. Use the "
        "URLPatternInit.baseURL property instead.");
    return nullptr;
  }

  const auto& input_string = input->GetAsUSVString();

  url_pattern::Parser parser(input_string, *options);
  parser.Parse(exception_state);
  if (exception_state.HadException())
    return nullptr;

  URLPatternInit* init = parser.GetResult();
  if (!base_url && !init->hasProtocol()) {
    exception_state.ThrowTypeError(
        "Relative constructor string '" + input_string +
        "' must have a base URL passed as the second argument.");
    return nullptr;
  }

  if (base_url)
    init->setBaseURL(base_url);

  return Create(init, parser.GetProtocolComponent(), options, exception_state);
}

URLPattern* URLPattern::Create(const V8URLPatternInput* input,
                               const String& base_url,
                               ExceptionState& exception_state) {
  return Create(input, base_url, MakeGarbageCollected<URLPatternOptions>(),
                exception_state);
}

URLPattern* URLPattern::Create(const V8URLPatternInput* input,
                               const URLPatternOptions* options,
                               ExceptionState& exception_state) {
  if (input->IsURLPatternInit()) {
    return URLPattern::Create(input->GetAsURLPatternInit(),
                              /*precomputed_protocol_component=*/nullptr,
                              options, exception_state);
  }
  return Create(input, /*base_url=*/String(), options, exception_state);
}

URLPattern* URLPattern::Create(const V8URLPatternInput* input,
                               ExceptionState& exception_state) {
  if (input->IsURLPatternInit()) {
    return URLPattern::Create(input->GetAsURLPatternInit(),
                              /*precomputed_protocol_component=*/nullptr,
                              MakeGarbageCollected<URLPatternOptions>(),
                              exception_state);
  }

  return Create(input, /*base_url=*/String(), exception_state);
}

URLPattern* URLPattern::Create(const URLPatternInit* init,
                               Component* precomputed_protocol_component,
                               const URLPatternOptions* options,
                               ExceptionState& exception_state) {
  // Each component defaults to a wildcard matching any input.  We use
  // the null string as a shorthand for the default.
  String protocol;
  String username;
  String password;
  String hostname;
  String port;
  String pathname;
  String search;
  String hash;

  // Apply the input URLPatternInit on top of the default values.
  ApplyInit(init, ValueType::kPattern, protocol, username, password, hostname,
            port, pathname, search, hash, exception_state);
  if (exception_state.HadException())
    return nullptr;

  // Manually canonicalize port patterns that exactly match the default
  // port for the protocol.  We must do this separately from the compile
  // since the liburlpattern::Parse() method will invoke encoding callbacks
  // for partial values within the pattern and this transformation must apply
  // to the entire value.
  if (IsProtocolDefaultPort(protocol, port))
    port = "";

  // Compile each component pattern into a Component structure that
  // can be used for matching.

  auto* protocol_component = precomputed_protocol_component;
  if (!protocol_component) {
    protocol_component = Component::Compile(
        protocol, Component::Type::kProtocol,
        /*protocol_component=*/nullptr, *options, exception_state);
  }
  if (exception_state.HadException())
    return nullptr;

  auto* username_component =
      Component::Compile(username, Component::Type::kUsername,
                         protocol_component, *options, exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* password_component =
      Component::Compile(password, Component::Type::kPassword,
                         protocol_component, *options, exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* hostname_component =
      Component::Compile(hostname, Component::Type::kHostname,
                         protocol_component, *options, exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* port_component =
      Component::Compile(port, Component::Type::kPort, protocol_component,
                         *options, exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* pathname_component =
      Component::Compile(pathname, Component::Type::kPathname,
                         protocol_component, *options, exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* search_component =
      Component::Compile(search, Component::Type::kSearch, protocol_component,
                         *options, exception_state);
  if (exception_state.HadException())
    return nullptr;

  auto* hash_component =
      Component::Compile(hash, Component::Type::kHash, protocol_component,
                         *options, exception_state);
  if (exception_state.HadException())
    return nullptr;

  return MakeGarbageCollected<URLPattern>(
      protocol_component, username_component, password_component,
      hostname_component, port_component, pathname_component, search_component,
      hash_component, base::PassKey<URLPattern>());
}

URLPattern::URLPattern(Component* protocol,
                       Component* username,
                       Component* password,
                       Component* hostname,
                       Component* port,
                       Component* pathname,
                       Component* search,
                       Component* hash,
                       base::PassKey<URLPattern> key)
    : protocol_(protocol),
      username_(username),
      password_(password),
      hostname_(hostname),
      port_(port),
      pathname_(pathname),
      search_(search),
      hash_(hash) {}

bool URLPattern::test(ScriptState* script_state,
                      const V8URLPatternInput* input,
                      const String& base_url,
                      ExceptionState& exception_state) const {
  return Match(script_state, input, base_url, /*result=*/nullptr,
               exception_state);
}

bool URLPattern::test(ScriptState* script_state,
                      const V8URLPatternInput* input,
                      ExceptionState& exception_state) const {
  return test(script_state, input, /*base_url=*/String(), exception_state);
}

URLPatternResult* URLPattern::exec(ScriptState* script_state,
                                   const V8URLPatternInput* input,
                                   const String& base_url,
                                   ExceptionState& exception_state) const {
  URLPatternResult* result = URLPatternResult::Create();
  if (!Match(script_state, input, base_url, result, exception_state))
    return nullptr;
  return result;
}

URLPatternResult* URLPattern::exec(ScriptState* script_state,
                                   const V8URLPatternInput* input,
                                   ExceptionState& exception_state) const {
  return exec(script_state, input, /*base_url=*/String(), exception_state);
}

String URLPattern::protocol() const {
  return protocol_->GeneratePatternString();
}

String URLPattern::username() const {
  return username_->GeneratePatternString();
}

String URLPattern::password() const {
  return password_->GeneratePatternString();
}

String URLPattern::hostname() const {
  return hostname_->GeneratePatternString();
}

String URLPattern::port() const {
  return port_->GeneratePatternString();
}

String URLPattern::pathname() const {
  return pathname_->GeneratePatternString();
}

String URLPattern::search() const {
  return search_->GeneratePatternString();
}

String URLPattern::hash() const {
  return hash_->GeneratePatternString();
}

// static
int URLPattern::compareComponent(const V8URLPatternComponent& component,
                                 const URLPattern* left,
                                 const URLPattern* right) {
  switch (component.AsEnum()) {
    case V8URLPatternComponent::Enum::kProtocol:
      return url_pattern::Component::Compare(*left->protocol_,
                                             *right->protocol_);
    case V8URLPatternComponent::Enum::kUsername:
      return url_pattern::Component::Compare(*left->username_,
                                             *right->username_);
    case V8URLPatternComponent::Enum::kPassword:
      return url_pattern::Component::Compare(*left->password_,
                                             *right->password_);
    case V8URLPatternComponent::Enum::kHostname:
      return url_pattern::Component::Compare(*left->hostname_,
                                             *right->hostname_);
    case V8URLPatternComponent::Enum::kPort:
      return url_pattern::Component::Compare(*left->port_, *right->port_);
    case V8URLPatternComponent::Enum::kPathname:
      return url_pattern::Component::Compare(*left->pathname_,
                                             *right->pathname_);
    case V8URLPatternComponent::Enum::kSearch:
      return url_pattern::Component::Compare(*left->search_, *right->search_);
    case V8URLPatternComponent::Enum::kHash:
      return url_pattern::Component::Compare(*left->hash_, *right->hash_);
  }
  NOTREACHED();
}

String URLPattern::ToString() const {
  StringBuilder builder;
  builder.Append("(");
  Vector<String> components = {protocol(), username(), password(), hostname(),
                               port(),     pathname(), search(),   hash()};
  for (wtf_size_t i = 0; i < components.size(); i++) {
    builder.Append(components[i] == g_empty_string ? " " : components[i]);
    if (i != components.size() - 1)
      builder.Append(",");
  }
  builder.Append(")");
  return builder.ReleaseString();
}

void URLPattern::Trace(Visitor* visitor) const {
  visitor->Trace(protocol_);
  visitor->Trace(username_);
  visitor->Trace(password_);
  visitor->Trace(hostname_);
  visitor->Trace(port_);
  visitor->Trace(pathname_);
  visitor->Trace(search_);
  visitor->Trace(hash_);
  ScriptWrappable::Trace(visitor);
}

bool URLPattern::Match(ScriptState* script_state,
                       const V8URLPatternInput* input,
                       const String& base_url,
                       URLPatternResult* result,
                       ExceptionState& exception_state) const {
  // By default each URL component value starts with an empty string.  The
  // given input is then layered on top of these defaults.
  String protocol(g_empty_string);
  String username(g_empty_string);
  String password(g_empty_string);
  String hostname(g_empty_string);
  String port(g_empty_string);
  String pathname(g_empty_string);
  String search(g_empty_string);
  String hash(g_empty_string);

  HeapVector<Member<V8URLPatternInput>> inputs;

  switch (input->GetContentType()) {
    case V8URLPatternInput::ContentType::kURLPatternInit: {
      if (base_url) {
        exception_state.ThrowTypeError(
            "Invalid second argument baseURL '" + base_url +
            "' provided with a URLPatternInit input. Use the "
            "URLPatternInit.baseURL property instead.");
        return false;
      }

      URLPatternInit* init = input->GetAsURLPatternInit();

      inputs.push_back(MakeGarbageCollected<V8URLPatternInput>(init));

      // Layer the URLPatternInit values on top of the default empty strings.
      ApplyInit(init, ValueType::kURL, protocol, username, password, hostname,
                port, pathname, search, hash, exception_state);
      if (exception_state.HadException()) {
        // Treat exceptions simply as a failure to match.
        exception_state.ClearException();
        return false;
      }
      break;
    }
    case V8URLPatternInput::ContentType::kUSVString: {
      KURL parsed_base_url(base_url);
      if (base_url && !parsed_base_url.IsValid()) {
        // Treat as failure to match, but don't throw an exception.
        return false;
      }

      const String& input_string = input->GetAsUSVString();

      inputs.push_back(MakeGarbageCollected<V8URLPatternInput>(input_string));
      if (base_url)
        inputs.push_back(MakeGarbageCollected<V8URLPatternInput>(base_url));

      // The compile the input string as a fully resolved URL.
      KURL url(parsed_base_url, input_string);
      if (!url.IsValid() || url.IsEmpty()) {
        // Treat as failure to match, but don't throw an exception.
        return false;
      }

      // Apply the parsed URL components on top of our defaults.
      if (url.Protocol())
        protocol = url.Protocol();
      if (url.User())
        username = url.User();
      if (url.Pass())
        password = url.Pass();
      if (url.Host())
        hostname = url.Host();
      if (url.Port() > 0)
        port = String::Number(url.Port());
      if (url.GetPath())
        pathname = url.GetPath();
      if (url.Query())
        search = url.Query();
      if (url.FragmentIdentifier())
        hash = url.FragmentIdentifier();
      break;
    }
  }

  // Declare vectors to hold matched group name/value pairs produced by the
  // matching algorithm.
  Vector<std::pair<String, String>> protocol_group_list;
  Vector<std::pair<String, String>> username_group_list;
  Vector<std::pair<String, String>> password_group_list;
  Vector<std::pair<String, String>> hostname_group_list;
  Vector<std::pair<String, String>> port_group_list;
  Vector<std::pair<String, String>> pathname_group_list;
  Vector<std::pair<String, String>> search_group_list;
  Vector<std::pair<String, String>> hash_group_list;

  // If we are not generating a full result then we don't need to populate
  // group lists.
  auto* protocol_group_list_ref = result ? &protocol_group_list : nullptr;
  auto* username_group_list_ref = result ? &username_group_list : nullptr;
  auto* password_group_list_ref = result ? &password_group_list : nullptr;
  auto* hostname_group_list_ref = result ? &hostname_group_list : nullptr;
  auto* port_group_list_ref = result ? &port_group_list : nullptr;
  auto* pathname_group_list_ref = result ? &pathname_group_list : nullptr;
  auto* search_group_list_ref = result ? &search_group_list : nullptr;
  auto* hash_group_list_ref = result ? &hash_group_list : nullptr;

  CHECK(protocol_);
  CHECK(username_);
  CHECK(password_);
  CHECK(hostname_);
  CHECK(port_);
  CHECK(pathname_);
  CHECK(search_);
  CHECK(hash_);

  // Each component of the pattern must match the corresponding component of
  // the input.
  bool matched = protocol_->Match(protocol, protocol_group_list_ref) &&
                 username_->Match(username, username_group_list_ref) &&
                 password_->Match(password, password_group_list_ref) &&
                 hostname_->Match(hostname, hostname_group_list_ref) &&
                 port_->Match(port, port_group_list_ref) &&
                 pathname_->Match(pathname, pathname_group_list_ref) &&
                 search_->Match(search, search_group_list_ref) &&
                 hash_->Match(hash, hash_group_list_ref);

  if (!matched || !result)
    return matched;

  result->setInputs(std::move(inputs));

  result->setProtocol(MakeURLPatternComponentResult(script_state, protocol,
                                                    protocol_group_list));
  result->setUsername(MakeURLPatternComponentResult(script_state, username,
                                                    username_group_list));
  result->setPassword(MakeURLPatternComponentResult(script_state, password,
                                                    password_group_list));
  result->setHostname(MakeURLPatternComponentResult(script_state, hostname,
                                                    hostname_group_list));
  result->setPort(
      MakeURLPatternComponentResult(script_state, port, port_group_list));
  result->setPathname(MakeURLPatternComponentResult(script_state, pathname,
                                                    pathname_group_list));
  result->setSearch(
      MakeURLPatternComponentResult(script_state, search, search_group_list));
  result->setHash(
      MakeURLPatternComponentResult(script_state, hash, hash_group_list));

  return true;
}

// static

}  // namespace blink
