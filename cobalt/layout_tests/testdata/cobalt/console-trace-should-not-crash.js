// Call from external JavaScript file.
//
// Expected output:
//   f @ console-trace-should-not-crash.js:8
//   (anonymous function) @ console-trace-should-not-crash.js:11

function f() {
  console.trace();
}

f();
