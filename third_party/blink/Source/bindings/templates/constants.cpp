{##############################################################################}
{% macro constant_getter_callback(constant) %}
{% filter conditional(constant.conditional_string) %}
static void {{constant.name}}ConstantGetterCallback(v8::Local<v8::String>, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("blink", "DOMGetter");
    {% if constant.deprecate_as %}
    UseCounter::countDeprecationIfNotPrivateScript(info.GetIsolate(), callingExecutionContext(info.GetIsolate()), UseCounter::{{constant.deprecate_as}});
    {% endif %}
    {% if constant.measure_as %}
    UseCounter::countIfNotPrivateScript(info.GetIsolate(), callingExecutionContext(info.GetIsolate()), UseCounter::{{constant.measure_as}});
    {% endif %}
    {% if constant.idl_type in ('Double', 'Float') %}
    v8SetReturnValue(info, {{constant.value}});
    {% elif constant.idl_type == 'String' %}
    v8SetReturnValueString(info, "{{constant.value}}");
    {% else %}
    v8SetReturnValueInt(info, {{constant.value}});
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("v8", "V8Execution");
}
{% endfilter %}
{% endmacro %}


{######################################}
{% macro install_constants() %}
{% if constant_configuration_constants %}
{# Normal constants #}
static const V8DOMConfiguration::ConstantConfiguration {{v8_class}}Constants[] = {
    {% for constant in constant_configuration_constants %}
    {{constant_configuration(constant)}},
    {% endfor %}
};
V8DOMConfiguration::installConstants(functionTemplate, prototypeTemplate, {{v8_class}}Constants, WTF_ARRAY_LENGTH({{v8_class}}Constants), isolate);
{% endif %}
{# Runtime-enabled constants #}
{% for constant in runtime_enabled_constants %}
{% filter runtime_enabled(constant.runtime_enabled_function) %}
static const V8DOMConfiguration::ConstantConfiguration constantConfiguration = {{constant_configuration(constant)}};
V8DOMConfiguration::installConstants(functionTemplate, prototypeTemplate, &constantConfiguration, 1, isolate);
{% endfilter %}
{% endfor %}
{# Constants with [DeprecateAs] or [MeasureAs] #}
{% for constant in special_getter_constants %}
V8DOMConfiguration::installConstant(functionTemplate, prototypeTemplate, "{{constant.name}}", {{cpp_class}}V8Internal::{{constant.name}}ConstantGetterCallback, isolate);
{% endfor %}
{# Check constants #}
{% if not do_not_check_constants %}
{% for constant in constants %}
{% if constant.idl_type not in ('Double', 'Float', 'String') %}
{% set constant_cpp_class = constant.cpp_class or cpp_class %}
COMPILE_ASSERT({{constant.value}} == {{constant_cpp_class}}::{{constant.reflected_name}}, TheValueOf{{cpp_class}}_{{constant.reflected_name}}DoesntMatchWithImplementation);
{% endif %}
{% endfor %}
{% endif %}
{% endmacro %}


{######################################}
{%- macro constant_configuration(constant) %}
{% if constant.idl_type in ('Double', 'Float') %}
    {% set value = '0, %s, 0' % constant.value %}
{% elif constant.idl_type == 'String' %}
    {% set value = '0, 0, "%s"' % constant.value %}
{% else %}
    {# 'Short', 'Long' etc. #}
    {% set value = '%s, 0, 0' % constant.value %}
{% endif %}
{"{{constant.name}}", {{value}}, V8DOMConfiguration::ConstantType{{constant.idl_type}}}
{%- endmacro %}
