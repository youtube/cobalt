/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Top level callback function that can be called by any component to create
// a Runtime.RemoteObject from a JS object passed in from C++.
devtoolsBackend.createRemoteObjectCallback = function(object, params) {
  var remoteObject = null;
  try {
    remoteObject = new devtoolsBackend.runtime.RemoteObject(
        object, params.objectGroup, params.returnByValue);
  } catch(e) {
    console.log('Exception creating remote object: ' + e);
  }

  var json = "";
  try {
    json = JSON.stringify(remoteObject);
  } catch(e) {
    console.log('Exception stringifying remote object: ' + e)
  }
  return json;
}

// JavaScript functions used by the Chrome debugging protocol Runtime domain:
// https://developer.chrome.com/devtools/docs/protocol/1.1/runtime

devtoolsBackend.runtime = {};

// Calls a function on a previously accessed RemoteObject with an argument list
// and returns a new RemoteObject. Used extensively by devtools for
// auto-completion. The new RemoteObject uses the same |objectGroup| as the
// original object.
// https://developer.chrome.com/devtools/docs/protocol/1.1/runtime#command-callFunctionOn
devtoolsBackend.runtime.callFunctionOn = function(params) {
  var result = {};
  var value = null;
  try {
    eval('var f = ' + params.functionDeclaration);
    var objectEntry = this._objectStore[params.objectId];
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
      new this.RemoteObject(value, objectGroup, params.returnByValue);
  return JSON.stringify(result);
}

// Evaluates a string and returns a RemoteObject.
// https://developer.chrome.com/devtools/docs/protocol/1.1/runtime#command-evaluate
devtoolsBackend.runtime.evaluate = function(params) {
  var result = {};
  var value = null;
  try {
    if (params.includeCommandLineAPI) {
      this._addCommandLineAPI();
    }

    // Use |eval| indirectly, to cause evaluation at global scope.
    // This is so subsequent calls can access variables declared here, etc.
    var geval = eval;
    value = geval(params.expression);

    // Store the last result if we're doing a "real" evaluation, not an
    // auto-complete. Seems a little special case-y, but this is taken from
    // Chrome's implementation, and looks like the only way to differentiate.
    if (params.objectGroup == 'console') {
      this._lastResult = value;
    }

    result.wasThrown = false;
  } catch(e) {
    value = e;
    result.exceptionDetails = e;
    result.wasThrown = true;
  }

  // Create the RemoteObject corresponding to the result.
  result.result =
      new this.RemoteObject(value, params.objectGroup, params.returnByValue);

  // Add the preview, if requested.
  if (params.generatePreview) {
    var preview = this.generatePreview(value);
    if (preview) {
      result.result.preview = preview;
    }
  }

  if (params.includeCommandLineAPI) {
    this._removeCommandLineAPI();
  }

  return JSON.stringify(result);
}

// Returns the properties of a previously accessed object as an array of
// PropertyDescriptor objects.
// The parameters specifify several options:
// * ownProperties - only include immediate properties, not the prototype chain.
// * accessorPropertiesOnly - only include accessor properties.
// https://developer.chrome.com/devtools/docs/protocol/1.1/runtime#runtime.getproperties
devtoolsBackend.runtime.getProperties = function(params) {
  var result = {};
  var properties = [];
  try {
    var objectEntry = this._objectStore[params.objectId];
    var object = objectEntry.object;
    var objectGroup = objectEntry.objectGroup;
    this.addProperties(object, objectGroup, !params.ownProperties, properties);

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
// https://developer.chrome.com/devtools/docs/protocol/1.1/runtime#command-releaseObject
devtoolsBackend.runtime.releaseObject = function(params) {
  delete this._objectStore[params.objectId];
}

// Releases our references to a group of previously accessed objects.
// https://developer.chrome.com/devtools/docs/protocol/1.1/runtime#command-releaseObjectGroup
devtoolsBackend.runtime.releaseObjectGroup = function(params) {
  for (var objectId in this._objectStore) {
    var objectEntry = this._objectStore[objectId];
    if (objectEntry && objectEntry.objectGroup == params.objectGroup) {
      delete this._objectStore[objectId];
    }
  }
}

// Adds the properties of |object| to the |result| array as new
// PropertyDescriptor objects. Some properties may be objects themselves,
// in which case new RemoteObjects are created using the specified
// |objectGroup|, which should be that of |object|.
// If |includePrototype| is set, the function will be called recursively on
// the prototype chain of the object.
devtoolsBackend.runtime.addProperties =
    function(object, objectGroup, includePrototype, result) {
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
        new this.PropertyDescriptor(object, objectGroup, key);
    result.push(propertyDescriptor);
  }

  var proto = null;
  try {
    proto = Object.getPrototypeOf(object);
  } catch (e) {}

  if (includePrototype) {
    // Recursively add the properties from the prototype chain.
    if (proto) {
      this.addProperties(proto, objectGroup, includePrototype, result);
    }
  } else if (proto && !foundProto) {
    // |getOwnPropertyNames| may not include the object prototype,
    // so if that's the case, add it now. It's a deprecated name, but devtools
    // still uses it.
    var propertyDescriptor =
        new this.PropertyDescriptor(object, objectGroup, '__proto__');
    result.push(propertyDescriptor);
  }
}

// Gets an object from the internal object store.
devtoolsBackend.runtime.getObject = function(objectId) {
  return this._objectStore[objectId].object;
}

// Adds an object to the internal object store and returns a unique id that can
// be used to access it again.
devtoolsBackend.runtime.addObject = function(object, objectGroup) {
  // If we've already added this object, then use the same objectId.
  for (var objectId in this._objectStore) {
    var objectEntry = this._objectStore[objectId];
    if (objectEntry.object === object &&
        objectEntry.objectGroup == objectGroup) {
      return objectId;
    }
  }

  var objectId = this._nextObjectId.toString();
  this._nextObjectId += 1;
  this._objectStore[objectId] = {};
  this._objectStore[objectId].object = object;
  this._objectStore[objectId].objectGroup = objectGroup;
  return objectId;
}

// Generates an object preview, which may be requested for the evaluate
// command.
devtoolsBackend.runtime.generatePreview = function(object) {
  if (!object || (typeof object != 'object')) {
    return null;
  } else {
    return new this.ObjectPreview(object);
  }
}

// Returns the subtype of an object, or null if the specified value is not an
// object.
devtoolsBackend.runtime.getSubtype = function(object) {
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
devtoolsBackend.runtime.getClassName = function(object) {
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

// Creates a RemoteObject, which is the type used to return many values to
// devtools. If |value| is an object, then is it inserted into
// |devtoolsBackend.runtime._objectStore| and the |objectId| key used to
// access it is included in the RemoteObject. If |value| is not an object,
// or |returnByValue| is true, then |value| is directly included in the
// RemoteObject.
devtoolsBackend.runtime.RemoteObject = function(value, objectGroup,
                                                returnByValue) {
  this.type = typeof value;

  if (value == null) {
    this.subtype == 'null';
    this.value = null;
    return;
  }

  if (this.type == 'object') {
    this.objectId = devtoolsBackend.runtime.addObject(value, objectGroup);
    this.subtype = devtoolsBackend.runtime.getSubtype(value);
    this.className = devtoolsBackend.runtime.getClassName(value);
  }

  // Fill in the description field. Devtools will only display arrays
  // correctly if their description follows a particular format. For other
  // values, try to use the generic string conversion, and fall back to the
  // className if that fails.
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

// Creates a PropertyDescriptor for |property| of |object|,
// which is the type used to return object properties to devtools.
// Some properties may be objects, in which case new RemoteObjects are created
// and inserted into |devtoolsBackend.runtime._objectStore| using the specified
// |objectGroup|, which should be that of |object|.
devtoolsBackend.runtime.PropertyDescriptor = function(object, objectGroup,
                                                      property) {
  this.name = property.toString();
  var descriptor = Object.getOwnPropertyDescriptor(object, property);
  // Some Cobalt objects don't seem to support |getOwnPropertyDescriptor|,
  // so we handle that case in the else clause below.
  if (descriptor) {
    this.configurable = descriptor.configurable;
    this.enumerable = descriptor.enumerable;
    if (descriptor.get) {
      this.get = new devtoolsBackend.runtime.RemoteObject(descriptor.get,
                                                          objectGroup,
                                                          false);
    }
    if (descriptor.set) {
      this.set = new devtoolsBackend.runtime.RemoteObject(descriptor.set,
                                                          objectGroup,
                                                          false);
    }
    if (descriptor.value != null) {
      this.value = new devtoolsBackend.runtime.RemoteObject(descriptor.value,
                                                            objectGroup,
                                                            false);
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
    this.value = new devtoolsBackend.runtime.RemoteObject(object[property],
                                                          objectGroup, false);
  }
}

// Creates an ObjectPreview, the type to represent a preview of an object,
// which may be requested by devtools in the evaluate command.
devtoolsBackend.runtime.ObjectPreview = function(value) {
  this.type = typeof value;
  this.subtype = devtoolsBackend.runtime.getSubtype(value);
  this.lossless = true;
  this.overflow = false;
  this.properties = [];

  // Use the className as the preview description. This matches Chrome.
  this.description = devtoolsBackend.runtime.getClassName(value);

  // If we have an array-like object, add the array items, or append the
  // length to the description if there's too many.
  if (value.length != null) {
    var MAX_ARRAY_ITEMS = 99;
    if (value.length <= MAX_ARRAY_ITEMS) {
      for (var i = 0; i < value.length; i++) {
        var property = new devtoolsBackend.runtime.PropertyPreview(i, value[i]);
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
      var property = new devtoolsBackend.runtime.PropertyPreview(name,
                                                                 value[name]);
      this.properties.push(property);
    }
  }
}

// Creates a PropertyPreview, the type to represent a preview of a single
// object property.
devtoolsBackend.runtime.PropertyPreview = function(name, value) {
  this.name = name.toString();
  this.type = typeof value;

  try {
    this.value = value.toString();
  } catch(e) {}

  if (this.type == 'object') {
    this.subtype = devtoolsBackend.runtime.getSubtype(value);
  }
}

// The object store used to reference objects by internally generated id.
devtoolsBackend.runtime._objectStore = {};
devtoolsBackend.runtime._nextObjectId = 0;

// The last evaluated result.
devtoolsBackend.runtime._lastResult = null;

// Values in the global scope that have been overridden by corresponding
// members of the Command Line API for the duration of an evaluation. We use
// this to restore the original values after the evaluation.
devtoolsBackend.runtime._globalOverrides = {};

// Command Line API implementation.
// This is a set of convenience variables/functions that are not present in
// the global scope by default, but can be specified as available to the
// Runtime.evaluate function by the includeCommandLineAPI parameter.
// https://developers.google.com/web/tools/chrome-devtools/debug/command-line/command-line-reference
devtoolsBackend.runtime._commandLineAPI = {};

devtoolsBackend.runtime._commandLineAPI.$_ =
    devtoolsBackend.runtime._lastResult;

devtoolsBackend.runtime._commandLineAPI.$ =
    document.querySelector.bind(document);

devtoolsBackend.runtime._commandLineAPI.$$ =
    document.querySelectorAll.bind(document);

devtoolsBackend.runtime._commandLineAPI.keys = Object.keys;

devtoolsBackend.runtime._commandLineAPI.values = function(object) {
  var keys = Object.keys(object);
  var result = [];
  for (var i = 0; i < keys.length; i++) {
    result.push(object[keys[i]]);
  }
  return result;
}

devtoolsBackend.runtime._addCommandLineAPI = function() {
  this._commandLineAPI.$_ = this._lastResult;
  for (var property in this._commandLineAPI) {
    if (this._commandLineAPI.hasOwnProperty(property)) {
      this._globalOverrides[property] = window[property];
      window[property] = this._commandLineAPI[property];
    }
  }
}

devtoolsBackend.runtime._removeCommandLineAPI = function() {
  for (var property in this._globalOverrides) {
    if (this._globalOverrides.hasOwnProperty(property)) {
      window[property] = this._globalOverrides[property];
      delete this._globalOverrides[property];
    }
  }
}
