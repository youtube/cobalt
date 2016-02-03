#version 100
precision mediump float;
uniform sampler2D uSampler0_Stage0;
uniform vec4 uTexDom_Stage0;
uniform sampler2D uSampler0_Stage1;
uniform vec4 uTexDom_Stage1;
varying vec4 vColor;
varying vec2 vMatrixCoord_Stage0;
varying vec2 vMatrixCoord_Stage1;

void main() {
  vec4 output_Stage0;
  {
    // Stage 0: TextureDomain
    output_Stage0 = (vColor * texture2D(
        uSampler0_Stage0,
        clamp(vMatrixCoord_Stage0, uTexDom_Stage0.xy, uTexDom_Stage0.zw)));
  }

  vec4 output_Stage1;
  {
    // Stage 1: TextureDomain
    {
      bvec4 outside;
      outside.xy = lessThan(vMatrixCoord_Stage1, uTexDom_Stage1.xy);
      outside.zw = greaterThan(vMatrixCoord_Stage1, uTexDom_Stage1.zw);
      output_Stage1 = any(outside) ?
          vec4(0.0, 0.0, 0.0, 0.0) :
          (output_Stage0 * texture2D(uSampler0_Stage1, vMatrixCoord_Stage1));
    }
  }

  gl_FragColor = output_Stage1;
}
