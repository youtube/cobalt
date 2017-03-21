{% extends 'interface_base.cpp' %}

{##############################################################################}
{% block partial_interface %}
void {{v8_class_or_partial}}::initialize()
{
    // Should be invoked from initModules.
    {{v8_class}}::updateWrapperTypeInfo(
        &{{v8_class_or_partial}}::install{{v8_class}}Template,
        &{{v8_class_or_partial}}::installConditionallyEnabledMethods);
    {% for method in methods %}
    {% if method.overloads and method.overloads.has_partial_overloads %}
    {{v8_class}}::register{{method.name | blink_capitalize}}MethodForPartialInterface(&{{cpp_class_or_partial}}V8Internal::{{method.name}}Method);
    {% endif %}
    {% endfor %}
}

{% endblock %}
