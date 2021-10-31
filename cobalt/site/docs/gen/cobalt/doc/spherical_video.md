---
layout: doc
title: "Enabling Spherical Video in Cobalt"
---
# Enabling Spherical Video in Cobalt

Cobalt supports playback of 360 spherical videos.  Cobalt does not expose
this support to web applications through WebGL (which is currently
unimplemented in Cobalt), but rather through a custom `map-to-mesh` CSS
filter and custom [`window.camera3D` Web API](../dom/camera_3d.idl). Support
for spherical video in Cobalt requires a GLES rasterizer (i.e. it is not
supported for the Starboard Blitter API), and Starboard platform support for
the player
[decode-to-texture output mode](../../starboard/doc/howto_decode_to_texture.md).

## Enabling spherical video support

Spherical video support requires `map-to-mesh` support, which is enabled by
default. You can explicitly disable it either through the command line switch
`--disable_map_to_mesh` or by implementing the CobaltExtensionGraphicsApi
function `IsMapToMeshEnabled()` to return `false`.

When `map-to-mesh` is supported, Cobalt will make the `map-to-mesh` CSS filter
parseable.  The web app can then detect whether the browser, Cobalt, supports
spherical video by evaluating the following JavaScript:

```
function checkForMapToMeshSupport() {
  return 'CSS' in window && 'supports' in window.CSS &&
         CSS.supports(
             'filter',
             'map-to-mesh(url(p.msh), 100deg 60deg,' +
                 'matrix3d(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1),' +
                 'monoscopic)');
}
```

It is required that your platform provides
[decode-to-texture support](../../starboard/doc/howto_decode_to_texture.md).

## Input

Cobalt currently supports input mappings from the following keys (defined in [starboard/key.h](../../starboard/key.h)):

 - `kSbKeyLeft`
 - `kSbKeyUp`
 - `kSbKeyRight`
 - `kSbKeyDown`
 - `kSbKeyGamepadDPadUp`
 - `kSbKeyGamepadDPadDown`
 - `kSbKeyGamepadDPadLeft`
 - `kSbKeyGamepadDPadRight`
 - `kSbKeyGamepadLeftStickUp`
 - `kSbKeyGamepadLeftStickDown`
 - `kSbKeyGamepadLeftStickLeft`
 - `kSbKeyGamepadLeftStickRight`
 - `kSbKeyGamepadRightStickUp`
 - `kSbKeyGamepadRightStickDown`
 - `kSbKeyGamepadRightStickLeft`
 - `kSbKeyGamepadRightStickRight`

Additionally, if your platform generates `kSbInputEventTypeMove` (from
[starboard/input.h](../../starboard/input.h)) events with
`SbInputData::position` set to values in the range `[-1, 1]`, for the following
keys,

 - `kSbKeyGamepadLeftStickUp`
 - `kSbKeyGamepadLeftStickDown`
 - `kSbKeyGamepadLeftStickLeft`
 - `kSbKeyGamepadLeftStickRight`
 - `kSbKeyGamepadRightStickUp`
 - `kSbKeyGamepadRightStickDown`
 - `kSbKeyGamepadRightStickLeft`
 - `kSbKeyGamepadRightStickRight`

then they will be treated as analog inputs when controlling the camera.
