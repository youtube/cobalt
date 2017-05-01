#version 100
precision mediump float;
uniform sampler2D uSampler0_Stage0;
uniform vec3 ucircle_Stage1;
uniform float uRTHeight;
varying vec4 vColor;
varying vec2 vMatrixCoord_Stage0;
void main() 
{
  vec4 fragCoordYDown =
      vec4(gl_FragCoord.x, uRTHeight - gl_FragCoord.y, 1.0, 1.0);
  vec4 output_Stage0;
  {
    // Stage 0: Texture
    output_Stage0 =
        (vColor * texture2D(uSampler0_Stage0, vMatrixCoord_Stage0).aaaa);
  }
  vec4 output_Stage1;
  {
    // Stage 1: Circle
    float d = ucircle_Stage1.z - length(fragCoordYDown.xy - ucircle_Stage1.xy);
    d = clamp(d, 0.0, 1.0);
    output_Stage1 = (output_Stage0 * d);
  }
  gl_FragColor = output_Stage1;
}
