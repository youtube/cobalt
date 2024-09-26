// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy_factory.h"

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_html.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_script.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_trusted_script_url.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/event_target_names.h"
#include "third_party/blink/renderer/core/events/before_create_policy_event.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/exception_metadata.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/trustedtypes/event_handler_names.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_html.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_type_policy.h"
#include "third_party/blink/renderer/core/trustedtypes/trusted_types_util.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_dom_wrapper.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace blink {

namespace {

const char* kHtmlNamespace = "http://www.w3.org/1999/xhtml";

struct AttributeTypeEntry {
  AtomicString element;
  AtomicString attribute;
  AtomicString element_namespace;
  AtomicString attribute_namespace;
  SpecificTrustedType type;
};

typedef Vector<AttributeTypeEntry> AttributeTypeVector;

AttributeTypeVector BuildAttributeVector() {
  const struct {
    const char* element;
    const char* attribute;
    const char* element_namespace;
    const char* attribute_namespace;
    SpecificTrustedType type;
  } kTypeTable[] = {
      {"embed", "src", kHtmlNamespace, nullptr,
       SpecificTrustedType::kScriptURL},
      {"iframe", "srcdoc", kHtmlNamespace, nullptr, SpecificTrustedType::kHTML},
      {"object", "codebase", kHtmlNamespace, nullptr,
       SpecificTrustedType::kScriptURL},
      {"object", "data", kHtmlNamespace, nullptr,
       SpecificTrustedType::kScriptURL},
      {"script", "src", kHtmlNamespace, nullptr,
       SpecificTrustedType::kScriptURL},
#define FOREACH_EVENT_HANDLER(name) \
  {"*", #name, nullptr, nullptr, SpecificTrustedType::kScript},
      EVENT_HANDLER_LIST(FOREACH_EVENT_HANDLER)
#undef FOREACH_EVENT_HANDLER
  };

  AttributeTypeVector table;
  for (const auto& entry : kTypeTable) {
    // Attribute comparisons are case-insensitive, for both element and
    // attribute name. We rely on the fact that they're stored as lowercase.
    DCHECK_EQ(String(entry.element).LowerASCII(), entry.element);
    DCHECK_EQ(String(entry.attribute).LowerASCII(), entry.attribute);
    table.push_back(AttributeTypeEntry{entry.element, entry.attribute,
                                       entry.element_namespace,
                                       entry.attribute_namespace, entry.type});
  }
  return table;
}

const AttributeTypeVector& GetAttributeTypeVector() {
  DEFINE_STATIC_LOCAL(AttributeTypeVector, attribute_table_,
                      (BuildAttributeVector()));
  return attribute_table_;
}

AttributeTypeVector BuildPropertyVector() {
  const struct {
    const char* element;
    const char* property;
    const char* element_namespace;
    SpecificTrustedType type;
  } kTypeTable[] = {
      {"embed", "src", kHtmlNamespace, SpecificTrustedType::kScriptURL},
      {"iframe", "srcdoc", kHtmlNamespace, SpecificTrustedType::kHTML},
      {"object", "codeBase", kHtmlNamespace, SpecificTrustedType::kScriptURL},
      {"object", "data", kHtmlNamespace, SpecificTrustedType::kScriptURL},
      {"script", "innerText", kHtmlNamespace, SpecificTrustedType::kScript},
      {"script", "src", kHtmlNamespace, SpecificTrustedType::kScriptURL},
      {"script", "text", kHtmlNamespace, SpecificTrustedType::kScript},
      {"script", "textContent", kHtmlNamespace, SpecificTrustedType::kScript},
      {"*", "innerHTML", nullptr, SpecificTrustedType::kHTML},
      {"*", "outerHTML", nullptr, SpecificTrustedType::kHTML},
  };
  AttributeTypeVector table;
  for (const auto& entry : kTypeTable) {
    // Elements are case-insensitive, but property names are not.
    // Properties don't have a namespace, so we're leaving that blank.
    DCHECK_EQ(String(entry.element).LowerASCII(), entry.element);
    table.push_back(AttributeTypeEntry{entry.element, entry.property,
                                       entry.element_namespace, AtomicString(),
                                       entry.type});
  }
  return table;
}

const AttributeTypeVector& GetPropertyTypeVector() {
  DEFINE_STATIC_LOCAL(AttributeTypeVector, property_table_,
                      (BuildPropertyVector()));
  return property_table_;
}

// Find a matching entry in an AttributeTypeVector. Assumes that argument
// normalization has already happened.
SpecificTrustedType FindEntryInAttributeTypeVector(
    const AttributeTypeVector& attribute_type_vector,
    const AtomicString& element,
    const AtomicString& attribute,
    const AtomicString& element_namespace,
    const AtomicString& attribute_namespace) {
  for (const auto& entry : attribute_type_vector) {
    bool entry_matches = ((entry.element == element &&
                           entry.element_namespace == element_namespace) ||
                          entry.element == "*") &&
                         entry.attribute == attribute &&
                         entry.attribute_namespace == attribute_namespace;
    if (entry_matches)
      return entry.type;
  }
  return SpecificTrustedType::kNone;
}

// Find a matching entry in an AttributeTypeVector. Converts arguments to
// AtomicString and does spec-mandated mapping of empty strings as namespaces.
SpecificTrustedType FindEntryInAttributeTypeVector(
    const AttributeTypeVector& attribute_type_vector,
    const String& element,
    const String& attribute,
    const String& element_namespace,
    const String& attribute_namespace) {
  return FindEntryInAttributeTypeVector(
      attribute_type_vector, AtomicString(element), AtomicString(attribute),
      element_namespace.empty() ? AtomicString(kHtmlNamespace)
                                : AtomicString(element_namespace),
      attribute_namespace.empty() ? AtomicString()
                                  : AtomicString(attribute_namespace));
}

}  // anonymous namespace

TrustedTypePolicy* TrustedTypePolicyFactory::createPolicy(
    const String& policy_name,
    ExceptionState& exception_state) {
  return createPolicy(policy_name,
                      MakeGarbageCollected<TrustedTypePolicyOptions>(),
                      exception_state);
}

TrustedTypePolicy* TrustedTypePolicyFactory::createPolicy(
    const String& policy_name,
    const TrustedTypePolicyOptions* policy_options,
    ExceptionState& exception_state) {
  if (RuntimeEnabledFeatures::TrustedTypeBeforePolicyCreationEventEnabled()) {
    DispatchEventResult result =
        DispatchEvent(*BeforeCreatePolicyEvent::Create(policy_name));
    if (result != DispatchEventResult::kNotCanceled) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kNotAllowedError,
          "The policy creation has been canceled.");
      return nullptr;
    }
  }
  if (!GetExecutionContext()) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "The document is detached.");
    return nullptr;
  }
  UseCounter::Count(GetExecutionContext(),
                    WebFeature::kTrustedTypesCreatePolicy);

  // TT requires two validity checks: One against the CSP, and one for the
  // default policy. Use |disallowed| (and |violation_details|) to aggregate
  // these, so we can have unified error handling.
  //
  // Spec ref:
  // https://www.w3.org/TR/2022/WD-trusted-types-20220927/#create-trusted-type-policy-algorithm,
  // steps 2 + 3
  bool disallowed = false;
  ContentSecurityPolicy::AllowTrustedTypePolicyDetails violation_details =
      ContentSecurityPolicy::AllowTrustedTypePolicyDetails::kAllowed;

  // This issue_id is used to generate a link in the DevTools front-end from
  // the JavaScript TypeError to the inspector issue which is reported by
  // ContentSecurityPolicy::ReportViolation via the call to
  // AllowTrustedTypeAssignmentFailure below.
  base::UnguessableToken issue_id = base::UnguessableToken::Create();

  if (GetExecutionContext()->GetContentSecurityPolicy()) {
    disallowed = !GetExecutionContext()
                      ->GetContentSecurityPolicy()
                      ->AllowTrustedTypePolicy(
                          policy_name, policy_map_.Contains(policy_name),
                          violation_details, issue_id);
  }
  if (!disallowed && policy_name == "default" &&
      policy_map_.Contains("default")) {
    disallowed = true;
    violation_details = ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
        kDisallowedDuplicateName;
  }

  if (violation_details != ContentSecurityPolicy::ContentSecurityPolicy::
                               AllowTrustedTypePolicyDetails::kAllowed) {
    // We may report a violation here even when disallowed is false
    // in case policy is a report-only one.
    probe::OnContentSecurityPolicyViolation(
        GetExecutionContext(),
        ContentSecurityPolicyViolationType::kTrustedTypesPolicyViolation);
  }
  if (disallowed) {
    // For a better error message, we'd like to disambiguate between
    // "disallowed" and "disallowed because of a duplicate name".
    bool disallowed_because_of_duplicate_name =
        violation_details ==
        ContentSecurityPolicy::AllowTrustedTypePolicyDetails::
            kDisallowedDuplicateName;
    const String message =
        disallowed_because_of_duplicate_name
            ? "Policy with name \"" + policy_name + "\" already exists."
            : "Policy \"" + policy_name + "\" disallowed.";
    exception_state.ThrowTypeError(message);
    MaybeAssociateExceptionMetaData(exception_state, "issueId",
                                    IdentifiersFactory::IdFromToken(issue_id));
    return nullptr;
  }

  UseCounter::Count(GetExecutionContext(),
                    WebFeature::kTrustedTypesPolicyCreated);
  if (policy_name == "default") {
    DCHECK(!policy_map_.Contains("default"));
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kTrustedTypesDefaultPolicyCreated);
  }

  auto* policy = MakeGarbageCollected<TrustedTypePolicy>(
      policy_name, const_cast<TrustedTypePolicyOptions*>(policy_options));
  policy_map_.insert(policy_name, policy);
  return policy;
}

