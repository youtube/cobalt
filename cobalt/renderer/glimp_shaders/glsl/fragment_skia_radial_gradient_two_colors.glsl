#version 100
precision mediump float;
uniform vec4 uGradientStartColor_Stage0;
uniform vec4 uGradientEndColor_Stage0;
varying vec4 vColor;
varying vec2 vMatrixCoord_Stage0;

void main()
{
  vec4 output_Stage0;
  {
    // Stage 0: Radial Gradient
    vec4 colorTemp = mix(uGradientStartColor_Stage0, uGradientEndColor_Stage0, clamp(length(vMatrixCoord_Stage0), 0.0, 1.0));
    output_Stage0 = (vColor * colorTemp);
  }

  gl_FragColor = output_Stage0;
}

