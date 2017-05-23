precision mediump float;
varying vec2 v_tex_coord_y;
varying vec2 v_tex_coord_u;
varying vec2 v_tex_coord_v;
uniform sampler2D texture_y;
uniform sampler2D texture_u;
uniform sampler2D texture_v;
uniform mat4 to_rgb_color_matrix;
void main() {
  vec4 untransformed_color = vec4(
    texture2D(texture_y, v_tex_coord_y).a,
    texture2D(texture_u, v_tex_coord_u).a,
    texture2D(texture_v, v_tex_coord_v).a, 1.0);
  gl_FragColor = untransformed_color * to_rgb_color_matrix;
}