TrustedTypePolicy* TrustedTypePolicyFactory::defaultPolicy() const {
  const auto iter = policy_map_.find("default");
  return iter != policy_map_.end() ? iter->value : nullptr;
}

TrustedTypePolicyFactory::TrustedTypePolicyFactory(ExecutionContext* context)
    : ExecutionContextClient(context),
      empty_html_(MakeGarbageCollected<TrustedHTML>("")),
      empty_script_(MakeGarbageCollected<TrustedScript>("")) {}

const WrapperTypeInfo*
TrustedTypePolicyFactory::GetWrapperTypeInfoFromScriptValue(
    ScriptState* script_state,
    const ScriptValue& script_value) {
  v8::Local<v8::Value> value = script_value.V8Value();
  if (value.IsEmpty() || !value->IsObject() ||
      !V8DOMWrapper::IsWrapper(script_state->GetIsolate(), value))
    return nullptr;
  v8::Local<v8::Object> object = value.As<v8::Object>();
  return ToWrapperTypeInfo(object);
}

bool TrustedTypePolicyFactory::isHTML(ScriptState* script_state,
                                      const ScriptValue& script_value) {
  const WrapperTypeInfo* wrapper_type_info =
      GetWrapperTypeInfoFromScriptValue(script_state, script_value);
  return wrapper_type_info &&
         wrapper_type_info->Equals(V8TrustedHTML::GetWrapperTypeInfo());
}

