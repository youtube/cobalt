{##############################################################################}
{% macro generate_method(method, world_suffix) %}
{% filter conditional(method.conditional_string) %}
static void {{method.name}}{{method.overload_index}}Method{{world_suffix}}(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    {# Local variables #}
    {% if method.has_exception_state %}
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "{{method.name}}", "{{interface_name}}", info.Holder(), info.GetIsolate());
    {% endif %}
    {# Overloaded methods have length checked during overload resolution #}
    {% if method.number_of_required_arguments and not method.overload_index %}
    if (UNLIKELY(info.Length() < {{method.number_of_required_arguments}})) {
        {{throw_minimum_arity_type_error(method, method.number_of_required_arguments) | indent(8)}}
        return;
    }
    {% endif %}
    {% if not method.is_static %}
    {{cpp_class}}* impl = {{v8_class}}::toImpl(info.Holder());
    {% endif %}
    {% if method.is_custom_element_callbacks %}
    CustomElementProcessingStack::CallbackDeliveryScope deliveryScope;
    {% endif %}
    {# Security checks #}
    {% if method.is_check_security_for_window %}
    if (LocalDOMWindow* window = impl->toDOMWindow()) {
        if (!BindingSecurity::shouldAllowAccessToFrame(info.GetIsolate(), window->frame(), exceptionState)) {
            {{throw_from_exception_state(method)}};
            return;
        }
        if (!window->document())
            return;
    }
    {% elif method.is_check_security_for_frame %}
    if (!BindingSecurity::shouldAllowAccessToFrame(info.GetIsolate(), impl->frame(), exceptionState)) {
        {{throw_from_exception_state(method)}};
        return;
    }
    {% endif %}
    {% if method.is_check_security_for_node %}
    if (!BindingSecurity::shouldAllowAccessToNode(info.GetIsolate(), impl->{{method.name}}(exceptionState), exceptionState)) {
        v8SetReturnValueNull(info);
        {{throw_from_exception_state(method)}};
        return;
    }
    {% endif %}
    {# Call method #}
    {% if method.arguments %}
    {{generate_arguments(method, world_suffix) | indent}}
    {% endif %}
    {% if world_suffix %}
    {{cpp_method_call(method, method.v8_set_return_value_for_main_world, method.cpp_value) | indent}}
    {% else %}
    {{cpp_method_call(method, method.v8_set_return_value, method.cpp_value) | indent}}
    {% endif %}
}
{% endfilter %}
{% endmacro %}


{######################################}
{% macro generate_arguments(method, world_suffix) %}
{% for argument in method.arguments %}
{{generate_argument_var_declaration(argument)}};
{% endfor %}
{
    {% for argument in method.arguments %}
    {% if argument.default_value %}
    if (!info[{{argument.index}}]->IsUndefined()) {
        {{generate_argument(method, argument, world_suffix) | indent(8)}}
    } else {
        {{argument.name}} = {{argument.default_value}};
    }
    {% else %}
    {{generate_argument(method, argument, world_suffix) | indent}}
    {% endif %}
    {% endfor %}
}
{% endmacro %}


{######################################}
{% macro generate_argument_var_declaration(argument) %}
{# FIXME: remove EventListener special case #}
{% if argument.idl_type == 'EventListener' %}
RefPtr<{{argument.idl_type}}> {{argument.name}}
{%- else %}
{{argument.cpp_type}} {{argument.name}}
{%- endif %}{# argument.idl_type == 'EventListener' #}
{% endmacro %}


{######################################}
{% macro generate_argument(method, argument, world_suffix) %}
{% if argument.is_optional and not argument.has_default and
      not argument.is_dictionary and
      not argument.is_callback_interface %}
{# Optional arguments without a default value generate an early call with
   fewer arguments if they are omitted.
   Optional Dictionary arguments default to empty dictionary. #}
if (UNLIKELY(info.Length() <= {{argument.index}})) {
    {% if world_suffix %}
    {{cpp_method_call(method, argument.v8_set_return_value_for_main_world, argument.cpp_value) | indent}}
    {% else %}
    {{cpp_method_call(method, argument.v8_set_return_value, argument.cpp_value) | indent}}
    {% endif %}
    {% if argument.has_event_listener_argument %}
    {{hidden_dependency_action(method.name) | indent}}
    {% endif %}
    return;
}
{% endif %}
{% if argument.has_type_checking_interface and not argument.is_variadic_wrapper_type %}
{# Type checking for wrapper interface types (if interface not implemented,
   throw a TypeError), per http://www.w3.org/TR/WebIDL/#es-interface
   Note: for variadic arguments, the type checking is done for each matched
   argument instead; see argument.is_variadic_wrapper_type code-path below. #}
if (info.Length() > {{argument.index}} && {% if argument.is_nullable %}!isUndefinedOrNull(info[{{argument.index}}]) && {% endif %}!V8{{argument.idl_type}}::hasInstance(info[{{argument.index}}], info.GetIsolate())) {
    {{throw_type_error(method, '"parameter %s is not of type \'%s\'."' %
                               (argument.index + 1, argument.idl_type)) | indent}}
    return;
}
{% endif %}{# argument.has_type_checking_interface #}
{% if argument.is_callback_interface %}
{# FIXME: remove EventListener special case #}
{% if argument.idl_type == 'EventListener' %}
{% if method.name == 'removeEventListener' or method.name == 'removeListener' %}
{{argument.name}} = V8EventListenerList::getEventListener(ScriptState::current(info.GetIsolate()), info[{{argument.index}}], false, ListenerFindOnly);
{% else %}{# method.name == 'addEventListener' #}
{{argument.name}} = V8EventListenerList::getEventListener(ScriptState::current(info.GetIsolate()), info[{{argument.index}}], false, ListenerFindOrCreate);
{% endif %}{# method.name #}
{% else %}{# argument.idl_type == 'EventListener' #}
{# Callback functions must be functions:
   http://www.w3.org/TR/WebIDL/#es-callback-function #}
{% if argument.is_optional %}
if (!isUndefinedOrNull(info[{{argument.index}}])) {
    if (!info[{{argument.index}}]->IsFunction()) {
        {{throw_type_error(method,
              '"The callback provided as parameter %s is not a function."' %
                  (argument.index + 1)) | indent(8)}}
        return;
    }
    {{argument.name}} = V8{{argument.idl_type}}::create(v8::Handle<v8::Function>::Cast(info[{{argument.index}}]), ScriptState::current(info.GetIsolate()));
} else {
    {{argument.name}} = nullptr;
}
{% else %}{# argument.is_optional #}
if (info.Length() <= {{argument.index}} || !{% if argument.is_nullable %}(info[{{argument.index}}]->IsFunction() || info[{{argument.index}}]->IsNull()){% else %}info[{{argument.index}}]->IsFunction(){% endif %}) {
    {{throw_type_error(method,
          '"The callback provided as parameter %s is not a function."' %
              (argument.index + 1)) | indent }}
    return;
}
{{argument.name}} = {% if argument.is_nullable %}info[{{argument.index}}]->IsNull() ? nullptr : {% endif %}V8{{argument.idl_type}}::create(v8::Handle<v8::Function>::Cast(info[{{argument.index}}]), ScriptState::current(info.GetIsolate()));
{% endif %}{# argument.is_optional #}
{% endif %}{# argument.idl_type == 'EventListener' #}
{% elif argument.is_variadic_wrapper_type %}
for (int i = {{argument.index}}; i < info.Length(); ++i) {
    if (!V8{{argument.idl_type}}::hasInstance(info[i], info.GetIsolate())) {
        {{throw_type_error(method, '"parameter %s is not of type \'%s\'."' %
                                   (argument.index + 1, argument.idl_type)) | indent(8)}}
        return;
    }
    {{argument.name}}.append(V8{{argument.idl_type}}::toImpl(v8::Handle<v8::Object>::Cast(info[i])));
}
{% elif argument.is_dictionary %}
{# Dictionaries must have type Undefined, Null or Object:
http://heycam.github.io/webidl/#es-dictionary #}
if (!isUndefinedOrNull(info[{{argument.index}}]) && !info[{{argument.index}}]->IsObject()) {
    {{throw_type_error(method, '"parameter %s (\'%s\') is not an object."' %
                               (argument.index + 1, argument.name)) | indent}}
    return;
}
{{argument.v8_value_to_local_cpp_value}};
{% else %}{# argument.is_nullable #}
{{argument.v8_value_to_local_cpp_value}};
{% endif %}{# argument.is_nullable #}
{# Type checking, possibly throw a TypeError, per:
   http://www.w3.org/TR/WebIDL/#es-type-mapping #}
{% if argument.has_type_checking_unrestricted %}
{# Non-finite floating point values (NaN, +Infinity or −Infinity), per:
   http://heycam.github.io/webidl/#es-float
   http://heycam.github.io/webidl/#es-double #}
if (!std::isfinite({{argument.name}})) {
    {{throw_type_error(method, '"%s parameter %s is non-finite."' %
                               (argument.idl_type, argument.index + 1)) | indent}}
    return;
}
{% elif argument.enum_validation_expression %}
{# Invalid enum values: http://www.w3.org/TR/WebIDL/#idl-enums #}
String string = {{argument.name}};
if (!({{argument.enum_validation_expression}})) {
    {{throw_type_error(method,
          '"parameter %s (\'" + string + "\') is not a valid enum value."' %
              (argument.index + 1)) | indent}}
    return;
}
{% elif argument.idl_type == 'Promise' %}
{# We require this for our implementation of promises, though not in spec:
http://heycam.github.io/webidl/#es-promise #}
if (!{{argument.name}}.isUndefinedOrNull() && !{{argument.name}}.isObject()) {
    {{throw_type_error(method, '"parameter %s (\'%s\') is not an object."' %
                               (argument.index + 1, argument.name)) | indent}}
    return;
}
{% endif %}
{% endmacro %}


{######################################}
{% macro cpp_method_call(method, v8_set_return_value, cpp_value) %}
{# Local variables #}
{% if method.is_call_with_script_state %}
{# [CallWith=ScriptState] #}
ScriptState* scriptState = ScriptState::current(info.GetIsolate());
{% endif %}
{% if method.is_call_with_execution_context %}
{# [ConstructorCallWith=ExecutionContext] #}
{# [CallWith=ExecutionContext] #}
ExecutionContext* executionContext = currentExecutionContext(info.GetIsolate());
{% endif %}
{% if method.is_call_with_script_arguments %}
{# [CallWith=ScriptArguments] #}
RefPtrWillBeRawPtr<ScriptArguments> scriptArguments(createScriptArguments(scriptState, info, {{method.number_of_arguments}}));
{% endif %}
{% if method.is_call_with_document %}
{# [ConstructorCallWith=Document] #}
Document& document = *toDocument(currentExecutionContext(info.GetIsolate()));
{% endif %}
{# Call #}
{% if method.idl_type == 'void' %}
{{cpp_value}};
{% elif method.is_implemented_in_private_script %}
{{method.cpp_type}} result{{method.cpp_type_initializer}};
if (!{{method.cpp_value}})
    return;
{% elif method.use_output_parameter_for_result %}
{{method.cpp_type}} result;
{{cpp_value}};
{% elif method.is_constructor %}
{{method.cpp_type}} impl = {{cpp_value}};
{% elif method.use_local_result %}
{{method.cpp_type}} result = {{cpp_value}};
{% endif %}
{# Post-call #}
{% if method.is_raises_exception %}
if (exceptionState.hadException()) {
    {{throw_from_exception_state(method)}};
    return;
}
{% endif %}
{# Set return value #}
{% if method.is_constructor %}
{{generate_constructor_wrapper(method)}}
{%- elif v8_set_return_value %}
{% if method.is_explicit_nullable %}
if (result.isNull())
    v8SetReturnValueNull(info);
else
    {{v8_set_return_value}};
{% else %}
{{v8_set_return_value}};
{% endif %}
{%- endif %}{# None for void #}
{# Post-set #}
{% if interface_name in ('EventTarget', 'MediaQueryList')
    and method.name in ('addEventListener', 'removeEventListener', 'addListener', 'removeListener') %}
{% set hidden_dependency_action = 'addHiddenValueToArray'
       if method.name in ('addEventListener', 'addListener') else 'removeHiddenValueFromArray' %}
{% set argument_index = '1' if interface_name == 'EventTarget' else '0' %}
{# Length check needed to skip action on legacy calls without enough arguments.
   http://crbug.com/353484 #}
if (info.Length() >= {{argument_index}} + 1 && listener && !impl->toNode())
    {{hidden_dependency_action}}(info.GetIsolate(), info.Holder(), info[{{argument_index}}], {{v8_class}}::eventListenerCacheIndex);
{% endif %}
{% endmacro %}


{######################################}
{% macro throw_type_error(method, error_message) %}
{% if method.has_exception_state %}
exceptionState.throwTypeError({{error_message}});
{{throw_from_exception_state(method)}};
{% elif method.idl_type == 'Promise' %}
v8SetReturnValue(info, ScriptPromise::rejectRaw(info.GetIsolate(), V8ThrowException::createTypeError(info.GetIsolate(), {{type_error_message(method, error_message)}})));
{% else %}
V8ThrowException::throwTypeError({{type_error_message(method, error_message)}}, info.GetIsolate());
{% endif %}{# method.has_exception_state #}
{% endmacro %}


{######################################}
{% macro type_error_message(method, error_message) %}
{% if method.is_constructor %}
ExceptionMessages::failedToConstruct("{{interface_name}}", {{error_message}})
{%- else %}
ExceptionMessages::failedToExecute("{{method.name}}", "{{interface_name}}", {{error_message}})
{%- endif %}
{%- endmacro %}


{######################################}
{% macro throw_from_exception_state(method_or_overloads) %}
{% if method_or_overloads.idl_type == 'Promise' or method_or_overloads.returns_promise_all %}
v8SetReturnValue(info, exceptionState.reject(ScriptState::current(info.GetIsolate())).v8Value())
{%- else %}
exceptionState.throwIfNeeded()
{%- endif %}
{%- endmacro %}


{######################################}
{% macro throw_minimum_arity_type_error(method, number_of_required_arguments) %}
{% if method.has_exception_state %}
setMinimumArityTypeError(exceptionState, {{number_of_required_arguments}}, info.Length());
{{throw_from_exception_state(method)}};
{%- elif method.idl_type == 'Promise' %}
v8SetReturnValue(info, ScriptPromise::rejectRaw(info.GetIsolate(), {{create_minimum_arity_type_error_without_exception_state(method, number_of_required_arguments)}}));
{%- else %}
V8ThrowException::throwException({{create_minimum_arity_type_error_without_exception_state(method, number_of_required_arguments)}}, info.GetIsolate());
{%- endif %}
{%- endmacro %}


{######################################}
{% macro create_minimum_arity_type_error_without_exception_state(method, number_of_required_arguments) %}
{% if method.is_constructor %}
createMinimumArityTypeErrorForConstructor(info.GetIsolate(), "{{interface_name}}", {{number_of_required_arguments}}, info.Length())
{%- else %}
createMinimumArityTypeErrorForMethod(info.GetIsolate(), "{{method.name}}", "{{interface_name}}", {{number_of_required_arguments}}, info.Length())
{%- endif %}
{%- endmacro %}


{##############################################################################}
{# FIXME: We should return a rejected Promise if an error occurs in this
function when ALL methods in this overload return Promise. In order to do so,
we must ensure either ALL or NO methods in this overload return Promise #}
{% macro overload_resolution_method(overloads, world_suffix) %}
static void {{overloads.name}}Method{{world_suffix}}(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "{{overloads.name}}", "{{interface_name}}", info.Holder(), info.GetIsolate());
    {% if overloads.measure_all_as %}
    UseCounter::countIfNotPrivateScript(info.GetIsolate(), callingExecutionContext(info.GetIsolate()), UseCounter::{{overloads.measure_all_as}});
    {% endif %}
    {% if overloads.deprecate_all_as %}
    UseCounter::countDeprecationIfNotPrivateScript(info.GetIsolate(), callingExecutionContext(info.GetIsolate()), UseCounter::{{overloads.deprecate_all_as}});
    {% endif %}
    {# First resolve by length #}
    {# 2. Initialize argcount to be min(maxarg, n). #}
    switch (std::min({{overloads.maxarg}}, info.Length())) {
    {# 3. Remove from S all entries whose type list is not of length argcount. #}
    {% for length, tests_methods in overloads.length_tests_methods %}
    {# 10. If i = d, then: #}
    case {{length}}:
        {# Then resolve by testing argument #}
        {% for test, method in tests_methods %}
        {% if method.visible %}
        {% filter runtime_enabled(not overloads.runtime_enabled_function_all and
                                  method.runtime_enabled_function) %}
        if ({{test}}) {
            {% if method.measure_as and not overloads.measure_all_as %}
            UseCounter::countIfNotPrivateScript(info.GetIsolate(), callingExecutionContext(info.GetIsolate()), UseCounter::{{method.measure_as}});
            {% endif %}
            {% if method.deprecate_as and not overloads.deprecate_all_as %}
            UseCounter::countDeprecationIfNotPrivateScript(info.GetIsolate(), callingExecutionContext(info.GetIsolate()), UseCounter::{{method.deprecate_as}});
            {% endif %}
            {{method.name}}{{method.overload_index}}Method{{world_suffix}}(info);
            return;
        }
        {% endfilter %}
        {% endif %}
        {% endfor %}
        break;
    {% endfor %}
    {% if is_partial or not overloads.has_partial_overloads %}
    default:
        {# If methods are overloaded between interface and partial interface #}
        {# definitions, need to invoke methods defined in the partial #}
        {# interface. #}
        {# FIXME: we do not need to always generate this code. #}
        {# Invalid arity, throw error #}
        {# Report full list of valid arities if gaps and above minimum #}
        {% if overloads.valid_arities %}
        if (info.Length() >= {{overloads.minarg}}) {
            setArityTypeError(exceptionState, "{{overloads.valid_arities}}", info.Length());
            {{throw_from_exception_state(overloads)}};
            return;
        }
        {% endif %}
        {# Otherwise just report "not enough arguments" #}
        exceptionState.throwTypeError(ExceptionMessages::notEnoughArguments({{overloads.minarg}}, info.Length()));
        {{throw_from_exception_state(overloads)}};
        return;
    {% endif %}
    }
    {% if not is_partial and overloads.has_partial_overloads %}
    ASSERT({{overloads.name}}MethodForPartialInterface);
    ({{overloads.name}}MethodForPartialInterface)(info);
    {% else %}
    {# No match, throw error #}
    exceptionState.throwTypeError("No function was found that matched the signature provided.");
    {{throw_from_exception_state(overloads)}};
    {% endif %}
}
{% endmacro %}


{##############################################################################}
{% macro method_callback(method, world_suffix) %}
{% filter conditional(method.conditional_string) %}
static void {{method.name}}MethodCallback{{world_suffix}}(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("blink", "DOMMethod");
    {% if not method.overloads %}{# Overloaded methods are measured in overload_resolution_method() #}
    {% if method.measure_as %}
    UseCounter::countIfNotPrivateScript(info.GetIsolate(), callingExecutionContext(info.GetIsolate()), UseCounter::{{method.measure_as}});
    {% endif %}
    {% if method.deprecate_as %}
    UseCounter::countDeprecationIfNotPrivateScript(info.GetIsolate(), callingExecutionContext(info.GetIsolate()), UseCounter::{{method.deprecate_as}});
    {% endif %}
    {% endif %}{# not method.overloads #}
    {% if world_suffix in method.activity_logging_world_list %}
    ScriptState* scriptState = ScriptState::from(info.GetIsolate()->GetCurrentContext());
    V8PerContextData* contextData = scriptState->perContextData();
    {% if method.activity_logging_world_check %}
    if (scriptState->world().isIsolatedWorld() && contextData && contextData->activityLogger())
    {% else %}
    if (contextData && contextData->activityLogger()) {
    {% endif %}
        ExceptionState exceptionState(ExceptionState::ExecutionContext, "{{method.name}}", "{{interface_name}}", info.Holder(), info.GetIsolate());
        Vector<v8::Handle<v8::Value> > loggerArgs = toImplArguments<v8::Handle<v8::Value> >(info, 0, exceptionState);
        contextData->activityLogger()->logMethod("{{interface_name}}.{{method.name}}", info.Length(), loggerArgs.data());
    }
    {% endif %}
    {% if method.is_custom %}
    {{v8_class}}::{{method.name}}MethodCustom(info);
    {% else %}
    {{cpp_class_or_partial}}V8Internal::{{method.name}}Method{{world_suffix}}(info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("v8", "V8Execution");
}
{% endfilter %}
{% endmacro %}


{##############################################################################}
{% macro origin_safe_method_getter(method, world_suffix) %}
static void {{method.name}}OriginSafeMethodGetter{{world_suffix}}(const v8::PropertyCallbackInfo<v8::Value>& info)
{
    {% set signature = 'v8::Local<v8::Signature>()'
                       if method.is_do_not_check_signature else
                       'v8::Signature::New(info.GetIsolate(), %s::domTemplate(info.GetIsolate()))' % v8_class %}
    static int domTemplateKey; // This address is used for a key to look up the dom template.
    V8PerIsolateData* data = V8PerIsolateData::from(info.GetIsolate());
    {# FIXME: 1 case of [DoNotCheckSignature] in Window.idl may differ #}
    v8::Handle<v8::FunctionTemplate> privateTemplate = data->domTemplate(&domTemplateKey, {{cpp_class}}V8Internal::{{method.name}}MethodCallback{{world_suffix}}, v8Undefined(), {{signature}}, {{method.length}});

    v8::Handle<v8::Object> holder = {{v8_class}}::findInstanceInPrototypeChain(info.This(), info.GetIsolate());
    if (holder.IsEmpty()) {
        // This is only reachable via |object.__proto__.func|, in which case it
        // has already passed the same origin security check
        v8SetReturnValue(info, privateTemplate->GetFunction());
        return;
    }
    {{cpp_class}}* impl = {{v8_class}}::toImpl(holder);
    if (!BindingSecurity::shouldAllowAccessToFrame(info.GetIsolate(), impl->frame(), DoNotReportSecurityError)) {
        static int sharedTemplateKey; // This address is used for a key to look up the dom template.
        v8::Handle<v8::FunctionTemplate> sharedTemplate = data->domTemplate(&sharedTemplateKey, {{cpp_class}}V8Internal::{{method.name}}MethodCallback{{world_suffix}}, v8Undefined(), {{signature}}, {{method.length}});
        v8SetReturnValue(info, sharedTemplate->GetFunction());
        return;
    }

    {# The findInstanceInPrototypeChain() call above only returns a non-empty handle if info.This() is an Object. #}
    v8::Local<v8::Value> hiddenValue = v8::Handle<v8::Object>::Cast(info.This())->GetHiddenValue(v8AtomicString(info.GetIsolate(), "{{method.name}}"));
    if (!hiddenValue.IsEmpty()) {
        v8SetReturnValue(info, hiddenValue);
        return;
    }

    v8SetReturnValue(info, privateTemplate->GetFunction());
}

static void {{method.name}}OriginSafeMethodGetterCallback{{world_suffix}}(v8::Local<v8::String>, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("blink", "DOMGetter");
    {{cpp_class}}V8Internal::{{method.name}}OriginSafeMethodGetter{{world_suffix}}(info);
    TRACE_EVENT_SET_SAMPLING_STATE("v8", "V8Execution");
}
{% endmacro %}


{##############################################################################}
{% macro method_implemented_in_private_script(method) %}
bool {{v8_class}}::PrivateScript::{{method.name}}Method({{method.argument_declarations_for_private_script | join(', ')}})
{
    if (!frame)
        return false;
    v8::HandleScope handleScope(toIsolate(frame));
    ScriptForbiddenScope::AllowUserAgentScript script;
    v8::Handle<v8::Context> contextInPrivateScript = toV8Context(frame, DOMWrapperWorld::privateScriptIsolatedWorld());
    if (contextInPrivateScript.IsEmpty())
        return false;
    ScriptState* scriptState = ScriptState::from(contextInPrivateScript);
    ScriptState* scriptStateInUserScript = ScriptState::forMainWorld(frame);
    if (!scriptState->executionContext())
        return false;

    ScriptState::Scope scope(scriptState);
    v8::Handle<v8::Value> holder = toV8(holderImpl, scriptState->context()->Global(), scriptState->isolate());

    {% for argument in method.arguments %}
    v8::Handle<v8::Value> {{argument.handle}} = {{argument.private_script_cpp_value_to_v8_value}};
    {% endfor %}
    {% if method.arguments %}
    v8::Handle<v8::Value> argv[] = { {{method.arguments | join(', ', 'handle')}} };
    {% else %}
    {# Empty array initializers are illegal, and don\t compile in MSVC. #}
    v8::Handle<v8::Value> *argv = 0;
    {% endif %}
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "{{method.name}}", "{{cpp_class}}", scriptState->context()->Global(), scriptState->isolate());
    v8::Handle<v8::Value> v8Value = PrivateScriptRunner::runDOMMethod(scriptState, scriptStateInUserScript, "{{cpp_class}}", "{{method.name}}", holder, {{method.arguments | length}}, argv);
    if (v8Value.IsEmpty())
        return false;
    {% if method.idl_type != 'void' %}
    {{method.private_script_v8_value_to_local_cpp_value}};
    *result = cppValue;
    {% endif %}
    RELEASE_ASSERT(!exceptionState.hadException());
    return true;
}
{% endmacro %}


{##############################################################################}
{% macro generate_constructor(constructor) %}
{% set name = '%sConstructorCallback' % v8_class
              if constructor.is_named_constructor else
              'constructor%s' % (constructor.overload_index or '') %}
static void {{name}}(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    {% if constructor.is_named_constructor %}
    if (!info.IsConstructCall()) {
        V8ThrowException::throwTypeError(ExceptionMessages::constructorNotCallableAsFunction("{{constructor.name}}"), info.GetIsolate());
        return;
    }

    if (ConstructorMode::current(info.GetIsolate()) == ConstructorMode::WrapExistingObject) {
        v8SetReturnValue(info, info.Holder());
        return;
    }
    {% endif %}
    {% if constructor.has_exception_state %}
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "{{interface_name}}", info.Holder(), info.GetIsolate());
    {% endif %}
    {# Overloaded constructors have length checked during overload resolution #}
    {% if constructor.number_of_required_arguments and not constructor.overload_index %}
    if (UNLIKELY(info.Length() < {{constructor.number_of_required_arguments}})) {
        {{throw_minimum_arity_type_error(constructor, constructor.number_of_required_arguments) | indent(8)}}
        return;
    }
    {% endif %}
    {% if constructor.arguments %}
    {{generate_arguments(constructor) | indent}}
    {% endif %}
    {{cpp_method_call(constructor, constructor.v8_set_return_value, constructor.cpp_value) | indent}}
}
{% endmacro %}


{##############################################################################}
{% macro generate_constructor_wrapper(constructor) %}
{% if has_custom_wrap %}
v8::Handle<v8::Object> wrapper = wrap(impl.get(), info.Holder(), info.GetIsolate());
{% else %}
{% set constructor_class = v8_class + ('Constructor'
                                       if constructor.is_named_constructor else
                                       '') %}
v8::Handle<v8::Object> wrapper = info.Holder();
{% if is_script_wrappable %}
impl->associateWithWrapper(&{{constructor_class}}::wrapperTypeInfo, wrapper, info.GetIsolate());
{% else %}
V8DOMWrapper::associateObjectWithWrapper<{{v8_class}}>(impl.release(), &{{constructor_class}}::wrapperTypeInfo, wrapper, info.GetIsolate());
{% endif %}
{% endif %}
v8SetReturnValue(info, wrapper);
{% endmacro %}


{##############################################################################}
{% macro method_configuration(method) %}
{% set method_callback =
   '%sV8Internal::%sMethodCallback' % (cpp_class_or_partial, method.name) %}
{% set method_callback_for_main_world =
   '%sV8Internal::%sMethodCallbackForMainWorld' % (cpp_class_or_partial, method.name)
   if method.is_per_world_bindings else '0' %}
{% set only_exposed_to_private_script = 'V8DOMConfiguration::OnlyExposedToPrivateScript' if method.only_exposed_to_private_script else 'V8DOMConfiguration::ExposedToAllScripts' %}
{"{{method.name}}", {{method_callback}}, {{method_callback_for_main_world}}, {{method.length}}, {{only_exposed_to_private_script}}}
{%- endmacro %}


{######################################}
{% macro install_custom_signature(method) %}
{% set method_callback = '%sV8Internal::%sMethodCallback' % (cpp_class_or_partial, method.name) %}
{% set method_callback_for_main_world = '%sForMainWorld' % method_callback
  if method.is_per_world_bindings else '0' %}
{% set property_attribute =
  'static_cast<v8::PropertyAttribute>(%s)' % ' | '.join(method.property_attributes)
  if method.property_attributes else 'v8::None' %}
{% set only_exposed_to_private_script = 'V8DOMConfiguration::OnlyExposedToPrivateScript' if method.only_exposed_to_private_script else 'V8DOMConfiguration::ExposedToAllScripts' %}
static const V8DOMConfiguration::MethodConfiguration {{method.name}}MethodConfiguration = {
    "{{method.name}}", {{method_callback}}, {{method_callback_for_main_world}}, {{method.length}}, {{only_exposed_to_private_script}},
};
V8DOMConfiguration::installMethod({{method.function_template}}, {{method.signature}}, {{property_attribute}}, {{method.name}}MethodConfiguration, isolate);
{%- endmacro %}

{######################################}
{% macro install_conditionally_enabled_methods() %}
void {{v8_class_or_partial}}::installConditionallyEnabledMethods(v8::Handle<v8::Object> prototypeObject, v8::Isolate* isolate)
{
    {% if is_partial %}
    {{v8_class}}::installConditionallyEnabledMethods(prototypeObject, isolate);
    {% endif %}
    {% if conditionally_enabled_methods %}
    {# Define per-context enabled operations #}
    v8::Local<v8::Signature> defaultSignature = v8::Signature::New(isolate, domTemplate(isolate));
    ExecutionContext* context = toExecutionContext(prototypeObject->CreationContext());
    ASSERT(context);

    {% for method in conditionally_enabled_methods %}
    {% filter per_context_enabled(method.per_context_enabled_function) %}
    {% filter exposed(method.exposed_test) %}
    prototypeObject->Set(v8AtomicString(isolate, "{{method.name}}"), v8::FunctionTemplate::New(isolate, {{cpp_class_or_partial}}V8Internal::{{method.name}}MethodCallback, v8Undefined(), defaultSignature, {{method.number_of_required_arguments}})->GetFunction());
    {% endfilter %}
    {% endfilter %}
    {% endfor %}
    {% endif %}
}
{%- endmacro %}
