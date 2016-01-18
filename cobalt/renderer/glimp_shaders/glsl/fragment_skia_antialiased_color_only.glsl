#version 100
#extension GL_OES_standard_derivatives: require
precision mediump float;
varying vec4 vColor;
varying vec4 vQuadEdge_Stage0;

void main() {
  vec4 output_Stage0;
  {
    // Stage 0: QuadEdge
    float edgeAlpha;
    vec2 duvdx = dFdx(vQuadEdge_Stage0.xy);
    vec2 duvdy = dFdy(vQuadEdge_Stage0.xy);
    if (vQuadEdge_Stage0.z > 0.0 && vQuadEdge_Stage0.w > 0.0) {
      edgeAlpha = min(min(vQuadEdge_Stage0.z, vQuadEdge_Stage0.w) + 0.5, 1.0);
    } else {
      vec2 gF = vec2(2.0*vQuadEdge_Stage0.x*duvdx.x - duvdx.y,
                     2.0*vQuadEdge_Stage0.x*duvdy.x - duvdy.y);
      edgeAlpha = (vQuadEdge_Stage0.x*vQuadEdge_Stage0.x - vQuadEdge_Stage0.y);
      edgeAlpha = clamp(0.5 - edgeAlpha / length(gF), 0.0, 1.0);
    }
    output_Stage0 = vec4(edgeAlpha);
  }
  gl_FragColor = (vColor * output_Stage0);
}

