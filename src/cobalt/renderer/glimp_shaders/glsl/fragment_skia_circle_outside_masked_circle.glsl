#version 100
precision mediump float;
uniform vec3 ucircle_Stage1;
uniform float uRTHeight;
varying vec4 vColor;
varying vec4 vCircleEdge_Stage0;

void main() {
  vec4 fragCoordYDown = vec4(gl_FragCoord.x, uRTHeight - gl_FragCoord.y, 1.0, 1.0);

  vec4 output_Stage0;
  {
    // Stage 0: CircleEdge
    float d = length(vCircleEdge_Stage0.xy);
    float edgeAlpha = clamp(vCircleEdge_Stage0.z - d, 0.0, 1.0);
    output_Stage0 = vec4(edgeAlpha);
  }

  vec4 output_Stage1;
  {
    // Stage 1: Circle
    float d = length(ucircle_Stage1.xy - fragCoordYDown.xy) - ucircle_Stage1.z;
    d = clamp(d, 0.0, 1.0);
    output_Stage1 = (output_Stage0 * d);
  }

  gl_FragColor = (vColor * output_Stage1);
}

