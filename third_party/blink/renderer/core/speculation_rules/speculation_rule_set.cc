// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/speculation_rules/speculation_rule_set.h"

#include "base/containers/contains.h"
#include "services/network/public/mojom/no_vary_search.mojom-shared.h"
#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom-shared.h"
#include "third_party/blink/renderer/core/css/style_rule.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/loader/resource/speculation_rules_resource.h"
#include "third_party/blink/renderer/core/script/script_element_base.h"
#include "third_party/blink/renderer/core/speculation_rules/document_rule_predicate.h"
#include "third_party/blink/renderer/core/speculation_rules/speculation_rules_features.h"
#include "third_party/blink/renderer/core/speculation_rules/speculation_rules_metrics.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/json/json_parser.h"
#include "third_party/blink/renderer/platform/json/json_values.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

namespace blink {

namespace {

void AddConsoleMessageForSpeculationRuleSetValidation(
    SpeculationRuleSet& speculation_rule_set,
    Document& element_document,
    ScriptElementBase* script_element,
    SpeculationRulesResource* resource) {
  // `script_element` and `resource` are mutually exclusive.
  CHECK(script_element || resource);
  CHECK(!script_element || !resource);

  if (speculation_rule_set.HasError()) {
    if (speculation_rule_set.ShouldReportUMAForError()) {
      CountSpeculationRulesLoadOutcome(
          script_element ? SpeculationRulesLoadOutcome::kParseErrorInline
                         : SpeculationRulesLoadOutcome::kParseErrorFetched);
    }
    String error_message;
    if (script_element) {
      error_message = "While parsing speculation rules: " +
                      speculation_rule_set.error_message();
    } else {
      error_message = "While parsing speculation rules fetched from \"" +
                      resource->GetResourceRequest().Url().ElidedString() +
                      "\": " + speculation_rule_set.error_message() + "\".";
    }
    auto* console_message = MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kWarning, error_message);
    if (script_element) {
      console_message->SetNodes(element_document.GetFrame(),
                                {script_element->GetDOMNodeId()});
    }
    element_document.AddConsoleMessage(console_message);
  }
  if (speculation_rule_set.HasWarnings()) {
    // Only add the first warning message to console.
    String warning_message;
    if (script_element) {
      warning_message = "While parsing speculation rules: " +
                        speculation_rule_set.warning_messages()[0];
    } else {
      warning_message = "While parsing speculation rules fetched from \"" +
                        resource->GetResourceRequest().Url().ElidedString() +
                        "\": " + speculation_rule_set.warning_messages()[0] +
                        "\".";
    }
    auto* console_message = MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kOther,
        mojom::blink::ConsoleMessageLevel::kWarning, warning_message);
    if (script_element) {
      console_message->SetNodes(element_document.GetFrame(),
                                {script_element->GetDOMNodeId()});
    }
    element_document.AddConsoleMessage(console_message);
  }
}

// https://html.spec.whatwg.org/C/#valid-browsing-context-name
bool IsValidContextName(const String& name_or_keyword) {
  // "A valid browsing context name is any string with at least one character
  // that does not start with a U+005F LOW LINE character. (Names starting with
  // an underscore are reserved for special keywords.)"
  if (name_or_keyword.empty())
    return false;
  if (name_or_keyword.StartsWith("_"))
    return false;
  return true;
}

// https://html.spec.whatwg.org/C/#valid-browsing-context-name-or-keyword
bool IsValidBrowsingContextNameOrKeyword(const String& name_or_keyword) {
  // "A valid browsing context name or keyword is any string that is either a
  // valid browsing context name or that is an ASCII case-insensitive match for
  // one of: _blank, _self, _parent, or _top."
  String canonicalized_name_or_keyword = name_or_keyword.LowerASCII();
  if (IsValidContextName(name_or_keyword) ||
      EqualIgnoringASCIICase(name_or_keyword, "_blank") ||
      EqualIgnoringASCIICase(name_or_keyword, "_self") ||
      EqualIgnoringASCIICase(name_or_keyword, "_parent") ||
      EqualIgnoringASCIICase(name_or_keyword, "_top")) {
    return true;
  }
  return false;
}

// If `out_error` is provided and hasn't already had a message set, sets it to
// `message`.
void SetParseErrorMessage(String* out_error, String message) {
  if (out_error && out_error->IsNull()) {
    *out_error = message;
  }
}

