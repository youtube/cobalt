#version 100
precision mediump float;
uniform sampler2D uSampler0_Stage0;
uniform vec4 uFilterColor_Stage1;
varying vec4 vColor;
varying vec2 vMatrixCoord_Stage0;

// This shader is used heavily by shadow effects.

void main() {
  vec4 output_Stage0;
  {
    // Stage 0: Texture
    output_Stage0 = (vColor * texture2D(uSampler0_Stage0, vMatrixCoord_Stage0));
  }

  vec4 output_Stage1;
  {
    // Stage 1: ModeColorFilterEffect
    output_Stage1 = (output_Stage0.a * uFilterColor_Stage1);
  }

  gl_FragColor = output_Stage1;
}

