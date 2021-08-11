#version 100

precision mediump float;
precision mediump sampler2D;
varying highp vec3 vEllipseOffsets_Stage0;
varying highp vec4 vEllipseRadii_Stage0;
varying mediump vec4 vinColor_Stage0;
void main() {
    mediump vec4 outputColor_Stage0;
    mediump vec4 outputCoverage_Stage0;
    {
        outputColor_Stage0 = vinColor_Stage0;
        highp vec2 offset = vEllipseOffsets_Stage0.xy;
        highp float test = dot(offset, offset) - 1.0;
        highp vec2 grad = (2.0 * offset) * (vEllipseOffsets_Stage0.z * vEllipseRadii_Stage0.xy);
        highp float grad_dot = dot(grad, grad);
        grad_dot = max(grad_dot, 6.103600026108325e-05);
        highp float invlen = vEllipseOffsets_Stage0.z * inversesqrt(grad_dot);
        highp float edgeAlpha = clamp(0.5 - test * invlen, 0.0, 1.0);
        outputCoverage_Stage0 = vec4(edgeAlpha);
    }
    {
        gl_FragColor = outputColor_Stage0 * outputCoverage_Stage0;
    }
}
