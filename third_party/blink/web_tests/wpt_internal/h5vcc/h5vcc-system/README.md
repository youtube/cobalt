# Web tests for H5vccSystem

These automated tests rely on a test-only interface that must be provided by the
browser under test. The pattern from the Serial API is followed: an
implementation of the H5vccSystem Mojo interface is provided by
`../../resources/chromium/cobalt/fake-h5vcc-system-impl.js`.
