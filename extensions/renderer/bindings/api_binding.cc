// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/api_binding.h"

#include <algorithm>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "extensions/renderer/bindings/api_binding_hooks.h"
#include "extensions/renderer/bindings/api_binding_types.h"
#include "extensions/renderer/bindings/api_binding_util.h"
#include "extensions/renderer/bindings/api_event_handler.h"
#include "extensions/renderer/bindings/api_invocation_errors.h"
#include "extensions/renderer/bindings/api_request_handler.h"
#include "extensions/renderer/bindings/api_signature.h"
#include "extensions/renderer/bindings/api_type_reference_map.h"
#include "extensions/renderer/bindings/binding_access_checker.h"
#include "extensions/renderer/bindings/declarative_event.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/per_context_data.h"

namespace extensions {

namespace {

// Returns the name of the enum value for use in JavaScript; JS enum entries use
// SCREAMING_STYLE.
std::string GetJSEnumEntryName(const std::string& original) {
  // The webstorePrivate API has an empty enum value for a result.
  // TODO(devlin): Work with the webstore team to see if we can move them off
  // this - they also already have a "success" result that they can use.
  // See crbug.com/709120.
  if (original.empty())
    return original;

  std::string result;
  // If the original starts with a digit, prefix it with an underscore.
  if (base::IsAsciiDigit(original[0]))
    result.push_back('_');
  // Given 'myEnum-Foo':
  for (size_t i = 0; i < original.size(); ++i) {
    // Add an underscore between camelcased items:
    // 'myEnum-Foo' -> 'mY_Enum-Foo'
    if (i > 0 && base::IsAsciiLower(original[i - 1]) &&
        base::IsAsciiUpper(original[i])) {
      result.push_back('_');
      result.push_back(original[i]);
    } else if (original[i] == '-') {  // 'mY_Enum-Foo' -> 'mY_Enum_Foo'
      result.push_back('_');
    } else {  // 'mY_Enum_Foo' -> 'MY_ENUM_FOO'
      result.push_back(base::ToUpperASCII(original[i]));
    }
  }
  return result;
}

std::unique_ptr<APISignature> GetAPISignatureFromDictionary(
    const base::Value::Dict* dict,
    BindingAccessChecker* access_checker,
    const std::string& api_name) {
  const base::Value* params = dict->Find("parameters");
  if (params && !params->is_list())
    params = nullptr;
  CHECK(params);

  const base::Value* returns_async = dict->Find("returns_async");
  if (returns_async && !returns_async->is_dict())
    returns_async = nullptr;

  return APISignature::CreateFromValues(*params, returns_async, access_checker,
                                        api_name, false /*is_event_signature*/);
}

void RunAPIBindingHandlerCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  gin::Arguments args(info);
  if (!binding::IsContextValidOrThrowError(args.isolate()->GetCurrentContext()))
    return;

  v8::Local<v8::External> external;
  CHECK(args.GetData(&external));
  auto* callback = static_cast<APIBinding::HandlerCallback*>(external->Value());

  callback->Run(&args);
}

}  // namespace

struct APIBinding::MethodData {
  MethodData(std::string full_name, const APISignature* signature)
      : full_name(std::move(full_name)), signature(signature) {}

  // The fully-qualified name of this api (e.g. runtime.sendMessage instead of
  // sendMessage).
  const std::string full_name;
  // The expected API signature.
  const APISignature* signature;
  // The callback used by the v8 function.
  APIBinding::HandlerCallback callback;
};

// TODO(devlin): Maybe separate EventData into two classes? Rules, actions, and
// conditions should never be present on vanilla events.
struct APIBinding::EventData {
  EventData(std::string exposed_name,
            std::string full_name,
            bool supports_filters,
            bool supports_rules,
            bool supports_lazy_listeners,
            int max_listeners,
            bool notify_on_change,
            std::vector<std::string> actions,
            std::vector<std::string> conditions,
            APIBinding* binding)
      : exposed_name(std::move(exposed_name)),
        full_name(std::move(full_name)),
        supports_filters(supports_filters),
        supports_rules(supports_rules),
        supports_lazy_listeners(supports_lazy_listeners),
        max_listeners(max_listeners),
        notify_on_change(notify_on_change),
        actions(std::move(actions)),
        conditions(std::move(conditions)),
        binding(binding) {}

  // The name of the event on the API object (e.g. onCreated).
  std::string exposed_name;