SpeculationRule* ParseSpeculationRule(JSONObject* input,
                                      const KURL& base_url,
                                      ExecutionContext* context,
                                      String* out_error,
                                      Vector<String>& out_warnings) {
  // https://wicg.github.io/nav-speculation/speculation-rules.html#parse-a-speculation-rule

  // If input has any key other than "source", "urls", "where", "requires",
  // "target_hint", "referrer_policy", "relative_to", and "eagerness", then
  // return null.
  const char* const kKnownKeys[] = {
      "source",          "urls",       "where", "requires", "target_hint",
      "referrer_policy", "relative_to"};
  const auto kConditionalKnownKeys = [context]() {
    Vector<const char*, 4> conditional_known_keys;
    if (speculation_rules::EagernessEnabled(context)) {
      conditional_known_keys.push_back("eagerness");
    }
    if (RuntimeEnabledFeatures::SpeculationRulesNoVarySearchHintEnabled(
            context)) {
      conditional_known_keys.push_back("expects_no_vary_search");
    }
    return conditional_known_keys;
  }();

  for (wtf_size_t i = 0; i < input->size(); ++i) {
    const String& input_key = input->at(i).first;
    if (!base::Contains(kKnownKeys, input_key) &&
        !base::Contains(kConditionalKnownKeys, input_key)) {
      SetParseErrorMessage(
          out_error, "A rule contains an unknown key: \"" + input_key + "\".");
      return nullptr;
    }
  }

  bool document_rules_enabled =
      RuntimeEnabledFeatures::SpeculationRulesDocumentRulesEnabled(context);
  const bool relative_to_enabled =
      RuntimeEnabledFeatures::SpeculationRulesRelativeToDocumentEnabled(
          context);

  // If input["source"] does not exist or is neither the string "list" nor the
  // string "document", then return null.
  String source;
  if (!input->GetString("source", &source)) {
    SetParseErrorMessage(out_error, "A rule must have a source.");
    return nullptr;
  }
  if (!(source == "list" || (document_rules_enabled && source == "document"))) {
    SetParseErrorMessage(out_error,
                         "A rule has an unknown source: \"" + source + "\".");
    return nullptr;
  }

  Vector<KURL> urls;
  if (source == "list") {
    // If input["where"] exists, then return null.
    if (input->Get("where")) {
      SetParseErrorMessage(out_error,
                           "A list rule may not have document rule matchers.");
      return nullptr;
    }

    // For now, use the given base URL to construct the list rules.
    KURL base_url_to_parse = base_url;
    //  If input["relative_to"] exists:
    if (JSONValue* relative_to = input->Get("relative_to")) {
      const char* const kKnownRelativeToValues[] = {"ruleset", "document"};
      String value;
      // If relativeTo is neither the string "ruleset" nor the string
      // "document", then return null.
      if (!relative_to_enabled || !relative_to->AsString(&value) ||
          !base::Contains(kKnownRelativeToValues, value)) {
        SetParseErrorMessage(out_error,
                             "A rule has an unknown \"relative_to\" value.");
        return nullptr;
      }
      // If relativeTo is "document", then set baseURL to the document's
      // document base URL.
      if (value == "document") {
        base_url_to_parse = context->BaseURL();
      }
    }

    // Let urls be an empty list.
    // If input["urls"] does not exist, is not a list, or has any element which
    // is not a string, then return null.
    JSONArray* input_urls = input->GetArray("urls");
    if (!input_urls) {
      SetParseErrorMessage(out_error,
                           "A list rule must have a \"urls\" array.");
      return nullptr;
    }

    // For each urlString of input["urls"]...
    urls.ReserveInitialCapacity(input_urls->size());
    for (wtf_size_t i = 0; i < input_urls->size(); ++i) {
      String url_string;
      if (!input_urls->at(i)->AsString(&url_string)) {
        SetParseErrorMessage(out_error, "URLs must be given as strings.");
        return nullptr;
      }

      // Let parsedURL be the result of parsing urlString with baseURL.
      // If parsedURL is failure, then continue.
      KURL parsed_url(base_url_to_parse, url_string);
      if (!parsed_url.IsValid() || !parsed_url.ProtocolIsInHTTPFamily())
        continue;

      urls.push_back(std::move(parsed_url));
    }
  }

  DocumentRulePredicate* document_rule_predicate = nullptr;
  if (source == "document") {
    DCHECK(document_rules_enabled);
    // If input["urls"] exists, then return null.
    if (input->Get("urls")) {
      SetParseErrorMessage(out_error,
                           "A document rule cannot have a \"urls\" key.");
      return nullptr;
    }

    // "relative_to" outside the "href_matches" clause is not allowed for
    // document rules.
    if (input->Get("relative_to")) {
      SetParseErrorMessage(out_error,
                           "A document rule cannot have \"relative_to\" "
                           "outside the \"where\" clause.");
      return nullptr;
    }

    // If input["where"] does not exist, then set predicate to a document rule
    // conjunction whose clauses is an empty list.
    if (!input->Get("where")) {
      document_rule_predicate = DocumentRulePredicate::MakeDefaultPredicate();
    } else {
      // Otherwise, set predicate to the result of parsing a document rule
      // predicate given input["where"] and baseURL.
      document_rule_predicate = DocumentRulePredicate::Parse(
          input->GetJSONObject("where"), base_url, context,
          IGNORE_EXCEPTION_FOR_TESTING, out_error);
    }
    if (!document_rule_predicate)
      return nullptr;
  }

  // Let requirements be an empty ordered set.
  // If input["requires"] exists, but is not a list, then return null.
  JSONValue* requirements = input->Get("requires");
  if (requirements && requirements->GetType() != JSONValue::kTypeArray) {
    SetParseErrorMessage(out_error, "\"requires\" must be an array.");
    return nullptr;
  }

  // For each requirement of input["requires"]...
  SpeculationRule::RequiresAnonymousClientIPWhenCrossOrigin
      requires_anonymous_client_ip(false);
  if (JSONArray* requirements_array = JSONArray::Cast(requirements)) {
    for (wtf_size_t i = 0; i < requirements_array->size(); ++i) {
      String requirement;
      if (!requirements_array->at(i)->AsString(&requirement)) {
        SetParseErrorMessage(out_error, "Requirements must be strings.");
        return nullptr;
      }

      if (requirement == "anonymous-client-ip-when-cross-origin") {
        requires_anonymous_client_ip =
            SpeculationRule::RequiresAnonymousClientIPWhenCrossOrigin(true);
      } else {
        SetParseErrorMessage(
            out_error,
            "A rule has an unknown requirement: \"" + requirement + "\".");
        return nullptr;
      }
    }
  }

  // Let targetHint be null.
  absl::optional<mojom::blink::SpeculationTargetHint> target_hint;

  // If input["target_hint"] exists:
  JSONValue* target_hint_value = input->Get("target_hint");
  if (target_hint_value) {
    // If input["target_hint"] is not a valid browsing context name or keyword,
    // then return null.
    // Set targetHint to input["target_hint"].
    String target_hint_str;
    if (!target_hint_value->AsString(&target_hint_str)) {
      SetParseErrorMessage(out_error, "\"target_hint\" must be a string.");
      return nullptr;
    }
    if (!IsValidBrowsingContextNameOrKeyword(target_hint_str)) {
      SetParseErrorMessage(out_error,
                           "A rule has an invalid \"target_hint\": \"" +
                               target_hint_str + "\".");
      return nullptr;
    }
    target_hint =
        SpeculationRuleSet::SpeculationTargetHintFromString(target_hint_str);
  }

  // Let referrerPolicy be the empty string.
  absl::optional<network::mojom::ReferrerPolicy> referrer_policy;
  // If input["referrer_policy"] exists:
  JSONValue* referrer_policy_value = input->Get("referrer_policy");
  if (referrer_policy_value) {
    // If input["referrer_policy"] is not a referrer policy, then return null.
    String referrer_policy_str;
    if (!referrer_policy_value->AsString(&referrer_policy_str)) {
      SetParseErrorMessage(out_error, "A referrer policy must be a string.");
      return nullptr;
    }

    if (!referrer_policy_str.empty()) {
      network::mojom::ReferrerPolicy referrer_policy_out =
          network::mojom::ReferrerPolicy::kDefault;
      if (!SecurityPolicy::ReferrerPolicyFromString(
              referrer_policy_str, kDoNotSupportReferrerPolicyLegacyKeywords,
              &referrer_policy_out)) {
        SetParseErrorMessage(out_error,
                             "A rule has an invalid referrer policy: \"" +
                                 referrer_policy_str + "\".");
        return nullptr;
      }
      DCHECK_NE(referrer_policy_out, network::mojom::ReferrerPolicy::kDefault);
      // Set referrerPolicy to input["referrer_policy"].
      referrer_policy = referrer_policy_out;
    }
  }

  mojom::blink::SpeculationEagerness eagerness;
  if (JSONValue* eagerness_value = input->Get("eagerness")) {
    // Feature gated due to known keys check above.
    DCHECK(speculation_rules::EagernessEnabled(context));

    String eagerness_str;
    if (!eagerness_value->AsString(&eagerness_str)) {
      SetParseErrorMessage(out_error, "Eagerness value must be a string.");
      return nullptr;
    }

    if (eagerness_str == "eager") {
      eagerness = mojom::blink::SpeculationEagerness::kEager;
    } else if (eagerness_str == "moderate") {
      eagerness = mojom::blink::SpeculationEagerness::kModerate;
    } else if (eagerness_str == "conservative") {
      eagerness = mojom::blink::SpeculationEagerness::kConservative;
    } else {
      SetParseErrorMessage(
          out_error, "Eagerness value: \"" + eagerness_str + "\" is invalid.");
      return nullptr;
    }

    UseCounter::Count(context, WebFeature::kSpeculationRulesExplicitEagerness);
  } else {
    eagerness = source == "list"
                    ? mojom::blink::SpeculationEagerness::kEager
                    : mojom::blink::SpeculationEagerness::kConservative;
  }

  network::mojom::blink::NoVarySearchPtr no_vary_search = nullptr;
  if (JSONValue* no_vary_search_value = input->Get("expects_no_vary_search")) {
    CHECK(RuntimeEnabledFeatures::SpeculationRulesNoVarySearchHintEnabled(
        context));
    String no_vary_search_str;
    if (!no_vary_search_value->AsString(&no_vary_search_str)) {
      SetParseErrorMessage(out_error,
                           "expects_no_vary_search's value must be a string.");
      return nullptr;
    }
    // Parse No-Vary-Search hint value.
    auto no_vary_search_hint = blink::ParseNoVarySearch(no_vary_search_str);
    CHECK(no_vary_search_hint);
    if (no_vary_search_hint->is_parse_error()) {
      const auto& parse_error = no_vary_search_hint->get_parse_error();
      CHECK_NE(parse_error, network::mojom::NoVarySearchParseError::kOk);
      if (parse_error !=
          network::mojom::NoVarySearchParseError::kDefaultValue) {
        out_warnings.push_back(GetNoVarySearchHintConsoleMessage(parse_error));
      }
    } else {
      UseCounter::Count(context, WebFeature::kSpeculationRulesNoVarySearchHint);
      no_vary_search = std::move(no_vary_search_hint->get_no_vary_search());
    }
  }

  const mojom::blink::SpeculationInjectionWorld world =
      context->GetCurrentWorld()
          ? context->GetCurrentWorld()->IsMainWorld()
                ? mojom::blink::SpeculationInjectionWorld::kMain
                : mojom::blink::SpeculationInjectionWorld::kIsolated
          : mojom::blink::SpeculationInjectionWorld::kNone;

  return MakeGarbageCollected<SpeculationRule>(
      std::move(urls), document_rule_predicate, requires_anonymous_client_ip,
      target_hint, referrer_policy, eagerness, std::move(no_vary_search),
      world);
}

}  // namespace

