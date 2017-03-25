# Enabling Spherical Video in Cobalt

Cobalt supports playback of 360 spherical videos.  Cobalt does not expose
this support to web applications through WebGL (which is currently
unimplemented in Cobalt), but rather through a custom `map-to-mesh` CSS
filter and custom [`window.camera3D` Web API](../dom/camera_3d.idl). Support
for spherical video in Cobalt requires a GLES rasterizer (i.e. it is not
supported for the Starboard Blitter API), and Starboard platform support for
the player
[decode-to-texture output mode](../starboard/doc/howto_decode_to_texture.md).

## Enabling spherical video support

Spherical video support requires `map-to-mesh` support, which must be
explicitly enabled in your Starboard platform's `gyp_configuration.gypi` file.

In particular, your platform's `gyp_configuration.gypi` file should contain the
line,

```
'enable_map_to_mesh': 1,
```

When `enable_map_to_mesh` is set to 1, Cobalt will make the `map-to-mesh`
CSS filter parseable.  The web app can then detect whether the browser,
Cobalt, supports spherical video by evaluating the following JavaScript:

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

Finally, it is required that your platform provides
[decode-to-texture support](../starboard/doc/howto_decode_to_texture.md).
