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
    var objectEntry = this.objectStore[params.objectId];
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
    // Use |eval| indirectly, to cause evaluation at global scope.
    // This is so subsequent calls can access variables declared here, etc.
    var geval = eval;
    value = geval(params.expression);
    result.wasThrown = false;
  } catch(e) {
    value = e;
    result.exceptionDetails = e;
    result.wasThrown = true;
  }

  result.result =
      new this.RemoteObject(value, params.objectGroup, params.returnByValue);
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
    var objectEntry = this.objectStore[params.objectId];
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
  delete this.objectStore[params.objectId];
}

// Releases our references to a group of previously accessed objects.
// https://developer.chrome.com/devtools/docs/protocol/1.1/runtime#command-releaseObjectGroup
devtoolsBackend.runtime.releaseObjectGroup = function(params) {
  for (var objectId in this.objectStore) {
    var objectEntry = this.objectStore[objectId];
    if (objectEntry && objectEntry.objectGroup == params.objectGroup) {
      delete this.objectStore[objectId];
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
  return this.objectStore[objectId].object;
}

// Adds an object to the internal object store and returns a unique id that can
// be used to access it again.
devtoolsBackend.runtime.addObject = function(object, objectGroup) {
  // If we've already added this object, then use the same objectId.
  for (var objectId in this.objectStore) {
    var objectEntry = this.objectStore[objectId];
    if (objectEntry.object === object &&
        objectEntry.objectGroup == objectGroup) {
      return objectId;
    }
  }

  var objectId = this.nextObjectId.toString();
  this.nextObjectId += 1;
  this.objectStore[objectId] = {};
  this.objectStore[objectId].object = object;
  this.objectStore[objectId].objectGroup = objectGroup;
  return objectId;
}

// Creates a RemoteObject, which is the type used to return many values to
// devtools. If |value| is an object, then is it inserted into
// |devtoolsBackend.runtime.objectStore| and the |objectId| key used to
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

  try {
    this.description = value.toString();
  } catch(e) {
    this.description = JSON.stringify(value);
  }

  if (value.contructor) {
    this.className = value.constructor.name;
  }

  if (this.type == 'object') {
    this.objectId = devtoolsBackend.runtime.addObject(value, objectGroup);
    if (value instanceof Array) {
      this.subtype = 'array';
    } else if (value instanceof Date) {
      this.subtype = 'date';
    } else if (value instanceof Error) {
      this.subtype = 'error';
    } else if (value instanceof Node) {
      this.subtype = 'node';
    } else if (value instanceof RegExp) {
      this.subtype = 'regexp';
    }
  }

  if (returnByValue || this.type != 'object') {
    this.value = value;
  }
}

// Creates a PropertyDescriptor for |property| of |object|,
// which is the type used to return object properties to devtools.
// Some properties may be objects, in which case new RemoteObjects are created
// and inserted into |devtoolsBackend.runtime.objectStore| using the specified
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

// The object store used to reference objects by internally generated id.
devtoolsBackend.runtime.objectStore = {};
devtoolsBackend.runtime.nextObjectId = 0;
