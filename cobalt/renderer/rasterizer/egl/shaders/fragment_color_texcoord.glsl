precision mediump float;
uniform vec4 u_texcoord_clamp;
uniform sampler2D u_texture;
varying vec4 v_color;
varying vec2 v_texcoord;
void main() {
  gl_FragColor = v_color * texture2D(u_texture,
      clamp(v_texcoord, u_texcoord_clamp.xy, u_texcoord_clamp.zw));
}
