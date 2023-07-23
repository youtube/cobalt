# Script/bindings in depth

Bindings is the bridge that allows client JavaScript code to read/modify DOM objects.  In order to better understand what this means, let's start with a simple example from the [V8 embedder's guide](https://github.com/v8/v8/wiki/Embedder%27s-Guide), and build up to Cobalt bindings in their entirety.

Suppose we are working with a native application that deals with `Point` structure, representing two coordinates as integers.  Now suppose that we also wanted to allow for the application to be scripted in JavaScript, meaning that JavaScript code is capable of manipulating our project specific `Point` struct.


```
struct Point {
  int x;
  int y;
};
```


This is accomplished by using embedder only native APIs to both



*   Allow JavaScript objects to represent `Point`s, by putting a pointer to the `Point` an object represents inside of a native-only internal field.
*   Register native functions with the JavaScript engine to say "when JavaScript code does this, instead of doing what you normally do, call this function".

In V8, this would look something like:


```
Local<ObjectTemplate> point_template = ObjectTemplate::New(isolate);
point_template->SetInternalFieldCount(1);
point_template.SetAccessor(String::NewFromUtf8(isolate, "x"), GetPointX, SetPointX);
point_template.SetAccessor(String::NewFromUtf8(isolate, "y"), GetPointY, SetPointY);

Point* p = new Point{0, 0};
Local<Object> obj = point_template->NewInstance();
obj->SetInternalField(0, External::New(isolate, p));

void GetPointX(Local<String> property,
               const PropertyCallbackInfo<Value>& info) {
  Local<Object> self = info.Holder();
  Local<External> wrap = Local<External>::Cast(self->GetInternalField(0));
  void* ptr = wrap->Value();
  int value = static_cast<Point*>(ptr)->x;
  info.GetReturnValue().Set(value);
}
// void GetPointY(...
```


In the above example, we first create an `ObjectTemplate`.  This is a structure that can later be fed into V8 that lets it know the "native shape" of the point object.  In particular, the `SetAccessor` lines are the ones that tell it to call into our `GetPointX` function when the property "x" is accessed on a point object.  After V8 calls into the `GetPointX` function, we find the associated native `Point` object, get its x field, convert that value to a JavaScript integer, and then pass that back to JavaScript as the return value.

That pattern, of intercept, convert from JavaScript to native, perform native operations, and then convert the native result back to JavaScript, is the essence of bindings.  Of course, it is far more complicated, in that there are many different types, and more operations that must be performed.  It is important however, to keep this general pattern in mind.

# Scaling up to full Cobalt bindings

How do we scale up from what we saw before into full Cobalt bindings?  The first problem that must be addressed is what should be exposed to JavaScript within each target.  Instead of manually writing an ObjectTemplate for each object we want to expose to JavaScript, we write an [IDL file](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/dom/document.idl?q=document%5C.idl&sq=package:%5Ecobalt$&dr).  Then, these [IDL files](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/browser/browser_bindings_gen.gyp?q=browser_bindings_gen&sq=package:%5Ecobalt$&dr#32) are collected by the [IDL compiler](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/bindings/idl_compiler_cobalt.py?q=idl_compiler&sq=package:%5Ecobalt$&dr=C#17), and then combined with a [jinja2 template file](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/bindings/v8c/templates/interface.cc.template?q=interface%5C.cc%5C.template&sq=package:%5Ecobalt$&dr), that will generate [C++ header](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/bindings/generated/v8c/testing/cobalt/bindings/testing/v8c_window.h?q=v8c_window%5C.h&sq=package:%5Ecobalt$&dr) and [source files](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/bindings/generated/v8c/testing/cobalt/bindings/testing/v8c_window.cc?q=v8c_window%5C.cc&sq=package:%5Ecobalt$&g=0#1) for each IDL.

The jinja2 template file is responsible for



*   [Providing access to the interface object itself](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/bindings/v8c/templates/interface.cc.template?q=interface%5C.cc%5C.template&sq=package:%5Ecobalt$&dr#866).
*   [Providing a function that creates Wrappers from a Wrappable](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/bindings/v8c/templates/interface.cc.template?q=interface%5C.cc%5C.template&sq=package:%5Ecobalt$&dr#844).
*   [Providing enum conversion functions](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/bindings/v8c/templates/interface.cc.template?q=interface%5C.cc%5C.template&sq=package:%5Ecobalt$&dr#939).
*   [Implementing and setting up interceptors if necessary](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/bindings/v8c/templates/interface.cc.template?q=interface%5C.cc%5C.template&sq=package:%5Ecobalt$&dr#102).
*   [Possibly implementing and attaching a constructor](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/bindings/v8c/templates/interface.cc.template?q=interface%5C.cc%5C.template&sq=package:%5Ecobalt$&dr#353), [which might have to also handle overloads](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/bindings/v8c/templates/macros.cc.template?q=macros%5C.cc%5C.template&sq=package:%5Ecobalt$&dr#357).
*   [Attaching attributes and functions to either the interface object, the object prototype, or object instances](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/bindings/v8c/templates/interface.cc.template?q=interface%5C.cc%5C.template&sq=package:%5Ecobalt$&dr#382).
*   [Rejecting edge case scenarios in which bindings functions are called on non-platform objects](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/bindings/v8c/templates/macros.cc.template?q=macros%5C.cc%5C.template&sq=package:%5Ecobalt$&dr#20).
*   [Stringification](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/bindings/v8c/templates/interface.cc.template?q=interface%5C.cc%5C.template&sq=package:%5Ecobalt$&dr#463) (a concept [defined by Web IDL](https://heycam.github.io/webidl/#idl-stringifiers)).
*   [Initialization](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/bindings/v8c/templates/interface.cc.template?q=interface%5C.cc%5C.template&sq=package:%5Ecobalt$&dr#876).
*   [Web IDL interface inheritance](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/bindings/v8c/templates/interface.cc.template?q=interface%5C.cc%5C.template&sq=package:%5Ecobalt$&dr#548).

The details behind each of these responsibilities are discussed in the [Web IDL](https://heycam.github.io/webidl/) spec, which has opinions on how to map Web IDL defined concepts that you see in the IDL files, to precise language bindings behavior.  [The first section](https://heycam.github.io/webidl/#idl) discusses this mapping without a particular language in mind, however thankfully after that there is a [section about ECMAScript bindings](https://heycam.github.io/webidl/#ecmascript-binding) in particular.

The majority of the template file is about translating what the spec says in the ECMAScript bindings section of that spec, and calling the appropriate native APIs given a particular interface context.  So for example, when we see


```
// document.idl
// ...
  HTMLCollection getElementsByTagName(DOMString localName);
// ...
```


in an idl file, our IDL compiler will render the jinja template (relevant sections shown below)


```
// interface.cc.template
// …
{%- for operation in operations + static_operations %}
void {{operation.idl_name}}{{overload.overload_index}}(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
{{ function_implementation(overload) -}}
}
{% macro function_implementation(operation) %}
  v8::Isolate* isolate = info.GetIsolate();
{% if operation.is_static %}
{{ static_function_prologue() }}
// ...
{% for operation in operations + static_operations %}
v8::Local<v8::String> name = NewInternalString(isolate, "{{operation.idl_name}}");
v8::Local<v8::FunctionTemplate> method_template = v8::FunctionTemplate::New(isolate,
    {{operation.idl_name}}{{"Static" if operation.is_static else ""}}Method);
method_template->RemovePrototype();
method_template->SetLength({{operation.length}});
{% if operation.is_static %}
function_template->
{% elif operation.is_unforgeable or is_global_interface %}
instance_template->
{% else %}
prototype_template->
{% endif %}
    Set(name, method_template);
```


and produce the following native code


```
void getElementsByTagNameMethod(const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();
  v8::Local<v8::Object> object = info.Holder();
  V8cGlobalEnvironment* global_environment = V8cGlobalEnvironment::GetFromIsolate(isolate);
  WrapperFactory* wrapper_factory = global_environment->wrapper_factory();
  if (!WrapperPrivate::HasWrapperPrivate(object) ||
      !V8cDocument::GetTemplate(isolate)->HasInstance(object)) {
    V8cExceptionState exception(isolate);
    exception.SetSimpleException(script::kDoesNotImplementInterface);
    return;
  }
    // The name of the property is the identifier.
    v8::Local<v8::String> name = NewInternalString(
        isolate,
        "getElementsByTagName");
    // ...
```


# script/

script/ implements various utility functions, abstraction classes, and wrapper classes that are both used directly by Cobalt, and to support bindings.  The majority of these are either interfaces to abstract over important engine native APIs (such as forcing garbage collection, or getting heap statistics), or interfaces to allow Cobalt to interact with ECMAScript defined types that are implemented by the JavaScript engine, such as promises.

[JavaScriptEngine](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/javascript_engine.h?type=cs&q=JavaScriptEngine&sq=package:%5Ecobalt$&g=0#36) is an abstract isolated instance of the JavaScript engine.  It corresponds directly to the type [v8::Isolate](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/v8/include/v8.h?type=cs&q=file:v8+Isolate&sq=package:%5Ecobalt$&g=0#6746).  These types are about everything that has to do with a JavaScript *before* the global object gets involved, so things like the heap, and builtin functions.  As this class owns the JavaScript heap, Cobalt must go through it when Cobalt wants to interact with the JavaScript heap, for things such as [forcing garbage collection](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/javascript_engine.h?type=cs&q=JavaScriptEngine&sq=package:%5Ecobalt$&g=0#64) (which is used to lower memory footprint during suspend), [reporting extra memory usage implied by the engine](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/javascript_engine.h?type=cs&q=JavaScriptEngine&sq=package:%5Ecobalt$&g=0#69) (so for example, when a JavaScript object being alive means that xhr data that lives in native Cobalt buffers is kept alive), and [gathering information about the size of the heap](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/javascript_engine.h?type=cs&q=JavaScriptEngine&sq=package:%5Ecobalt$&g=0#78) (used in APIs such as [window.performance.memory](https://developer.mozilla.org/en-US/docs/Web/API/Window/performance)).  Additionally, having a JavaScriptEngine is a prerequisite for creating a GlobalEnvironment, which is required for JavaScript execution.

[GlobalEnvironment](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/global_environment.h?type=cs&q=GlobalEnvironment&sq=package:%5Ecobalt$&g=0#38) is an abstract JavaScript execution context, which effectively means the [global object](https://www.ecma-international.org/ecma-262/6.0/#sec-global-object) itself, as well as things that are very closely related to the global object (such as script evaluation).  It corresponds to type [v8::Context](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/v8/include/v8.h?type=cs&q=file:v8+Context&sq=package:%5Ecobalt$&g=0#8380).  Also note that in the case of Cobalt, there is only one GlobalEnvironment per JavaScriptEngine, because we don't support features that would require two (such as iframes).  Cobalt will use this class when it wants to do things such as [evaluate scripts](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/global_environment.h?q=GlobalEnvironment&sq=package:%5Ecobalt$&dr=CSs#68), inspect execution state (such as [getting a stack trace](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/global_environment.h?q=GlobalEnvironment&sq=package:%5Ecobalt$&dr=CSs#76)), or [interact with garbage collection](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/global_environment.h?q=GlobalEnvironment&sq=package:%5Ecobalt$&dr=CSs#89).  Implementations of this class are also responsible for setting up the global environment

Additionally, it contains interfaces for JavaScript (both ES6 and Web IDL) types that Cobalt DOM code needs to interact with, such as [array buffers](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/array_buffer.h?q=array_buffer%5C.h&sq=package:%5Ecobalt$&dr), [callback functions](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/callback_function.h?sq=package:%5Ecobalt$&dr&g=0), and [promises](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/promise.h?sq=package:%5Ecobalt$&dr&g=0).  One worthy of special attention is [ValueHandle](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/value_handle.h?sq=package:%5Ecobalt$&dr&g=0), which represents an opaque JavaScript value (that could be anything), which can be used when interactions with an object is not the goal, but rather just holding onto it, and then likely passing it to someone else.

Each of these JavaScript value interface types when used by Cobalt must get held in a special wrapper type, called [ScriptValue](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/script_value.h?q=ScriptValue&sq=package:%5Ecobalt$&dr=CSs#50).  ScriptValue provides a common type to feed into additional ScriptValue wrapper types ([ScriptValue::Reference](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/script_value.h?q=ScriptValue&sq=package:%5Ecobalt$&dr=CSs#59) and [script::Handle](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/script_value.h?q=ScriptValue&sq=package:%5Ecobalt$&dr=CSs#144)), that manage the lifetime of the underlying JavaScript value held in the ScriptValue.  This is done because the JavaScript object itself is owned by the JavaScript engine, so management of its lifetime must be done through the native engine APIs, however Cobalt cannot access those directly.

# script/$engine

Then, we designate a specific area of code to be our engine specific implementation of the interfaces established in script.  We commit to an engine at gyp time, and then based on that, [select the appropriate set of files](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/engine.gyp?type=cs&q=file:cobalt+file:gyp+javascript_engine&sq=package:%5Ecobalt$&g=0#26).  This is the only area of Cobalt code (except for bindings/*/$engine, which is essentially more of the same stuff) that is allowed to include engine specific headers (files in v8/).  Maintaining this abstraction has been useful throughout our multiple JavaScript engine migrations over the years.

A large portion of script/$engine is filling in the types discussed in the previous section.  So [V8cEngine](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/v8c/v8c_engine.h?q=V8cEngine&sq=package:%5Ecobalt$&dr=CSs#31) implements JavaScriptEngine by wrapping a v8::Isolate, and [V8cGlobalEnvironment](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/v8c/v8c_global_environment.h?type=cs&q=V8cGlobalEnvironment&sq=package:%5Ecobalt$&g=0#44) implements GlobalEnvironment by wrapping a v8::Context.  Note that these files are actually quite a bit bigger than just being thin wrappers over the V8 types, as they have more work to do in addition to just implementing their script interfaces, such as [maintaining state necessary for bindings](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/v8c/v8c_global_environment.h?type=cs&q=V8cGlobalEnvironment&sq=package:%5Ecobalt$&g=0#175) (interface objects need to be owned somewhere), [serving as a bridge between the Isolate](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/v8c/v8c_global_environment.h?type=cs&q=V8cGlobalEnvironment&sq=package:%5Ecobalt$&g=0#49), and [dealing with garbage collection interaction](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/v8c/v8c_engine.h?q=V8cEngine&sq=package:%5Ecobalt$&dr=CSs#61) (the engine specific [script::Tracer](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/tracer.h?type=cs&q=script::Tracer&sq=package:%5Ecobalt$&g=0#54) is implemented near them).

JavaScript value interface type implementations follow the pattern of creating a concrete [implementation that wraps an appropriate v8::Value](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/v8c/native_promise.h?q=native_promise&sq=package:%5Ecobalt$&dr=CSs#45).  They will also typically have a Create function if it makes sense to build them in Cobalt.  In the case where they don't have a Create function (such as ValueHandle), the only way to gain access to one in Cobalt is to receive it from bindings code.

Another important area in this module are utility functions that exist solely for bindings.  In particular, in the [conversion helpers](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/v8c/conversion_helpers.h?q=conversion_helpers&sq=package:%5Ecobalt$&dr=CSs) are implemented here, which is a giant file that implements functions to convert back and forth between native Cobalt types and JavaScript values.  These conversion helper functions get called in the native callback implementation for the getters, setters, and functions that we saw at the beginning of this doc (so the stuff that would go inside of GetPointX).  Because these conversion helpers primarily exist for parts of bindings defined by Web IDL, they're on the more complex side (because Web IDL allows for many conversion details to be configurable, such as whether null objects are accepted, or whether integral types should be clamped or not), however they are also used throughout common script/ when it makes sense.

Finally, another critical thing that takes place in script/$engine is what we call [WrapperPrivate](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/script/v8c/wrapper_private.h?type=cs&q=WrapperPrivate&sq=package:%5Ecobalt$&g=0#31).  This is the special type that goes inside of the native only internal object field that was discussed in the beginning of this doc.  WrapperPrivate is a bit more complicated however, as it has to both bridge between Cobalt DOM garbage collection and the JavaScript engine's garbage collection for more details on this), and allow for Cobalt to possibly manipulate garbage collection from the JavaScript side as well (as in add JavaScript objects to the root set of reachable objects).

# Testing

Bindings test is tested in isolation via the bindings_test target.  Because bindings is the bridge from the JavaScript engine to the DOM, bindings test works by having an [entirely separate set of IDL](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/bindings/testing/testing.gyp?type=cs&sq=package:%5Ecobalt$&g=0#27) files that contain [minimal internal logic](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/bindings/testing/arbitrary_interface.h?q=arbitrary_interface&sq=package:%5Ecobalt$&dr=CSs#27), and are meant to [stress Web IDL features](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/bindings/testing/numeric_types_test_interface.idl?sq=package:%5Ecobalt$&dr=CSs&g=0).  This is accomplished by parameterizing what IDL files should get compiled at gyp time.  All other parts of the bindings build pipeline (such as the IDL compiler and jinja templates) are shared between bindings_test and cobalt entirely.  Note that bindings_test lives above the script/ interface, so no engine specific APIs can be used within the tests.

Additionally, when it is convenient to implement a test entirely within JavaScript, certain script/ and bindings/ features are tested within layout_tests and web_platform_tests (see for example, [platform-object-user-properties-survive-gc.html](https://cobalt.googlesource.com/cobalt/+/2a8c847d785c1602f60915c8e0112e0aec6a15a5/src/cobalt/layout_tests/testdata/cobalt/platform-object-user-properties-survive-gc.html?type=cs&q=platform-object-user&sq=package:%5Ecobalt$&g=0#1)).  These serve as higher level, more end-to-end tests, that are good for testing more complex examples that also involve Cobalt's usage of script/.
