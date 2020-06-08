These files define the inspector protocol, not necessarily what is implemented in Cobalt.
https://chromedevtools.github.io/devtools-protocol/

These files were taken from Blink in the Chromium repo to correspond to the version of DevTools we rebased to.

DevTools version:
https://chromium.googlesource.com/devtools/devtools-frontend.git/+log/refs/heads/chromium/3987
https://github.com/ChromeDevTools/devtools-frontend/commit/757e0e1e1ffc4a0d36d005d120de5f73c1b910e0

DevTools protocol:
https://chromium.googlesource.com/chromium/src.git/+/refs/tags/80.0.3987.148/third_party/blink/public/devtools_protocol/
  inspector/BUILD.gn (rules ported to GYP)
  inspector/browser_protocol-1.3.json
  inspector/browser_protocol.pdl

Properties:
https://chromium.googlesource.com/chromium/src.git/+/refs/tags/80.0.3987.148/third_party/blink/renderer/core/
  html/aria_properties.json5
  css/CSSProperties.json5
