#version 100
precision mediump float;
uniform vec4 urect_Stage0;
uniform float uRTHeight;
varying vec4 vColor;

// Used for rendering non-blurred rectangle shadows.

void main() {
  vec4 fragCoordYDown = vec4(gl_FragCoord.x,
                             uRTHeight - gl_FragCoord.y, 1.0, 1.0);

  vec4 output_Stage0;
  {
    // Stage 0: AARect
    float xSub, ySub;
    xSub = min(fragCoordYDown.x - urect_Stage0.x, 0.0);
    xSub += min(urect_Stage0.z - fragCoordYDown.x, 0.0);
    ySub = min(fragCoordYDown.y - urect_Stage0.y, 0.0);
    ySub += min(urect_Stage0.w - fragCoordYDown.y, 0.0);
    float alpha = (1.0 + max(xSub, -1.0)) * (1.0 + max(ySub, -1.0));
    alpha = 1.0 - alpha;
    output_Stage0 = (vColor * alpha);
  }

  gl_FragColor = output_Stage0;
}

