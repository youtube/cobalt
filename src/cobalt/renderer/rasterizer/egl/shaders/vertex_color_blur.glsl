uniform vec4 u_clip_adjustment;
uniform mat3 u_view_matrix;
attribute vec3 a_position;
attribute vec4 a_color;
attribute vec2 a_blur_position;
varying vec4 v_color;
varying vec2 v_blur_position;
void main() {
  vec3 pos2d = u_view_matrix * vec3(a_position.xy, 1);
  gl_Position = vec4(pos2d.xy * u_clip_adjustment.xy +
                     u_clip_adjustment.zw, a_position.z, pos2d.z);
  v_color = a_color;
  v_blur_position = a_blur_position;
}
