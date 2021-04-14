precision mediump float;
varying vec2 v_tex_coord_y;
varying vec2 v_tex_coord_uv;
uniform sampler2D texture_y;
uniform sampler2D texture_uv;
uniform mat4 to_rgb_color_matrix;

void main() {
  vec4 untransformed_color = vec4(
      texture2D(texture_y, v_tex_coord_y).a,
      texture2D(texture_uv, v_tex_coord_uv).ba, 1.0);

  vec4 color = untransformed_color * to_rgb_color_matrix;
  gl_FragColor = color;
}
