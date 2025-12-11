# Bindings

This directory contains classes and functionality used to implement the V8 bindings layer in Blink. Any reusable bindings components/infrastructure that are independent of `core/` objects (or can be generalized to be independent) should be added to this directory, otherwise they can be kept in `bindings/core/`.

Some of the things you can find here are:

* Functionality to wrap Blink C++ objects with a JavaScript object and maintain wrappers in multiple worlds (see [ScriptWrappable](script_wrappable.h), [ActiveScriptWrappableBase](active_script_wrappable_base.h))
* Implementation of wrapper tracing (see [documentation](TraceWrapperReference.md))
* Important abstractions for script execution (see [ScriptState](script_state.h), [V8PerIsolateData](v8_per_isolate_data.h), [V8PerContextData](v8_per_context_data.h))
* Utility functions to interface with V8 and convert between V8 and Blink types (see [v8_binding.h](v8_binding.h), [to_v8.h](to_v8.h))