// ---- SpeculationRuleSet::Source implementation ----

SpeculationRuleSet::Source::Source(base::PassKey<SpeculationRuleSet::Source>,
                                   const String& source_text,
                                   Document* document,
                                   absl::optional<DOMNodeId> node_id,
                                   absl::optional<KURL> base_url,
                                   absl::optional<uint64_t> request_id)
    : source_text_(source_text),
      document_(document),
      node_id_(node_id),
      base_url_(base_url),
      request_id_(request_id) {}

SpeculationRuleSet::Source* SpeculationRuleSet::Source::FromInlineScript(
    const String& source_text,
    Document& document,
    DOMNodeId node_id) {
  return MakeGarbageCollected<Source>(base::PassKey<Source>(), source_text,
                                      &document, node_id, absl::nullopt,
                                      absl::nullopt);
}

SpeculationRuleSet::Source* SpeculationRuleSet::Source::FromRequest(
    const String& source_text,
    const KURL& base_url,
    uint64_t request_id) {
  return MakeGarbageCollected<Source>(base::PassKey<Source>(), source_text,
                                      nullptr, absl::nullopt, base_url,
                                      request_id);
}

SpeculationRuleSet::Source* SpeculationRuleSet::Source::FromBrowserInjected(
    const String& source_text,
    const KURL& base_url) {
  return MakeGarbageCollected<Source>(base::PassKey<Source>(), source_text,
                                      nullptr, absl::nullopt, base_url,
                                      absl::nullopt);
}

