// Copyright 2016 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// This implementation of the "Runtime" Devtools protocol domain is only used
// for mozjs. When using V8, we use the built-in inspector backend which
// implements this for us.

(function(debugBackend) {

// Attach methods to handle commands in the 'Runtime' devtools domain.
// https://chromedevtools.github.io/devtools-protocol/1-3/Runtime
var commands = debugBackend.Runtime = {};

// Calls a function on a previously accessed RemoteObject with an argument list
// and returns a new RemoteObject. Used extensively by devtools for
// auto-completion. The new RemoteObject uses the same |objectGroup| as the
// original object.
// https://chromedevtools.github.io/devtools-protocol/1-3/Runtime#method-callFunctionOn
commands.callFunctionOn = function(params) {
  var result = {};
  var value = null;
  try {
    eval('var f = ' + params.functionDeclaration);
    var objectEntry = _objectStore[params.objectId];
    var thisArg = objectEntry.object;
    var objectGroup = objectEntry.objectGroup;
    value = f.apply(thisArg, params.arguments);
    result.wasThrown = false;
  } catch(e) {
    value = e;
    result.exceptionDetails = e;
    result.wasThrown = true;
  }

  result.result =
      new devtools.RemoteObject(value, objectGroup, params.returnByValue);
  return JSON.stringify(result);
}

// Evaluates a string and returns a RemoteObject.
// https://chromedevtools.github.io/devtools-protocol/1-3/Runtime#method-evaluate
commands.evaluate = function(params) {
  var result = {};
  var value = null;
  try {
    if (params.includeCommandLineAPI) {
      _addCommandLineAPI();
    }

    // Use |eval| indirectly, to cause evaluation at global scope.
    // This is so subsequent calls can access variables declared here, etc.
    var geval = eval;
    value = geval(params.expression);

    // Store the last result if we're doing a "real" evaluation, not an
    // auto-complete. Seems a little special case-y, but this is taken from
    // Chrome's implementation, and looks like the only way to differentiate.
    if (params.objectGroup == 'console') {
      _lastResult = value;
    }

    result.wasThrown = false;
  } catch(e) {
    value = e;
    result.exceptionDetails = e;
    result.wasThrown = true;
  }

  // Create the RemoteObject corresponding to the result.
  result.result =
      new devtools.RemoteObject(value, params.objectGroup, params.returnByValue);

  // Add the preview, if requested.
  if (params.generatePreview) {
    var preview = _generatePreview(value);
    if (preview) {
      result.result.preview = preview;
    }
  }

  if (params.includeCommandLineAPI) {
    _removeCommandLineAPI();
  }

  return JSON.stringify(result);
}

// Returns all let, const and class variables from global scope.
// https://chromedevtools.github.io/devtools-protocol/1-3/Runtime#method-globalLexicalScopeNames
commands.globalLexicalScopeNames = function(params) {
  var result = [];
  // TODO: Get the globals.
  return JSON.stringify(result);
}

// Returns the properties of a previously accessed object as an array of
// PropertyDescriptor objects.
// The parameters specifify several options:
// * ownProperties - only include immediate properties, not the prototype chain.
// * accessorPropertiesOnly - only include accessor properties.
// https://chromedevtools.github.io/devtools-protocol/1-3/Runtime#method-getProperties
commands.getProperties = function(params) {
  var result = {};
  var properties = [];
  try {
    var objectEntry = _objectStore[params.objectId];
    var object = objectEntry.object;
    var objectGroup = objectEntry.objectGroup;
    _addProperties(object, objectGroup, !params.ownProperties, properties);

    if (params.accessorPropertiesOnly) {
      properties = properties.filter(function(element, index, array) {
          return (element.get || element.set);
        });
    }
    result.wasThrown = false;
  } catch(e) {
    value = e;
    result.exceptionDetails = e;
    result.wasThrown = true;
  }

  result.result = properties;
  return JSON.stringify(result);
}

// Releases our reference to a previously accessed object.
// https://chromedevtools.github.io/devtools-protocol/1-3/Runtime#method-releaseObject
commands.releaseObject = function(params) {
  delete _objectStore[params.objectId];
}

// Releases our references to a group of previously accessed objects.
// https://chromedevtools.github.io/devtools-protocol/1-3/Runtime#method-releaseObjectGroup
commands.releaseObjectGroup = function(params) {
  for (var objectId in _objectStore) {
    var objectEntry = _objectStore[objectId];
    if (objectEntry && objectEntry.objectGroup == params.objectGroup) {
      delete _objectStore[objectId];
    }
  }
}

// Adds the properties of |object| to the |result| array as new
// PropertyDescriptor objects. Some properties may be objects themselves,
// in which case new RemoteObjects are created using the specified
// |objectGroup|, which should be that of |object|.
// If |includePrototype| is set, the function will be called recursively on
// the prototype chain of the object.
var _addProperties = function(object, objectGroup, includePrototype, result) {
  var properties = Object.getOwnPropertyNames(object);
  var foundProto = false;
  for (var i = 0; i < properties.length; i++) {
    var key = properties[i];
    foundProto = foundProto || (key == '__proto__');

    // If we can't find the property, and its name corresponds to a number,
    // try it as an array index (integer instead of string).
    if (object[key] == null) {
      if (!isNaN(key)) {
        key = parseInt(key);
      } else {
        continue;
      }
    }

    var propertyDescriptor =
        new devtools.PropertyDescriptor(object, objectGroup, key);
    result.push(propertyDescriptor);
  }

  var proto = null;
  try {
    proto = Object.getPrototypeOf(object);
  } catch (e) {}

  if (includePrototype) {
    // Recursively add the properties from the prototype chain.
    if (proto) {
      _addProperties(proto, objectGroup, includePrototype, result);
    }
  } else if (proto && !foundProto) {
    // |getOwnPropertyNames| may not include the object prototype,
    // so if that's the case, add it now. It's a deprecated name, but devtools
    // still uses it.
    var propertyDescriptor =
        new devtools.PropertyDescriptor(object, objectGroup, '__proto__');
    result.push(propertyDescriptor);
  }
}

// Gets an object from the internal object store.
var _getObject = function(objectId) {
  return _objectStore[objectId].object;
}

// Adds an object to the internal object store and returns a unique id that can
// be used to access it again.
var _addObject = function(object, objectGroup) {
  // If we've already added this object, then use the same objectId.
  for (var objectId in _objectStore) {
    var objectEntry = _objectStore[objectId];
    if (objectEntry.object === object &&
        objectEntry.objectGroup == objectGroup) {
      return objectId;
    }
  }

  var objectId = _nextObjectId.toString();
  _nextObjectId += 1;
  _objectStore[objectId] = {};
  _objectStore[objectId].object = object;
  _objectStore[objectId].objectGroup = objectGroup;
  return objectId;
}

// Generates an object preview, which may be requested for the evaluate
// command.
var _generatePreview = function(object) {
  if (!object || (typeof object != 'object')) {
    return null;
  } else {
    return new devtools.ObjectPreview(object);
  }
}

// Returns the subtype of an object, or null if the specified value is not an
// object.
var _getSubtype = function(object) {
  if (typeof object == 'object') {
    if (object instanceof Array) {
      return 'array';
    } else if (object instanceof Date) {
      return 'date';
    } else if (object instanceof Error) {
      return 'error';
    } else if (object instanceof Node) {
      return 'node';
    } else if (object instanceof RegExp) {
      return 'regexp';
    }
  }
  return null;
}

// Tries to get the classname of an object by following the prototype chain
// and looking for a constructor.
var _getClassName = function(object) {
  try {
    for (var obj = object; obj && !this.className;
         obj = Object.getPrototypeOf(obj)) {
      if (obj.constructor) {
        return obj.constructor.name;
      }
    }
  } catch(e) {}

  return null;
}

// Namespace for constructors of types defined in the Devtools protocol.
var devtools = {};

// Creates a RemoteObject, which is the type used to return many values to
// devtools. If |value| is an object, then is it inserted into |_objectStore|
// and the |objectId| key used to access it is included in the RemoteObject. If
// |value| is not an object, or |returnByValue| is true, then |value| is
// directly included in the RemoteObject.
devtools.RemoteObject = function(value, objectGroup, returnByValue) {
  this.type = typeof value;

  if (value == null) {
    this.subtype == 'null';
    this.value = null;
    return;
  }

  if (this.type == 'object') {
    this.objectId = _addObject(value, objectGroup);
    this.subtype = _getSubtype(value);
    this.className = _getClassName(value);
  }

  // Fill in the description field. Devtools will only display arrays correctly
  // if their description follows a particular format. For other values, try to
  // use the generic string conversion, and fall back to the className if that
  // fails.
  if (this.subtype == 'array') {
    this.description = 'Array[' + value.length + ']';
  } else {
    try {
      this.description = value.toString();
    } catch(e) {
      this.description = this.className;
    }
  }

  if (returnByValue || this.type != 'object') {
    this.value = value;
  }
}

// Creates a PropertyDescriptor for |property| of |object|, which is the type
// used to return object properties to devtools. Some properties may be objects,
// in which case new RemoteObjects are created and inserted into |_objectStore|
// using the specified |objectGroup|, which should be that of |object|.
devtools.PropertyDescriptor = function(object, objectGroup, property) {
  this.name = property.toString();
  var descriptor = Object.getOwnPropertyDescriptor(object, property);
  // Some Cobalt objects don't seem to support |getOwnPropertyDescriptor|,
  // so we handle that case in the else clause below.
  if (descriptor) {
    this.configurable = descriptor.configurable;
    this.enumerable = descriptor.enumerable;
    if (descriptor.get) {
      this.get = new devtools.RemoteObject(descriptor.get, objectGroup, false);
    }
    if (descriptor.set) {
      this.set = new devtools.RemoteObject(descriptor.set, objectGroup, false);
    }
    if (descriptor.value != null) {
      this.value =
          new devtools.RemoteObject(descriptor.value, objectGroup, false);
    }
    this.writable = descriptor.writable;
  } else if (object[property] != null) {
    this.configurable = false;
    this.enumerable = object.propertyIsEnumerable(property);
    if (object.__lookupGetter__(property)) {
      this.get = object.__lookupGetter__(property);
    }
    if (object.__lookupSetter__(property)) {
      this.set = object.__lookupSetter__(property);
    }
    this.value =
        new devtools.RemoteObject(object[property], objectGroup, false);
  }
}

// Creates an ObjectPreview, the type to represent a preview of an object,
// which may be requested by devtools in the evaluate command.
devtools.ObjectPreview = function(value) {
  this.type = typeof value;
  this.subtype = _getSubtype(value);
  this.lossless = true;
  this.overflow = false;
  this.properties = [];

  // Use the className as the preview description. This matches Chrome.
  this.description = _getClassName(value);

  // If we have an array-like object, add the array items, or append the
  // length to the description if there's too many.
  if (value.length != null) {
    var MAX_ARRAY_ITEMS = 99;
    if (value.length <= MAX_ARRAY_ITEMS) {
      for (var i = 0; i < value.length; i++) {
        var property = new devtools.PropertyPreview(i, value[i]);
        this.properties.push(property);
        if (typeof value[i] == 'object') {
          this.lossless = false;
        }
      }
    } else {
      this.description += '[' + value.length + ']';
      this.lossless = false;
      this.overflow = true;
    }
    return;
  }

  // Add object properties, up to a maximum.
  var MAX_PROPERTIES = 5;
  var numProperties = 0;
  for (var name in value) {
    if (value[name] != null) {
      if (++numProperties > MAX_PROPERTIES) {
        this.lossless = false;
        this.overflow = true;
        break;
      }
      if (typeof property == 'object') {
        this.lossless = false;
      }
      var property = new devtools.PropertyPreview(name, value[name]);
      this.properties.push(property);
    }
  }
}

// Creates a PropertyPreview, the type to represent a preview of a single
// object property.
devtools.PropertyPreview = function(name, value) {
  this.name = name.toString();
  this.type = typeof value;

  try {
    this.value = value.toString();
  } catch(e) {}

  if (this.type == 'object') {
    this.subtype = _getSubtype(value);
  }
}

// The object store used to reference objects by internally generated id.
var _objectStore = {};
var _nextObjectId = 0;

// The last evaluated result.
var _lastResult = null;

// Values in the global scope that have been overridden by corresponding
// members of the Command Line API for the duration of an evaluation. We use
// this to restore the original values after the evaluation.
var _globalOverrides = {};

// Command Line API implementation.
// This is a set of convenience variables/functions that are not present in
// the global scope by default, but can be specified as available to the
// Runtime.evaluate function by the includeCommandLineAPI parameter.
// https://developers.google.com/web/tools/chrome-devtools/console/utilities
var _commandLineAPI = {};

_commandLineAPI.$_ = _lastResult;

_commandLineAPI.$ = document.querySelector.bind(document);

_commandLineAPI.$$ = document.querySelectorAll.bind(document);

_commandLineAPI.keys = Object.keys;

_commandLineAPI.values = function(object) {
  var keys = Object.keys(object);
  var result = [];
  for (var i = 0; i < keys.length; i++) {
    result.push(object[keys[i]]);
  }
  return result;
}

var _addCommandLineAPI = function() {
  _commandLineAPI.$_ = _lastResult;
  for (var property in _commandLineAPI) {
    if (_commandLineAPI.hasOwnProperty(property)) {
      _globalOverrides[property] = window[property];
      window[property] = _commandLineAPI[property];
    }
  }
}

var _removeCommandLineAPI = function() {
  for (var property in _globalOverrides) {
    if (_globalOverrides.hasOwnProperty(property)) {
      window[property] = _globalOverrides[property];
      delete _globalOverrides[property];
    }
  }
}

// TODO: Pass debugBackend from C++ instead of getting it from the window.
})(window.debugBackend);
