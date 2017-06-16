precision mediump float;
uniform vec4 u_include;   // include scissor (x_min, y_min, x_max, y_max)
varying vec2 v_offset;
varying vec4 v_color;
void main() {
  vec2 include = step(u_include.xy, v_offset) * step(v_offset, u_include.zw);
  gl_FragColor = v_color * include.x * include.y;
}