bool TrustedTypePolicyFactory::isScript(ScriptState* script_state,
                                        const ScriptValue& script_value) {
  const WrapperTypeInfo* wrapper_type_info =
      GetWrapperTypeInfoFromScriptValue(script_state, script_value);
  return wrapper_type_info &&
         wrapper_type_info->Equals(V8TrustedScript::GetWrapperTypeInfo());
}

bool TrustedTypePolicyFactory::isScriptURL(ScriptState* script_state,
                                           const ScriptValue& script_value) {
  const WrapperTypeInfo* wrapper_type_info =
      GetWrapperTypeInfoFromScriptValue(script_state, script_value);
  return wrapper_type_info &&
         wrapper_type_info->Equals(V8TrustedScriptURL::GetWrapperTypeInfo());
}

TrustedHTML* TrustedTypePolicyFactory::emptyHTML() const {
  return empty_html_.Get();
}

TrustedScript* TrustedTypePolicyFactory::emptyScript() const {
  return empty_script_.Get();
}

String getTrustedTypeName(SpecificTrustedType type) {
  switch (type) {
    case SpecificTrustedType::kHTML:
      return "TrustedHTML";
    case SpecificTrustedType::kScript:
      return "TrustedScript";
    case SpecificTrustedType::kScriptURL:
      return "TrustedScriptURL";
    case SpecificTrustedType::kNone:
      return String();
  }
}

String TrustedTypePolicyFactory::getPropertyType(
    const String& tagName,
    const String& propertyName,
    const String& elementNS) const {
  return getTrustedTypeName(FindEntryInAttributeTypeVector(
      GetPropertyTypeVector(), tagName.LowerASCII(), propertyName, elementNS,
      String()));
}

String TrustedTypePolicyFactory::getAttributeType(
    const String& tagName,
    const String& attributeName,
    const String& tagNS,
    const String& attributeNS) const {
  return getTrustedTypeName(FindEntryInAttributeTypeVector(
      GetAttributeTypeVector(), tagName.LowerASCII(),
      attributeName.LowerASCII(), tagNS, attributeNS));
}

ScriptValue TrustedTypePolicyFactory::getTypeMapping(
    ScriptState* script_state) const {
  return getTypeMapping(script_state, String());
}

