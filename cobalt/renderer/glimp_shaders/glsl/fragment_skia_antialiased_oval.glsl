#version 100
#extension GL_OES_standard_derivatives: require
precision mediump float;
varying vec4 vColor;
varying vec2 vEllipseOffsets0_Stage0;
varying vec2 vEllipseOffsets1_Stage0;

void main()
{
  vec4 output_Stage0;
  {
    // Stage 0: DIEllipseEdge
    vec2 scaledOffset = vEllipseOffsets0_Stage0.xy;
    float test = dot(scaledOffset, scaledOffset) - 1.0;
    vec2 duvdx = dFdx(vEllipseOffsets0_Stage0);
    vec2 duvdy = dFdy(vEllipseOffsets0_Stage0);
    vec2 grad = vec2(
        2.0 * vEllipseOffsets0_Stage0.x * duvdx.x +
            2.0 * vEllipseOffsets0_Stage0.y * duvdx.y,
        2.0 * vEllipseOffsets0_Stage0.x * duvdy.x +
            2.0 * vEllipseOffsets0_Stage0.y * duvdy.y);
    float grad_dot = dot(grad, grad);
    grad_dot = max(grad_dot, 1.0e-4);
    float invlen = inversesqrt(grad_dot);
    float edgeAlpha = clamp(0.5 - test * invlen, 0.0, 1.0);
    output_Stage0 = vec4(edgeAlpha);
  }
  gl_FragColor = (vColor * output_Stage0);
}

