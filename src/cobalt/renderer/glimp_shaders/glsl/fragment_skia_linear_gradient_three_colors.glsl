#version 100
precision mediump float;
uniform vec4 uGradientStartColor_Stage0;
uniform vec4 uGradientMidColor_Stage0;
uniform vec4 uGradientEndColor_Stage0;
varying vec4 vColor;
varying vec2 vMatrixCoord_Stage0;

void main()
{
  vec4 output_Stage0;
  {
    // Stage 0: Linear Gradient
    float oneMinus2t = 1.0 - (2.0 * (vMatrixCoord_Stage0.x));
    vec4 colorTemp = clamp(oneMinus2t, 0.0, 1.0) * uGradientStartColor_Stage0;
    colorTemp += (1.0 - min(abs(oneMinus2t), 1.0)) * uGradientMidColor_Stage0;
    colorTemp += clamp(-oneMinus2t, 0.0, 1.0) * uGradientEndColor_Stage0;
    output_Stage0 = (vColor * colorTemp);
  }

  gl_FragColor = output_Stage0;
}