namespace {

// Support method for getTypeMapping: Ensure that top has a an element and
// attributes or properties entry.
// E.g. {element: { "attributes": {}}
void EnsureAttributeAndPropertiesDict(
    ScriptState* script_state,
    v8::Local<v8::Object>& top,
    const v8::Local<v8::String>& element,
    const v8::Local<v8::String>& attributes_or_properties) {
  if (!top->Has(script_state->GetContext(), element).ToChecked()) {
    top->Set(script_state->GetContext(), element,
             v8::Object::New(script_state->GetIsolate()))
        .Check();
  }
  v8::Local<v8::Object> middle = top->Get(script_state->GetContext(), element)
                                     .ToLocalChecked()
                                     ->ToObject(script_state->GetContext())
                                     .ToLocalChecked();
  if (!middle->Has(script_state->GetContext(), attributes_or_properties)
           .ToChecked()) {
    middle
        ->Set(script_state->GetContext(), attributes_or_properties,
              v8::Object::New(script_state->GetIsolate()))
        .Check();
  }
}

// Support method for getTypeMapping: Iterate over AttributeTypeVector and
// fill in the map entries.
void PopulateTypeMapping(
    ScriptState* script_state,
    v8::Local<v8::Object>& top,
    const AttributeTypeVector& attribute_vector,
    const v8::Local<v8::String>& attributes_or_properties) {
  for (const auto& iter : attribute_vector) {
    v8::Local<v8::String> element =
        V8String(script_state->GetIsolate(), iter.element);
    EnsureAttributeAndPropertiesDict(script_state, top, element,
                                     attributes_or_properties);
    top->Get(script_state->GetContext(), element)
        .ToLocalChecked()
        ->ToObject(script_state->GetContext())
        .ToLocalChecked()
        ->Get(script_state->GetContext(), attributes_or_properties)
        .ToLocalChecked()
        ->ToObject(script_state->GetContext())
        .ToLocalChecked()
        ->Set(
            script_state->GetContext(),
            V8String(script_state->GetIsolate(), iter.attribute),
            V8String(script_state->GetIsolate(), getTrustedTypeName(iter.type)))
        .Check();
  }
}

}  // anonymous namespace

ScriptValue TrustedTypePolicyFactory::getTypeMapping(ScriptState* script_state,
                                                     const String& ns) const {
  // Create three-deep dictionary of properties, like so:
  // {tagname: { ["attributes"|"properties"]: { attribute: type }}}

  if (!ns.empty())
    return ScriptValue::CreateNull(script_state->GetIsolate());

  v8::HandleScope handle_scope(script_state->GetIsolate());
  v8::Local<v8::Object> top = v8::Object::New(script_state->GetIsolate());
  PopulateTypeMapping(script_state, top, GetAttributeTypeVector(),
                      V8String(script_state->GetIsolate(), "attributes"));
  PopulateTypeMapping(script_state, top, GetPropertyTypeVector(),
                      V8String(script_state->GetIsolate(), "properties"));
  return ScriptValue(script_state->GetIsolate(), top);
}

void TrustedTypePolicyFactory::CountTrustedTypeAssignmentError() {
  if (!hadAssignmentError) {
    UseCounter::Count(GetExecutionContext(),
                      WebFeature::kTrustedTypesAssignmentError);
    hadAssignmentError = true;
  }
}

const AtomicString& TrustedTypePolicyFactory::InterfaceName() const {
  return event_target_names::kTrustedTypePolicyFactory;
}

ExecutionContext* TrustedTypePolicyFactory::GetExecutionContext() const {
  return ExecutionContextClient::GetExecutionContext();
}

void TrustedTypePolicyFactory::Trace(Visitor* visitor) const {
  EventTargetWithInlineData::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
  visitor->Trace(empty_html_);
  visitor->Trace(empty_script_);
  visitor->Trace(policy_map_);
}

inline bool FindEventHandlerAttributeInTable(
    const AtomicString& attributeName) {
  return SpecificTrustedType::kScript ==
         FindEntryInAttributeTypeVector(GetAttributeTypeVector(),
                                        AtomicString(), attributeName,
                                        AtomicString(), AtomicString());
}

bool TrustedTypePolicyFactory::IsEventHandlerAttributeName(
    const AtomicString& attributeName) {
  // Check that the "on" prefix indeed filters out only non-event handlers.
  DCHECK(!FindEventHandlerAttributeInTable(attributeName) ||
         attributeName.StartsWithIgnoringASCIICase("on"));

  return attributeName.StartsWithIgnoringASCIICase("on") &&
         FindEventHandlerAttributeInTable(attributeName);
}

}  // namespace blink
