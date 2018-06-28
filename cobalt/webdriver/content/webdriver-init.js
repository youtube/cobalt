// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

// Run the JSON deserialize algorithm on the value.
// https://www.w3.org/TR/webdriver/#dfn-json-deserialize
webdriverExecutor.deserialize = function(value) {
  if (value instanceof Array) {
    // Deserialize each of the array's values;
    var deserializedArray = [];
    var numArgs = value.length;
    for (var i = 0; i < numArgs; i++) {
      deserializedArray.push(webdriverExecutor.deserialize(value[i]));
    }
    return deserializedArray;
  } else if (value instanceof Object && value.ELEMENT) {
    // The argument is a WebElement, as denoted by the presence of the
    // ELEMENT property, so get the actual Element the id is mapped to.
    return webdriverExecutor.idToElement(value.ELEMENT);
  } else if (value instanceof Object) {
    // Deserialize each of the object's values.
    var deserializedObject = {};
    for (var key in value) {
      deserializedObject[key] = webdriverExecutor.deserialize(value[key]);
    }
    return deserializedObject;
  }
  return value;
};

// Run the JSON clone algorithm on the value.
// https://www.w3.org/TR/webdriver/#dfn-internal-json-clone-algorithm
webdriverExecutor.serialize = function(value, seen) {
  if (value === undefined || value === null) {
    return null;
  } else if (value instanceof Element) {
    var webElementId = webdriverExecutor.elementToId(value);
    return { "ELEMENT": webElementId };
  } else if (value instanceof Object) {
    if (seen.indexOf(value) != -1) {
      throw "Reference cycle found trying to serialize.";
    }
    seen.push(value);
    if (value instanceof Array || value instanceof NodeList
        || value instanceof HTMLCollection) {
      var serializedArray = [];
      var length = value.length;
      for (var i = 0; i < length; i++) {
        serializedArray.push(webdriverExecutor.serialize(value[i], seen));
      }
      return serializedArray;
    } else {
      // Deserialize each of the object's its values.
      var serializedObject = {};
      for (var key in value) {
        serializedObject[key] = webdriverExecutor.serialize(value[key], seen);
      }
      return serializedObject;
    }
  }
  return value;
};

// Set the executeScriptHarness callback function.
// The function takes a functionObject and a JSON string representing an array
// of arguments. The arguments are applied to the function |functionObject|.
webdriverExecutor.executeScriptHarness = function(params, resultHandler) {
  var parameters = webdriverExecutor.deserialize(JSON.parse(params.jsonArgs));
  if (params.asyncTimeout === null) {
    var result = params.functionObject.apply(window, parameters);
    resultHandler.onResult(JSON.stringify(webdriverExecutor.serialize(result, [])));
  } else {
    var timeoutCallback = function() {
      resultHandler.onTimeout();
    }
    var timerId = window.setTimeout(timeoutCallback, params.asyncTimeout);

    var resultCallback = function(result) {
      resultHandler.onResult(JSON.stringify(webdriverExecutor.serialize(result, [])));
      window.clearTimeout(timerId);
    }
    parameters.push(resultCallback);
    params.functionObject.apply(window, parameters);
  }
};
