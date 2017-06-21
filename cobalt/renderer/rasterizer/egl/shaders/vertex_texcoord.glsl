uniform vec4 u_clip_adjustment;
uniform mat3 u_view_matrix;
attribute vec2 a_position;
attribute vec2 a_texcoord;
varying vec2 v_texcoord;
void main() {
  vec3 pos2d = u_view_matrix * vec3(a_position, 1);
  gl_Position = vec4(pos2d.xy * u_clip_adjustment.xy +
                     u_clip_adjustment.zw, 0, pos2d.z);
  v_texcoord = a_texcoord;
}
