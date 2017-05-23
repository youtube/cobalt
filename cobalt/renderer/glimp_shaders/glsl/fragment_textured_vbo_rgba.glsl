precision mediump float;
varying vec2 v_tex_coord_rgba;
uniform sampler2D texture_rgba;
uniform mat4 to_rgb_color_matrix;

void main() {
  vec4 untransformed_color = vec4(texture2D(texture_rgba, v_tex_coord_rgba).rgba);
  gl_FragColor = untransformed_color * to_rgb_color_matrix;
}