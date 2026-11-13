// META: global=window

test(() => {
  assert_implements(window.h5vcc, "window.h5vcc not supported");
  assert_implements(window.h5vcc.system, "window.h5vcc.system not supported");
  assert_equals(
      typeof window.h5vcc.system.isCleanExitBeaconFlagged,
      "function",
      "window.h5vcc.system.isCleanExitBeaconFlagged is a function");
  const result = window.h5vcc.system.isCleanExitBeaconFlagged();
  assert_false(
      result instanceof Promise,
      "isCleanExitBeaconFlagged() does not return a Promise");
  assert_equals(
      typeof result,
      "boolean",
      "isCleanExitBeaconFlagged() returns a synchronous boolean");
}, "exercises H5vccSystem.isCleanExitBeaconFlagged()");