  // The fully-specified name of the event (e.g. tabs.onCreated).
  std::string full_name;

  // Whether the event supports filters.
  bool supports_filters;

  // Whether the event supports rules.
  bool supports_rules;

  // Whether the event supports lazy listeners.
  bool supports_lazy_listeners;

  // The maximum number of listeners this event supports.
  int max_listeners;

  // Whether to notify the browser of listener changes.
  bool notify_on_change;

  // The associated actions and conditions for declarative events.
  std::vector<std::string> actions;
  std::vector<std::string> conditions;

  // The associated APIBinding. This raw pointer is safe because the
  // EventData is only accessed from the callbacks associated with the
  // APIBinding, and both the APIBinding and APIEventHandler are owned by the
  // same object (the APIBindingsSystem).
  APIBinding* binding;
};

struct APIBinding::CustomPropertyData {
  CustomPropertyData(const std::string& type_name,
                     const std::string& property_name,
                     const base::Value::List* property_values,
                     const CreateCustomType& create_custom_type)
      : type_name(type_name),
        property_name(property_name),
        property_values(property_values),
        create_custom_type(create_custom_type) {}

  // The type of the property, e.g. 'storage.StorageArea'.
  std::string type_name;
  // The name of the property on the object, e.g. 'local' for
  // chrome.storage.local.
  std::string property_name;
  // Values curried into this particular type from the schema.
  const base::Value::List* property_values;

  CreateCustomType create_custom_type;
};

