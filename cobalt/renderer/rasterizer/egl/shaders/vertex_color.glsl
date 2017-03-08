uniform vec4 u_clip_adjustment;
uniform mat3 u_view_matrix;
attribute vec3 a_position;
attribute vec4 a_color;
varying vec4 v_color;
void main() {
  vec3 pos2d = u_view_matrix * vec3(a_position.xy, 1);
  gl_Position = vec4(pos2d.xy * u_clip_adjustment.xy +
                     u_clip_adjustment.zw, a_position.z, pos2d.z);
  v_color = a_color;
}