bool SpeculationRuleSet::Source::IsFromInlineScript() const {
  return node_id_.has_value();
}

bool SpeculationRuleSet::Source::IsFromRequest() const {
  return request_id_.has_value();
}

bool SpeculationRuleSet::Source::IsFromBrowserInjected() const {
  return !IsFromInlineScript() && !IsFromRequest();
}

const String& SpeculationRuleSet::Source::GetSourceText() const {
  return source_text_;
}

const absl::optional<DOMNodeId>& SpeculationRuleSet::Source::GetNodeId() const {
  return node_id_;
}

const absl::optional<KURL> SpeculationRuleSet::Source::GetSourceURL() const {
  if (IsFromRequest()) {
    CHECK(base_url_.has_value());
    return base_url_;
  }
  return absl::nullopt;
}

const absl::optional<uint64_t>& SpeculationRuleSet::Source::GetRequestId()
    const {
  return request_id_;
}

KURL SpeculationRuleSet::Source::GetBaseURL() const {
  if (base_url_) {
    DCHECK(!document_);
    return base_url_.value();
  }
  DCHECK(document_);
  return document_->BaseURL();
}

void SpeculationRuleSet::Source::Trace(Visitor* visitor) const {
  visitor->Trace(document_);
}

// ---- SpeculationRuleSet implementation ----