APIBinding::APIBinding(const std::string& api_name,
                       const base::Value::List* function_definitions,
                       const base::Value::List* type_definitions,
                       const base::Value::List* event_definitions,
                       const base::Value::Dict* property_definitions,
                       CreateCustomType create_custom_type,
                       OnSilentRequest on_silent_request,
                       std::unique_ptr<APIBindingHooks> binding_hooks,
                       APITypeReferenceMap* type_refs,
                       APIRequestHandler* request_handler,
                       APIEventHandler* event_handler,
                       BindingAccessChecker* access_checker)
    : api_name_(api_name),
      property_definitions_(property_definitions),
      create_custom_type_(std::move(create_custom_type)),
      on_silent_request_(std::move(on_silent_request)),
      binding_hooks_(std::move(binding_hooks)),
      type_refs_(type_refs),
      request_handler_(request_handler),
      event_handler_(event_handler),
      access_checker_(access_checker) {
  // TODO(devlin): It might make sense to instantiate the object_template_
  // directly here, which would avoid the need to hold on to
  // |property_definitions_| and |enums_|. However, there are *some* cases where
  // we don't immediately stamp out an API from the template following
  // construction.

  if (function_definitions) {
    for (const auto& func : *function_definitions) {
      const base::Value::Dict* func_dict = func.GetIfDict();
      CHECK(func_dict);
      const std::string* name = func_dict->FindString("name");
      CHECK(name);
      std::string full_name =
          base::StringPrintf("%s.%s", api_name_.c_str(), name->c_str());

      auto signature =
          GetAPISignatureFromDictionary(func_dict, access_checker, full_name);

      methods_[*name] =
          std::make_unique<MethodData>(full_name, signature.get());
      type_refs->AddAPIMethodSignature(full_name, std::move(signature));
    }
  }

  if (type_definitions) {
    for (const auto& type : *type_definitions) {
      const base::Value::Dict* type_dict = type.GetIfDict();
      CHECK(type_dict);
      const std::string* id = type_dict->FindString("id");
      CHECK(id);
      auto argument_spec = std::make_unique<ArgumentSpec>(type);
      const std::set<std::string>& enum_values = argument_spec->enum_values();
      if (!enum_values.empty()) {
        // Type names may be prefixed by the api name. If so, remove the prefix.
        absl::optional<std::string> stripped_id;
        if (base::StartsWith(*id, api_name_, base::CompareCase::SENSITIVE))
          stripped_id =
              id->substr(api_name_.size() + 1);  // +1 for trailing '.'
        std::vector<EnumEntry>& entries =
            enums_[stripped_id ? *stripped_id : *id];
        entries.reserve(enum_values.size());
        for (const auto& enum_value : enum_values) {
          entries.push_back(
              std::make_pair(enum_value, GetJSEnumEntryName(enum_value)));
        }
      }
      type_refs->AddSpec(*id, std::move(argument_spec));
      // Some types, like storage.StorageArea, have functions associated with
      // them. Cache the function signatures in the type map.
      const base::Value::List* type_functions =
          type_dict->FindList("functions");
      if (type_functions) {
        for (const auto& func : *type_functions) {
          const base::Value::Dict* func_dict = func.GetIfDict();
          CHECK(func_dict);
          const std::string* function_name = func_dict->FindString("name");
          CHECK(function_name);
          std::string full_name =
              base::StringPrintf("%s.%s", id->c_str(), function_name->c_str());

          auto signature = GetAPISignatureFromDictionary(
              func_dict, access_checker, full_name);

          type_refs->AddTypeMethodSignature(full_name, std::move(signature));
        }
      }
    }
  }

  if (event_definitions) {
    events_.reserve(event_definitions->size());
    for (const auto& event : *event_definitions) {
      const base::Value::Dict* event_dict = event.GetIfDict();
      CHECK(event_dict);
      const std::string* name = event_dict->FindString("name");
      CHECK(name);
      std::string full_name =
          base::StringPrintf("%s.%s", api_name_.c_str(), name->c_str());
      const base::Value::List* filters = event_dict->FindList("filters");
      bool supports_filters = filters && !filters->empty();

      std::vector<std::string> rule_actions;
      std::vector<std::string> rule_conditions;
      const base::Value::Dict* options = event_dict->FindDict("options");
      bool supports_rules = false;
      bool notify_on_change = true;
      bool supports_lazy_listeners = true;
      int max_listeners = binding::kNoListenerMax;
      if (options) {
        // TODO(devlin): For some reason, schemas indicate supporting filters
        // either through having a 'filters' property *or* through having
        // a 'supportsFilters' property. We should clean that up.
        supports_filters |=
            options->FindBool("supportsFilters").value_or(false);
        supports_rules = options->FindBool("supportsRules").value_or(false);
        if (supports_rules) {
          absl::optional<bool> supports_listeners =
              options->FindBool("supportsListeners");
          DCHECK(supports_listeners);
          DCHECK(!*supports_listeners)
              << "Events cannot support rules and listeners.";
          auto get_values = [options](base::StringPiece name,
                                      std::vector<std::string>* out_value) {
            const base::Value::List* list = options->FindList(name);
            CHECK(list);
            for (const auto& entry : *list) {
              DCHECK(entry.is_string());
              out_value->push_back(entry.GetString());
            }
          };
          get_values("actions", &rule_actions);
          get_values("conditions", &rule_conditions);
        }

        absl::optional<int> max_listeners_option =
            options->FindInt("maxListeners");
        if (max_listeners_option)
          max_listeners = *max_listeners_option;
        absl::optional<bool> unmanaged = options->FindBool("unmanaged");
        if (unmanaged)
          notify_on_change = !*unmanaged;

        absl::optional<bool> supports_lazy_listeners_value =
            options->FindBool("supportsLazyListeners");
        if (supports_lazy_listeners_value) {
          supports_lazy_listeners = *supports_lazy_listeners_value;
          DCHECK(!supports_lazy_listeners)
              << "Don't specify supportsLazyListeners: true; it's the default.";
        }
      }

      if (binding::IsResponseValidationEnabled()) {
        const base::Value* params = event_dict->Find("parameters");
        if (params && !params->is_list())
          params = nullptr;
        // NOTE: At least in tests, events may omit "parameters". It's unclear
        // if real schemas do, too. For now, sub in an empty list if necessary.
        // TODO(devlin): Track this down and CHECK(params).
        base::Value empty_params(base::Value::Type::LIST);
        std::unique_ptr<APISignature> event_signature =
            APISignature::CreateFromValues(
                params ? *params : empty_params, nullptr /*returns_async*/,
                access_checker, *name, true /*is_event_signature*/);
        DCHECK(!event_signature->has_async_return());
        type_refs_->AddEventSignature(full_name, std::move(event_signature));
      }

      events_.push_back(std::make_unique<EventData>(
          *name, std::move(full_name), supports_filters, supports_rules,
          supports_lazy_listeners, max_listeners, notify_on_change,
          std::move(rule_actions), std::move(rule_conditions), this));
    }
  }
}

APIBinding::~APIBinding() = default;

