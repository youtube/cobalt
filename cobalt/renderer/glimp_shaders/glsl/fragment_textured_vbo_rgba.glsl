precision mediump float;
varying vec2 v_tex_coord_rgba;
uniform sampler2D texture_rgba;

void main() {
  vec4 untransformed_color = vec4(texture2D(texture_rgba, v_tex_coord_rgba).rgba);
  vec4 color = untransformed_color;
  gl_FragColor = color;
}
