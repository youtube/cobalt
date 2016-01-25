#version 100
precision mediump float;
uniform sampler2D uSampler0_Stage0;
uniform float uGradientYCoordFS_Stage0;
varying vec4 vColor;
varying vec2 vMatrixCoord_Stage0;

void main()
{
  vec4 output_Stage0;
  {
    // Stage 0: Linear Gradient
    vec2 coord = vec2(vMatrixCoord_Stage0.x, uGradientYCoordFS_Stage0);
    output_Stage0 = (vColor * texture2D(uSampler0_Stage0, coord));
  }
  gl_FragColor = output_Stage0;
}

