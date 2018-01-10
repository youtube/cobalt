#version 100

precision mediump float;
varying mediump vec2 vEllipseOffsets_Stage0;
varying mediump vec4 vEllipseRadii_Stage0;
varying mediump vec4 vinColor_Stage0;
void main() {
    vec4 outputColor_Stage0;
    vec4 outputCoverage_Stage0;
    {
        outputColor_Stage0 = vinColor_Stage0;
        vec2 scaledOffset = vEllipseOffsets_Stage0 * vEllipseRadii_Stage0.xy;
        float test = dot(scaledOffset, scaledOffset) - 1.0;
        vec2 grad = (2.0 * scaledOffset) * vEllipseRadii_Stage0.xy;
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
