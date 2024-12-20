# Web tests for CrashAnnotator

These automated tests rely on a test-only interface that must be provided by the
browser under test. The pattern from the Serial API is followed: an
implementation of the CrashAnnotator Mojo interface is provided by
`../../resources/chromium/cobalt/fake-crash-annotator.js`.
