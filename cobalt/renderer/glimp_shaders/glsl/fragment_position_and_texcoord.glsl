precision mediump float;
varying vec2 v_tex_coord;
uniform sampler2D tex;

void main() {
  gl_FragColor = texture2D(tex, v_tex_coord);
}
