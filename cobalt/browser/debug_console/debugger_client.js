function DebuggerClient() {
  this.DEBUGGER_DETACHED = 0;
  this.DEBUGGER_ATTACHING = 1;
  this.DEBUGGER_ATTACHED = 2;
  this.scripts = [];
  this.attachState = this.DEBUGGER_DETACHED;
  this.onAttachCallback = this.onAttach.bind(this);
  this.onEventCallback = this.onEvent.bind(this);
  this.executionContext = 0;
}

// Attaches to the debugger and listens for debug events.
// Enables the domains we care about here.
DebuggerClient.prototype.attach = function() {
  if (this.attachState == this.DEBUGGER_DETACHED) {
    this.attachState = this.DEBUGGER_ATTACHING;
    printToMessageLog(messageLog.INTERACTIVE,
                      'Attempting to attach to debugger...');
    this.scripts = [];
    debugHub.debugger.onEvent.addListener(this.onEventCallback);
    debugHub.debugger.attach(this.onAttachCallback);
    this.sendCommand('Console.enable');
    this.sendCommand('Runtime.enable');
    this.sendCommand('Debugger.enable');
  } else if (this.attachState == this.DEBUGGER_ATTACHING) {
    printToMessageLog(messageLog.INTERACTIVE,
                      'Still attempting to attach to debugger...');
  }
}

// Local method to list the parsed scripts the client has been notifed of
// via the |Debugger.scriptParsed| event. Maps the possibly very long script id
// from the debug server to a more human-readable 0-based index.
DebuggerClient.prototype.getScripts = function(scriptId) {
  for (var i in this.scripts) {
    var index = this.pad(i, 3);
    var scriptUrl = this.scripts[i].url;
    printToMessageLog(messageLog.INTERACTIVE, index + ': ' + scriptUrl);
  }
}

//--- Commands.

// Each debugger command has an associated callback to get the result.

DebuggerClient.prototype.getScriptSource = function(scriptId) {
  // If the id looks like an index into the local script array, look it up there.
  if (scriptId >= 0 && scriptId < this.scripts.length) {
    scriptId = this.scripts[scriptId].scriptId;
  }

  var method = 'Debugger.getScriptSource';
  var params = { 'scriptId': scriptId.toString() };
  var callback = this.getScriptSourceCallback.bind(this);
  this.sendCommand(method, params, callback);
}

DebuggerClient.prototype.getScriptSourceCallback = function(result) {
  var scriptSource = result.scriptSource;
  var lines = scriptSource.split('\n');
  for (var i = 0; i < lines.length; i++) {
    var index = this.pad(i + 1, 4);
    printToMessageLog(messageLog.INFO, index + ': ' + lines[i]);
  }
}

DebuggerClient.prototype.evaluate = function(expression) {
  var callback = this.evaluateCallback.bind(this);
  var method = 'Runtime.evaluate';
  var params = {};
  params.contextId = this.executionContext;
  params.expression = expression;
  params.generatePreview = true;
  params.includeCommandLineAPI = true;
  params.objectGroup = 'console';
  params.returnByValue = false;
  this.sendCommand(method, params, callback);
}

DebuggerClient.prototype.evaluateCallback = function(result) {
  if (result.wasThrown) {
    printToMessageLog(messageLog.ERROR,
                      'Uncaught ' + result.result.description);
  } else if (result.result.preview) {
    printToMessageLog(messageLog.INFO, result.result.preview.description);
    if (result.result.preview.properties) {
      for (var i = 0; i < result.result.preview.properties.length; ++i) {
        var property = result.result.preview.properties[i];
        printToMessageLog(messageLog.INFO,
                          '  ' + property.name + ': ' + property.value);
      }
    }
    if (result.result.preview.overflow) {
      printToMessageLog(messageLog.INFO, '  ...');
    }
  } else if (result.result.description) {
    printToMessageLog(messageLog.INFO, result.result.description);
  }
  printToMessageLog(messageLog.INFO, '');
}

// All debugger commands are routed through this method. Converts the command
// parameters into a JSON string to pass to the debug server.
DebuggerClient.prototype.sendCommand = function(method, commandParams,
                                                callback) {
  var jsonParams = JSON.stringify(commandParams);
  var commandCallback = this.commandCallback.bind(this, callback);
  debugHub.debugger.sendCommand(method, jsonParams, commandCallback);
}

// All command callbacks are routed through this method. Parses the JSON
// response from the debug server, checks for errors and passes on to the
// command-specific callback to handle the result.
DebuggerClient.prototype.commandCallback = function(callback, responseString) {
  var response = JSON.parse(responseString);

  if (response && response.error) {
    printToMessageLog(messageLog.ERROR, 'Error: ' + response.error.message);
  } else if (callback) {
    if (response) {
      callback(response.result);
    } else {
      callback(null);
    }
  }
}

//-- Events.

DebuggerClient.prototype.onAttach = function() {
  if (debugHub.debugger.lastError) {
    printToMessageLog(messageLog.WARNING, 'Could not attach to debugger.');
    this.attachState = this.DEBUGGER_DETACHED;
  } else {
    printToMessageLog(messageLog.INTERACTIVE, 'Debugger attached.');
    this.attachState = this.DEBUGGER_ATTACHED;
  }
}

// All events generated by the debug server are routed through this method.
// Parses the JSON string and passes on to the appropriate handler according to
// the method name.
DebuggerClient.prototype.onEvent = function(method, paramString) {
  var params = JSON.parse(paramString);
  if (method == 'Debugger.scriptParsed') {
    this.onScriptParsed(params);
  } else if (method == 'Inspector.detached') {
    this.onDetached(params);
  } else if (method == 'Runtime.executionContextCreated') {
    this.onExecutionContextCreated(params);
  }
}

DebuggerClient.prototype.onDetached = function() {
  printToMessageLog(messageLog.INTERACTIVE, 'Debugger detached.');
  this.attachState = this.DEBUGGER_DETACHED;
}

DebuggerClient.prototype.onExecutionContextCreated = function(params) {
  this.executionContext = params.context.id;
  printToMessageLog(messageLog.INFO,
                    'Execution context created: ' + this.executionContext);
}

DebuggerClient.prototype.onScriptParsed = function(params) {
  this.scripts.push(params);
}

//--- Utils.

DebuggerClient.prototype.pad = function(number, minLength) {
  var result = number.toString();
  while (result.length < minLength) {
    result = ' ' + result;
  }
  return result;
}