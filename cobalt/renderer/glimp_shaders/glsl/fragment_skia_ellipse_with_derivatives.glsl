#version 100

#extension GL_OES_standard_derivatives : require
precision mediump float;
varying mediump vec2 vEllipseOffsets0_Stage0;
varying mediump vec2 vEllipseOffsets1_Stage0;
varying mediump vec4 vinColor_Stage0;
void main() {
    vec4 outputColor_Stage0;
    vec4 outputCoverage_Stage0;
    {
        outputColor_Stage0 = vinColor_Stage0;
        vec2 scaledOffset = vEllipseOffsets0_Stage0.xy;
        float test = dot(scaledOffset, scaledOffset) - 1.0;
        vec2 duvdx = dFdx(vEllipseOffsets0_Stage0);
        vec2 duvdy = dFdy(vEllipseOffsets0_Stage0);
        vec2 grad = vec2((2.0 * vEllipseOffsets0_Stage0.x) * duvdx.x + (2.0 * vEllipseOffsets0_Stage0.y) * duvdx.y, (2.0 * vEllipseOffsets0_Stage0.x) * duvdy.x + (2.0 * vEllipseOffsets0_Stage0.y) * duvdy.y);
        float grad_dot = dot(grad, grad);
        grad_dot = max(grad_dot, 0.0001);
        float invlen = inversesqrt(grad_dot);
        float edgeAlpha = clamp(0.5 - test * invlen, 0.0, 1.0);
        outputCoverage_Stage0 = vec4(edgeAlpha);
    }
    {
        gl_FragColor = outputColor_Stage0 * outputCoverage_Stage0;
    }
}
