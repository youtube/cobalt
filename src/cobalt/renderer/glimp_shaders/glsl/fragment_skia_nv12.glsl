#version 100
precision mediump float;
uniform sampler2D uSampler0_Stage0;
uniform sampler2D uSampler1_Stage0;
uniform mat4 uYUVMatrix_Stage0;
varying vec2 vMatrixCoord_Stage0;

void main() {
  vec4 output_Stage0;
  {
    // Stage 0: YUV to RGB
    output_Stage0 = vec4(
        texture2D(uSampler0_Stage0, vMatrixCoord_Stage0).aaaa.r,
        texture2D(uSampler1_Stage0, vMatrixCoord_Stage0).ra,
        1.0) * uYUVMatrix_Stage0;
  }
  gl_FragColor = output_Stage0;
}
