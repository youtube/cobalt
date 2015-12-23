precision mediump float;
varying vec2 v_tex_coord;
uniform sampler2D texture;

void main() {
  gl_FragColor = texture2D(texture, v_tex_coord);
}
