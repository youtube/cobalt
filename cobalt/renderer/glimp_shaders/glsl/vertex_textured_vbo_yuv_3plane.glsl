attribute vec3 a_position;
attribute vec2 a_tex_coord;
varying vec2 v_tex_coord_y;
varying vec2 v_tex_coord_u;
varying vec2 v_tex_coord_v;
uniform vec4 scale_translate_y;
uniform vec4 scale_translate_u;
uniform vec4 scale_translate_v;
uniform mat4 model_view_projection_transform;

void main() {
  gl_Position = model_view_projection_transform * vec4(a_position.xyz, 1.0);
  v_tex_coord_y = a_tex_coord * scale_translate_y.xy + scale_translate_y.zw;
  v_tex_coord_u = a_tex_coord * scale_translate_u.xy + scale_translate_u.zw;
  v_tex_coord_v = a_tex_coord * scale_translate_v.xy + scale_translate_v.zw;
}