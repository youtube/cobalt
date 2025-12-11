## WebRTC API Adapters

This directory contains code that wraps components of the WebRTC native API in
classes that expose an API specialized for implementing the Web API. Some of
these adapters also allow the WebRTC component to be safely used from a remote
thread.

### Blink Types

Blink types (like AtomicString, HeapVector, etc.) should not be used inside this
directory. The bindings in the parent directory are responsible for converting
the Blink binding types to the corresponding STL/WebRTC types before interacting
with the adapters.
