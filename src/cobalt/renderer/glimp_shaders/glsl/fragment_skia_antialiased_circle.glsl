#version 100
precision mediump float;
varying vec4 vColor;
varying vec4 vCircleEdge_Stage0;

void main() {
  vec4 output_Stage0;
  {
    // Stage 0: CircleEdge
    float d = length(vCircleEdge_Stage0.xy);
    float edgeAlpha = clamp(vCircleEdge_Stage0.z - d, 0.0, 1.0);
    output_Stage0 = vec4(edgeAlpha);
  }
  gl_FragColor = (vColor * output_Stage0);
}
