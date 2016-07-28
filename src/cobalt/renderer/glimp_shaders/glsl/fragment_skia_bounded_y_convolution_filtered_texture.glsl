#version 100
precision mediump float;
uniform sampler2D uSampler0_Stage0;
uniform vec2 uImageIncrement_Stage0;
uniform vec2 uBounds_Stage0;
uniform float uKernel_Stage0[25];
varying vec4 vColor;
varying vec2 vMatrixCoord_Stage0;

void main() {
  vec4 output_Stage0;
  {
    // Stage 0: Convolution
    output_Stage0 = vec4(0, 0, 0, 0);
    vec2 coord = vMatrixCoord_Stage0 - 12.0 * uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[0];
    coord += uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[1];
    coord += uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[2];
    coord += uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[3];
    coord += uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[4];
    coord += uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[5];
    coord += uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[6];
    coord += uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[7];
    coord += uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[8];
    coord += uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[9];
    coord += uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[10];
    coord += uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[11];
    coord += uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[12];
    coord += uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[13];
    coord += uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[14];
    coord += uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[15];
    coord += uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[16];
    coord += uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[17];
    coord += uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[18];
    coord += uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[19];
    coord += uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[20];
    coord += uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[21];
    coord += uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[22];
    coord += uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[23];
    coord += uImageIncrement_Stage0;
    output_Stage0 += texture2D(uSampler0_Stage0, coord) * float(coord.y >= uBounds_Stage0.x && coord.y <= uBounds_Stage0.y) * uKernel_Stage0[24];
    coord += uImageIncrement_Stage0;
    output_Stage0 *= vColor;
  }

  gl_FragColor = output_Stage0;
}
