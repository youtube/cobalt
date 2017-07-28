attribute vec3 a_position;
attribute vec2 a_tex_coord;
varying vec2 v_tex_coord_uyvy;
uniform vec4 scale_translate_uyvy;
uniform mat4 model_view_projection_transform;

void main() {
  gl_Position = model_view_projection_transform * vec4(a_position.xyz, 1.0);
  v_tex_coord_uyvy = a_tex_coord * scale_translate_uyvy.xy + scale_translate_uyvy.zw;
}