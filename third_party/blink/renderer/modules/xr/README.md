# WebXR Blink Module
_For a more thorough/high level overview of the entire WebXR stack, please refer to
[components/webxr][components-webxr-readme]_

The WebXR API enables Virtual Reality (VR) and Augmented Reality (AR) features on the Web.

WebXR and its associated modules are developed by the Immersive Web W3C
[Working Group][working-group] and [Community Group][community-group].

This Blink module implements the "core" [WebXR Device API][webxr-spec], as well as the following
WebXR modules:

 - [Gamepads][gamepads-spec]
 - [Augmented Reality][ar-spec]
 - [Hit Test][hit-test-spec]
 - [DOM Overlays][dom-overlays-spec]
 - [Anchors][anchors-spec]
 - [Lighting Estimation][lighting-spec]
 - [Hand Input][hand-input-spec]
 - [Layers][layers-spec]
 - [Depth Sensing][depth-spec]
 - [Plane Detection][plane-spec]

The Blink implementation communicates with the browser process using the
`VRService` [Mojo interface][mojom-readme]. The browser process (implemented in
[content/browser/xr/][browser-readme]) then coordinates with the low-level
device runtimes (implemented in [device/vr/][device-readme]) to support
the WebXR features.

[components-webxr-readme]: https://source.chromium.org/chromium/chromium/src/+/main:components/webxr/README.md
[working-group]: https://www.w3.org/immersive-web/
[community-group]: https://www.w3.org/community/immersive-web/
[webxr-spec]: https://www.w3.org/TR/webxr/
[gamepads-spec]: https://www.w3.org/TR/webxr-gamepads-module-1/
[ar-spec]: https://www.w3.org/TR/webxr-ar-module-1/
[hit-test-spec]: https://immersive-web.github.io/hit-test/
[dom-overlays-spec]: https://immersive-web.github.io/dom-overlays/
[anchors-spec]: https://immersive-web.github.io/anchors/
[lighting-spec]: https://immersive-web.github.io/lighting-estimation/
[hand-input-spec]: https://immersive-web.github.io/webxr-hand-input/
[layers-spec]: https://immersive-web.github.io/webxr-layers/
[depth-spec]: https://immersive-web.github.io/webxr-depth-sensing/
[plane-spec]: https://immersive-web.github.io/plane-detection/
[mojom-readme]: https://source.chromium.org/chromium/chromium/src/+/main:device/vr/public/mojom/README.md
[browser-readme]: https://source.chromium.org/chromium/chromium/src/+/main:content/browser/xr/README.md
[device-readme]: https://source.chromium.org/chromium/chromium/src/+/main:device/vr/README.md