v8::Local<v8::Object> APIBinding::CreateInstance(
    v8::Local<v8::Context> context) {
  DCHECK(binding::IsContextValid(context));
  v8::Isolate* isolate = context->GetIsolate();
  if (object_template_.IsEmpty())
    InitializeTemplate(isolate);
  DCHECK(!object_template_.IsEmpty());

  v8::Local<v8::Object> object =
      object_template_.Get(isolate)->NewInstance(context).ToLocalChecked();

  // The object created from the template includes all methods, but some may
  // be unavailable in this context. Iterate over them and delete any that
  // aren't available.
  // TODO(devlin): Ideally, we'd only do this check on the methods that are
  // conditionally exposed. Or, we could have multiple templates for different
  // configurations, assuming there are a small number of possibilities.
  for (const auto& key_value : methods_) {
    if (!access_checker_->HasAccess(context, key_value.second->full_name)) {
      v8::Maybe<bool> success = object->Delete(
          context, gin::StringToSymbol(isolate, key_value.first));
      CHECK(success.IsJust());
      CHECK(success.FromJust());
    }
  }
  for (const auto& event : events_) {
    if (!access_checker_->HasAccess(context, event->full_name)) {
      v8::Maybe<bool> success = object->Delete(
          context, gin::StringToSymbol(isolate, event->exposed_name));
      CHECK(success.IsJust());
      CHECK(success.FromJust());
    }
  }
  for (const auto& property : root_properties_) {
    std::string full_name = base::StrCat({api_name_, ".", property});
    if (!access_checker_->HasAccess(context, full_name)) {
      v8::Maybe<bool> success =
          object->Delete(context, gin::StringToSymbol(isolate, property));
      CHECK(success.IsJust());
      CHECK(success.FromJust());
    }
  }

  binding_hooks_->InitializeInstance(context, object);

  return object;
}

void APIBinding::InitializeTemplate(v8::Isolate* isolate) {
  DCHECK(object_template_.IsEmpty());
  v8::Local<v8::ObjectTemplate> object_template =
      v8::ObjectTemplate::New(isolate);

  for (const auto& key_value : methods_) {
    MethodData& method = *key_value.second;
    DCHECK(method.callback.is_null());
    method.callback =
        base::BindRepeating(&APIBinding::HandleCall, weak_factory_.GetWeakPtr(),
                            method.full_name, method.signature);

    object_template->Set(
        gin::StringToSymbol(isolate, key_value.first),
        v8::FunctionTemplate::New(isolate, &RunAPIBindingHandlerCallback,
                                  v8::External::New(isolate, &method.callback),
                                  v8::Local<v8::Signature>(), 0,
                                  v8::ConstructorBehavior::kThrow));
  }

  for (const auto& event : events_) {
    object_template->SetLazyDataProperty(
        gin::StringToSymbol(isolate, event->exposed_name),
        &APIBinding::GetEventObject, v8::External::New(isolate, event.get()));
  }

  for (const auto& entry : enums_) {
    v8::Local<v8::ObjectTemplate> enum_object =
        v8::ObjectTemplate::New(isolate);
    for (const auto& enum_entry : entry.second) {
      enum_object->Set(gin::StringToSymbol(isolate, enum_entry.second),
                       gin::StringToSymbol(isolate, enum_entry.first));
    }
    object_template->Set(isolate, entry.first.c_str(), enum_object);
  }

  if (property_definitions_) {
    DecorateTemplateWithProperties(isolate, object_template,
                                   *property_definitions_, /*is_root=*/true);
  }

  // Allow custom bindings a chance to tweak the template, such as to add
  // additional properties or types.
  binding_hooks_->InitializeTemplate(isolate, object_template, *type_refs_);

  object_template_.Set(isolate, object_template);
}

