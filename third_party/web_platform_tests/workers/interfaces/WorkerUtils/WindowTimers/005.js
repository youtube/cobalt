self.close();
// TODO (b/233788170): the below setTimeout should be a setInterval to match the
// chromium web platform tests, but setInterval doesn't properly stop execution
// when the window closes yet.
var t = setTimeout(function () { }, 10);
postMessage(t);
