attribute vec3 a_position;
attribute vec2 a_tex_coord;
varying vec2 v_tex_coord;
uniform vec2 u_tex_offset;
uniform vec2 u_tex_multiplier;
uniform mat4 u_mvp_matrix;
void main() {
  gl_Position = u_mvp_matrix * vec4(a_position.xyz, 1.0);
  v_tex_coord.x = a_tex_coord.x * u_tex_multiplier.x + u_tex_offset.x;
  v_tex_coord.y = a_tex_coord.y * u_tex_multiplier.y + u_tex_offset.y;
}
