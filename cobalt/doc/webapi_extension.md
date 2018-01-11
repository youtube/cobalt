# Cobalt Web Extension Support

Cobalt provides a facility for extending the JavaScript Web API.  This allows
custom web apps running on Cobalt to make calls through a custom API to
C++ Cobalt code defined per Starboard platform.  This can allow for closer
integration between the web app hosted by Cobalt and the system on which that
web app is running.

The Cobalt Web Extension support will allow you to attach an instance of a
custom class to the JavaScript `window` global object so that it can be
referenced from a web app in JavaScript as in the following example:

```
window.myInterface.RunMyFunction()
```

## Build-level modifications

In order to extend the interface, one should add the following lines to the
`variables` section of `<platform-directory>/cobalt/configuration.gypi` (see
Starboard's
[Application Customization](../../starboard/doc/building.md#application-customization)
for more information):

1. `cobalt_webapi_extension_source_idl_files`  
   This should be a list of [IDL files](https://en.wikipedia.org/wiki/Web_IDL)
   that define the collection of new interfaces introduced by your extensions.
   One of these new interfaces can be selected to be injected into the `window`
   element (see 3. `cobalt_webapi_extension_gyp_target` for information on how
   to do this).  Each IDL file listed here simultaneously defines a JavaScript
   and a C++ interface.  For each IDL file, you will be expected to also provide
   a header file in the same directory that re-declares (in C++) the interface
   declared in the IDL file, as well as an implementation of all the methods
   within it (either inline in the header file or in a corresponding source
   file).
2. `cobalt_webapi_extension_generated_header_idl_files`  
   This is a list of all files that may result in automatic header file
   generation that might be referenced from other C++ code.  An example of
   this is the definition of `enum`s that may then be referenced as types in
   a file from 1. `cobalt_webapi_extension_source_idl_files`.
3. `cobalt_webapi_extension_gyp_target`  
   This is the gyp target that will provide the IDL interface implementations,
   as well as any necessary auxiliary code.  It will be added as a dependency of
   [browser/cobalt.gyp:cobalt](../browser/cobalt.gyp).  It is expected that
   this target will implement the interface defined in
   [browser/webapi_extension.h](../browser/webapi_extension.h), which let you
   name the injected window property, and provide a function to instantiate it
   (i.e. to let you select which IDL object is the "entry point").

The first two lists get included by
[cobalt/browser/browser_bindings_gen.gyp](cobalt/browser/browser_bindings_gen.gyp),
where you can look to see many examples of existing Cobalt IDL files that define
the Web API available through Cobalt.  For each of these, you can also
examine their corresponding `.h` files and in most cases their `.cc` files as
well.

An example configuration for these variables is available at
[starboard/shared/test_webapi_extension/test_webapi_extension.gypi](../../starboard/shared/test_webapi_extension/test_webapi_extension.gypi), which
contains the following variable definitions:

```
'cobalt_webapi_extension_source_idl_files': [
  'my_new_interface.idl'
],
'cobalt_webapi_extension_generated_header_idl_files': [
  'my_new_enum.idl'
],
'cobalt_webapi_extension_gyp_target':
  '<(DEPTH)/starboard/shared/test_webapi_extension/webapi_extension.gyp:cobalt_test_webapi_extension',
```

## Implementing the [webapi_extension.h](../browser/webapi_extension.h) interface

As discussed above in 3. `cobalt_webapi_extension_gyp_target`, you must provide
an implementation of the two functions declared in 
[browser/webapi_extension.h](../browser/webapi_extension.h).

### `GetWebAPIExtensionObjectPropertyName()`
You should implement `GetWebAPIExtensionObjectPropertyName()` to return the name
of the injected `window` property.  For example, in the example from the
beginning of this document, `window.myInterface.RunMyFunction()`, we would have
the function return `std::string("myInterface")`.  If you return `nullopt` from
this function, it is assumed that you do not wish to extend the web interface.

Note that you should NOT name your `window` property the same as your class name
as described in the IDL file, it will result in a name collision in the
JavaScript environment.

### `CreateWebAPIExtensionObject()`
This function should instantiate and return the object to be accessed from
`window`.  The object must be defined by an IDL file.

## Debugging
You may find the Cobalt debug console to be particularly useful for debugging
IDL additions and changes.  In it, you can enter arbitrary JavaScript and then
hit enter to execute it.  You can toggle it open by hitting either CTRL+O or
F1, and you may have to hit the key twice to skip past the HUD mode.

Here is an example of an example interface being exercised through the
debug console:

![Debug console web extension example](resources/webapi_extension_example.jpg)