SpeculationRuleSet::SpeculationRuleSet(base::PassKey<SpeculationRuleSet>,
                                       Source* source)
    : inspector_id_(IdentifiersFactory::CreateIdentifier()), source_(source) {}

void SpeculationRuleSet::SetError(SpeculationRuleSetErrorType error_type,
                                  String error_message) {
  // Only the first error will be reported.
  if (error_type_ != SpeculationRuleSetErrorType::kNoError) {
    return;
  }

  error_type_ = error_type;
  error_message_ = error_message;
}

void SpeculationRuleSet::SetWarnings(Vector<String> warning_messages) {
  warning_messages_ = std::move(warning_messages);
}

// static
SpeculationRuleSet* SpeculationRuleSet::Parse(Source* source,
                                              ExecutionContext* context) {
  // https://wicg.github.io/nav-speculation/speculation-rules.html#parse-speculation-rules

  const String& source_text = source->GetSourceText();
  const KURL& base_url = source->GetBaseURL();

  // Let result be an empty speculation rule set.
  SpeculationRuleSet* result = MakeGarbageCollected<SpeculationRuleSet>(
      base::PassKey<SpeculationRuleSet>(), source);

  // Let parsed be the result of parsing a JSON string to an Infra value given
  // input.
  JSONParseError parse_error;
  auto parsed = JSONObject::From(ParseJSON(source_text, &parse_error));

  // If parsed is not a map, then return an empty rule sets.
  if (!parsed) {
    result->SetError(SpeculationRuleSetErrorType::kSourceIsNotJsonObject,
                     parse_error.type != JSONParseErrorType::kNoError
                         ? parse_error.message
                         : "Parsed JSON must be an object.");
    return result;
  }

  const auto parse_for_action =
      [&](const char* key, HeapVector<Member<SpeculationRule>>& destination,
          bool allow_target_hint) {
        // If key doesn't exist, it is not an error and is nop.
        JSONValue* value = parsed->Get(key);
        if (!value) {
          return;
        }

        JSONArray* array = JSONArray::Cast(value);
        if (!array) {
          result->SetError(SpeculationRuleSetErrorType::kInvalidRulesSkipped,
                           "A rule set for a key must be an array: path = [\"" +
                               String(key) + "\"]");
          return;
        }

        for (wtf_size_t i = 0; i < array->size(); ++i) {
          // If prefetch/prerenderRule is not a map, then continue.
          JSONObject* input_rule = JSONObject::Cast(array->at(i));
          if (!input_rule) {
            result->SetError(SpeculationRuleSetErrorType::kInvalidRulesSkipped,
                             "A rule must be an object: path = [\"" +
                                 String(key) + "\"][" + String::Number(i) +
                                 "]");
            continue;
          }

          // Let rule be the result of parsing a speculation rule given
          // prefetch/prerenderRule and baseURL.
          //
          // TODO(https://crbug.com/1410709): Refactor
          // ParseSpeculationRule to return
          // `std::tuple<SpeculationRule*, String, Vector<String>>`.
          String error_message;
          Vector<String> warning_messages;
          SpeculationRule* rule = ParseSpeculationRule(
              input_rule, base_url, context, &error_message, warning_messages);

          // If parse failed for a rule, then ignore it and continue.
          if (!rule) {
            result->SetError(SpeculationRuleSetErrorType::kInvalidRulesSkipped,
                             error_message);
            continue;
          }

          // If rule's target browsing context name hint is not null, then
          // continue.
          if (!allow_target_hint &&
              rule->target_browsing_context_name_hint().has_value()) {
            result->SetError(SpeculationRuleSetErrorType::kInvalidRulesSkipped,
                             "\"target_hint\" may not be set for " +
                                 String(key) + " rules.");
            continue;
          }

          // Add the warnings and continue
          result->SetWarnings(std::move(warning_messages));

          if (rule->predicate()) {
            result->has_document_rule_ = true;
            result->selectors_.AppendVector(rule->predicate()->GetStyleRules());
          }

          if (rule->eagerness() != mojom::blink::SpeculationEagerness::kEager) {
            result->requires_unfiltered_input_ = true;
          }

          // Append rule to result's prefetch/prerender rules.
          destination.push_back(rule);
        }
      };

  // If parsed["prefetch"] exists and is a list, then for each...
  parse_for_action("prefetch", result->prefetch_rules_, false);

  // If parsed["prefetch_with_subresources"] exists and is a list, then for
  // each...
  parse_for_action("prefetch_with_subresources",
                   result->prefetch_with_subresources_rules_, false);

  // If parsed["prerender"] exists and is a list, then for each...
  parse_for_action("prerender", result->prerender_rules_, true);

  return result;
}

