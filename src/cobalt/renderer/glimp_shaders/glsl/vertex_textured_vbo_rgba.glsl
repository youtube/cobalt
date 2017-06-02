attribute vec3 a_position;
attribute vec2 a_tex_coord;
varying vec2 v_tex_coord_rgba;
uniform vec4 scale_translate_rgba;
uniform mat4 model_view_projection_transform;

void main() {
  gl_Position = model_view_projection_transform * vec4(a_position.xyz, 1.0);
  v_tex_coord_rgba =
      a_tex_coord * scale_translate_rgba.xy + scale_translate_rgba.zw;
}