void APIBinding::DecorateTemplateWithProperties(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate> object_template,
    const base::Value::Dict& properties,
    bool is_root) {
  static const char kValueKey[] = "value";
  for (auto item : properties) {
    const base::Value::Dict* dict = item.second.GetIfDict();
    CHECK(dict);
    if (dict->FindBool("optional")) {
      // TODO(devlin): What does optional even mean here? It's only used, it
      // seems, for lastError and inIncognitoContext, which are both handled
      // with custom bindings. Investigate, and remove.
      continue;
    }

    const base::Value::List* platforms = dict->FindList("platforms");
    // TODO(devlin): Availability should be specified in the features files,
    // not the API schema files.
    if (platforms) {
      std::string this_platform = binding::GetPlatformString();
      auto is_this_platform = [&this_platform](const base::Value& platform) {
        return platform.is_string() && platform.GetString() == this_platform;
      };
      if (base::ranges::none_of(*platforms, is_this_platform))
        continue;
    }

    v8::Local<v8::String> v8_key = gin::StringToSymbol(isolate, item.first);
    const std::string* ref = dict->FindString("$ref");
    if (ref) {
      const base::Value::List* property_values = dict->FindList("value");
      CHECK(property_values);
      auto property_data = std::make_unique<CustomPropertyData>(
          *ref, item.first, property_values, create_custom_type_);
      object_template->SetLazyDataProperty(
          v8_key, &APIBinding::GetCustomPropertyObject,
          v8::External::New(isolate, property_data.get()));
      custom_properties_.push_back(std::move(property_data));
      if (is_root)
        root_properties_.insert(item.first);
      continue;
    }

    const std::string* type = dict->FindString("type");
    CHECK(type);
    if (*type != "object" && !dict->Find(kValueKey)) {
      // TODO(devlin): What does a fundamental property not having a value mean?
      // It doesn't seem useful, and looks like it's only used by runtime.id,
      // which is set by custom bindings. Investigate, and remove.
      continue;
    }
    if (*type == "integer") {
      absl::optional<int> val = dict->FindInt(kValueKey);
      CHECK(val);
      object_template->Set(v8_key, v8::Integer::New(isolate, *val));
    } else if (*type == "boolean") {
      absl::optional<bool> val = dict->FindBool(kValueKey);
      CHECK(val);
      object_template->Set(v8_key, v8::Boolean::New(isolate, *val));
    } else if (*type == "string") {
      const std::string* val = dict->FindString(kValueKey);
      CHECK(val) << item.first;
      object_template->Set(v8_key, gin::StringToSymbol(isolate, *val));
    } else if (*type == "object" || !ref->empty()) {
      v8::Local<v8::ObjectTemplate> property_template =
          v8::ObjectTemplate::New(isolate);
      const base::Value::Dict* property_dict = dict->FindDict("properties");
      CHECK(property_dict);
      DecorateTemplateWithProperties(isolate, property_template, *property_dict,
                                     /*is_root=*/false);
      object_template->Set(v8_key, property_template);
    }
    if (is_root)
      root_properties_.insert(item.first);
  }
}

// static
bool APIBinding::enable_promise_support_for_testing = false;

// static
void APIBinding::GetEventObject(
    v8::Local<v8::Name> property,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context;
  if (!info.Holder()->GetCreationContext().ToLocal(&context) ||
      !binding::IsContextValidOrThrowError(context))
    return;

  CHECK(info.Data()->IsExternal());
  auto* event_data =
      static_cast<EventData*>(info.Data().As<v8::External>()->Value());
  v8::Local<v8::Value> retval;
  if (event_data->binding->binding_hooks_->CreateCustomEvent(
          context, event_data->full_name, &retval)) {
    // A custom event was created; our work is done.
  } else if (event_data->supports_rules) {
    gin::Handle<DeclarativeEvent> event = gin::CreateHandle(
        isolate, new DeclarativeEvent(
                     event_data->full_name, event_data->binding->type_refs_,
                     event_data->binding->request_handler_, event_data->actions,
                     event_data->conditions, 0));
    retval = event.ToV8();
  } else {
    retval = event_data->binding->event_handler_->CreateEventInstance(
        event_data->full_name, event_data->supports_filters,
        event_data->supports_lazy_listeners, event_data->max_listeners,
        event_data->notify_on_change, context);
  }
  info.GetReturnValue().Set(retval);
}

void APIBinding::GetCustomPropertyObject(
    v8::Local<v8::Name> property_name,
    const v8::PropertyCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context;
  if (!info.Holder()->GetCreationContext().ToLocal(&context) ||
      !binding::IsContextValid(context))
    return;

  v8::Context::Scope context_scope(context);
  CHECK(info.Data()->IsExternal());
  auto* property_data =
      static_cast<CustomPropertyData*>(info.Data().As<v8::External>()->Value());

  v8::Local<v8::Object> property = property_data->create_custom_type.Run(
      isolate, property_data->type_name, property_data->property_name,
      property_data->property_values);
  if (property.IsEmpty())
    return;

  info.GetReturnValue().Set(property);
}

