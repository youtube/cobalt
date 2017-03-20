attribute vec3 a_position;
attribute vec2 a_tex_coord;
varying vec2 v_tex_coord;
uniform mat3 u_tex_transform;
uniform mat4 u_mvp_matrix;
void main() {
  gl_Position = u_mvp_matrix * vec4(a_position.xyz, 1.0);
  vec3 tex_coord = u_tex_transform * vec3(a_tex_coord, 1.0);
  v_tex_coord = tex_coord.xy;
}
