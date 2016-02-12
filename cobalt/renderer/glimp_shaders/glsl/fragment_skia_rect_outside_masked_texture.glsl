#version 100
precision mediump float;
uniform sampler2D uSampler0_Stage0;
uniform vec4 urect_Stage1;
uniform float uRTHeight;
varying vec4 vColor;
varying vec2 vMatrixCoord_Stage0;

// This texture is used when Skia renders a box shadow that has a very large
// blur applied to it, relative to box size.  It will do so by first rendering
// to a texture and then using that texture to blur, masked by the outside
// of a rectangle.

void main() {
  vec4 fragCoordYDown = vec4(gl_FragCoord.x, uRTHeight - gl_FragCoord.y, 1.0, 1.0);

  vec4 output_Stage0;
  {
    // Stage 0: Texture
    output_Stage0 = (vColor * texture2D(uSampler0_Stage0, vMatrixCoord_Stage0));
  }

  vec4 output_Stage1;
  {
    // Stage 1: AARect
    float xSub, ySub;
    xSub = min(fragCoordYDown.x - urect_Stage1.x, 0.0);
    xSub += min(urect_Stage1.z - fragCoordYDown.x, 0.0);
    ySub = min(fragCoordYDown.y - urect_Stage1.y, 0.0);
    ySub += min(urect_Stage1.w - fragCoordYDown.y, 0.0);
    float alpha = (1.0 + max(xSub, -1.0)) * (1.0 + max(ySub, -1.0));
    alpha = 1.0 - alpha;
    output_Stage1 = (output_Stage0 * alpha);
  }

  gl_FragColor = output_Stage1;
}