bool SpeculationRuleSet::HasError() const {
  return error_type_ != SpeculationRuleSetErrorType::kNoError;
}

bool SpeculationRuleSet::HasWarnings() const {
  return !warning_messages_.empty();
}

bool SpeculationRuleSet::ShouldReportUMAForError() const {
  // We report UMAs only if entire parse failed.
  switch (error_type_) {
    case SpeculationRuleSetErrorType::kSourceIsNotJsonObject:
      return true;
    case SpeculationRuleSetErrorType::kNoError:
    case SpeculationRuleSetErrorType::kInvalidRulesSkipped:
      return false;
  }
}

// static
mojom::blink::SpeculationTargetHint
SpeculationRuleSet::SpeculationTargetHintFromString(
    const StringView& target_hint_str) {
  // Currently only "_blank" and "_self" are supported.
  // TODO(https://crbug.com/1354049): Support more browsing context names and
  // keywords.
  if (EqualIgnoringASCIICase(target_hint_str, "_blank")) {
    return mojom::blink::SpeculationTargetHint::kBlank;
  } else if (EqualIgnoringASCIICase(target_hint_str, "_self")) {
    return mojom::blink::SpeculationTargetHint::kSelf;
  } else {
    return mojom::blink::SpeculationTargetHint::kNoHint;
  }
}

void SpeculationRuleSet::Trace(Visitor* visitor) const {
  visitor->Trace(prefetch_rules_);
  visitor->Trace(prefetch_with_subresources_rules_);
  visitor->Trace(prerender_rules_);
  visitor->Trace(source_);
  visitor->Trace(selectors_);
}

void SpeculationRuleSet::AddConsoleMessageForValidation(
    ScriptElementBase& script_element) {
  AddConsoleMessageForSpeculationRuleSetValidation(
      *this, script_element.GetDocument(), &script_element, nullptr);
}

void SpeculationRuleSet::AddConsoleMessageForValidation(
    Document& document,
    SpeculationRulesResource& resource) {
  AddConsoleMessageForSpeculationRuleSetValidation(*this, document, nullptr,
                                                   &resource);
}

}  // namespace blink
