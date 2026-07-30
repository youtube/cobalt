# WebXR Component

## WebXR Overview
The web-exposed interface to WebXR begins in [Blink][blink-readme].
This code, with the help of the `VRService` [mojom interface][mojom-readme],
talks with the [browser process][browser-readme] to broker a connection
directly with the corresponding [device code][device-readme].

Note that this device code is often hosted in a separate XR utility process,
and thus the [isolated_xr_device service][service-readme] needs to assist the
browser in brokering these connections. The code that talks directly with a
device or its corresponding SDK/API (e.g. OpenXR) is often referred to as a
"Runtime" throughout XR code. It is responsible for querying or formatting the
data into/out of the expected WebXR formats.

## Component Code
This component code may depend on code in both //device and //content. It is
intended for code that is necessary for a given runtime to work, but cannot be
added under //device due to layering violations. Often this is because there may
need to be customizable extension points added for different embedders. This
includes code such as rendering utilizing the viz framework, or extension
methods for embedders to customize the install flow for some runtimes.

[blink-readme]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/modules/xr/README.md
[mojom-readme]: https://source.chromium.org/chromium/chromium/src/+/main:device/vr/public/mojom/README.md
[browser-readme]: https://source.chromium.org/chromium/chromium/src/+/main:content/browser/xr/README.md
[device-readme]: https://source.chromium.org/chromium/chromium/src/+/main:device/vr/README.md
[service-readme]: https://source.chromium.org/chromium/chromium/src/+/main:content/services/isolated_xr_device/README.md