void APIBinding::HandleCall(const std::string& name,
                            const APISignature* signature,
                            gin::Arguments* arguments) {
  std::string error;
  v8::Isolate* isolate = arguments->isolate();
  v8::HandleScope handle_scope(isolate);

  // Since this is called synchronously from the JS entry point,
  // GetCurrentContext() should always be correct.
  v8::Local<v8::Context> context = isolate->GetCurrentContext();

  if (!access_checker_->HasAccessOrThrowError(context, name)) {
    // TODO(devlin): Do we need handle this for events as well? I'm not sure the
    // currrent system does (though perhaps it should). Investigate.
    return;
  }

  std::vector<v8::Local<v8::Value>> argument_list = arguments->GetAll();

  bool invalid_invocation = false;
  v8::Local<v8::Function> custom_callback;
  binding::ResultModifierFunction result_modifier;
  bool updated_args = false;
  int old_request_id = request_handler_->last_sent_request_id();
  {
    v8::TryCatch try_catch(isolate);
    APIBindingHooks::RequestResult hooks_result = binding_hooks_->RunHooks(
        name, context, signature, &argument_list, *type_refs_);

    switch (hooks_result.code) {
      case APIBindingHooks::RequestResult::INVALID_INVOCATION:
        invalid_invocation = true;
        error = std::move(hooks_result.error);
        // Throw a type error below so that it's not caught by our try-catch.
        break;
      case APIBindingHooks::RequestResult::THROWN:
        DCHECK(try_catch.HasCaught());
        try_catch.ReThrow();
        return;
      case APIBindingHooks::RequestResult::CONTEXT_INVALIDATED:
        DCHECK(!binding::IsContextValid(context));
        // The context was invalidated during the course of running the custom
        // hooks. Bail.
        return;
      case APIBindingHooks::RequestResult::HANDLED:
        if (!hooks_result.return_value.IsEmpty())
          arguments->Return(hooks_result.return_value);

        // TODO(devlin): This is a pretty simplistic implementation of this,
        // but it's similar to the current JS logic. If we wanted to be more
        // correct, we could create a RequestScope object that watches outgoing
        // requests.
        if (old_request_id == request_handler_->last_sent_request_id())
          on_silent_request_.Run(context, name, argument_list);

        return;  // Our work here is done.
      case APIBindingHooks::RequestResult::ARGUMENTS_UPDATED:
        updated_args = true;
        [[fallthrough]];
      case APIBindingHooks::RequestResult::NOT_HANDLED:
        break;  // Handle in the default manner.
    }
    custom_callback = hooks_result.custom_callback;
    result_modifier = std::move(hooks_result.result_modifier);
  }

  if (invalid_invocation) {
    arguments->ThrowTypeError(api_errors::InvocationError(
        name, signature->GetExpectedSignature(), error));
    return;
  }

  APISignature::JSONParseResult parse_result;
  {
    v8::TryCatch try_catch(isolate);

    // If custom hooks updated the arguments post-validation, we just trust the
    // values the hooks provide and convert them directly. This is because some
    // APIs have one set of values they use for validation, and a second they
    // use in the implementation of the function (see, e.g.
    // fileSystem.getDisplayPath).
    // TODO(devlin): That's unfortunate. Not only does it require special casing
    // here, but it also means we can't auto-generate the params for the
    // function on the browser side.
    if (updated_args) {
      parse_result =
          signature->ConvertArgumentsIgnoringSchema(context, argument_list);
      // Converted arguments passed to us by our bindings should never fail.
      DCHECK(parse_result.succeeded());
    } else {
      parse_result =
          signature->ParseArgumentsToJSON(context, argument_list, *type_refs_);
    }

    if (try_catch.HasCaught()) {
      DCHECK(!parse_result.succeeded());
      try_catch.ReThrow();
      return;
    }
  }
  if (!parse_result.succeeded()) {
    arguments->ThrowTypeError(api_errors::InvocationError(
        name, signature->GetExpectedSignature(), *parse_result.error));
    return;
  }

  v8::Local<v8::Promise> promise = request_handler_->StartRequest(
      context, name, std::move(*parse_result.arguments_list),
      parse_result.async_type, parse_result.callback, custom_callback,
      std::move(result_modifier));
  if (!promise.IsEmpty())
    arguments->Return(promise);
}

}  // namespace extensions
