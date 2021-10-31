precision mediump float;
varying vec2 v_tex_coord_uyvy;
uniform sampler2D texture_uyvy;
uniform mat4 to_rgb_color_matrix;
uniform ivec2 texture_size_uyvy;

void main() {
  float texture_space_x = float(texture_size_uyvy.x) * v_tex_coord_uyvy.x;
  texture_space_x = clamp(
      texture_space_x, 0.25, float(texture_size_uyvy.x) - 0.25);

  float texel_step_u = 1.0 / float(texture_size_uyvy.x);
  float sample_1_texture_space = floor(texture_space_x - 0.5) + 0.5;
  float sample_1_normalized =      sample_1_texture_space * texel_step_u;
  float sample_2_normalized = sample_1_normalized + texel_step_u;
  vec4 sample_1 =
      texture2D(texture_uyvy, vec2(sample_1_normalized, v_tex_coord_uyvy.y));
  vec4 sample_2 =
      texture2D(texture_uyvy, vec2(sample_2_normalized, v_tex_coord_uyvy.y));
  float lerp_progress = texture_space_x - sample_1_texture_space;
  vec2 uv_value = mix(sample_1.rb, sample_2.rb, lerp_progress);

  float y_value;
  if (lerp_progress < 0.25) {
    y_value = mix(sample_1.g, sample_1.a, lerp_progress * 2.0 + 0.5);
  } else if (lerp_progress < 0.75) {
    y_value = mix(sample_1.a, sample_2.g, lerp_progress * 2.0 - 0.5);
  } else {
    y_value = mix(sample_2.g, sample_2.a, lerp_progress * 2.0 - 1.5);
  }

  vec4 untransformed_color = vec4(y_value, uv_value.r, uv_value.g, 1.0);
  gl_FragColor = untransformed_color * to_rgb_color_matrix;
}
