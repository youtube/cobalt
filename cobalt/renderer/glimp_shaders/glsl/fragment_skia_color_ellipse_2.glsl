#version 100

#extension GL_OES_standard_derivatives : require
precision mediump float;
precision mediump sampler2D;
varying highp vec3 vEllipseOffsets0_Stage0;
varying highp vec2 vEllipseOffsets1_Stage0;
varying mediump vec4 vinColor_Stage0;
void main() {
    mediump vec4 outputColor_Stage0;
    mediump vec4 outputCoverage_Stage0;
    {
        outputColor_Stage0 = vinColor_Stage0;
        highp vec2 scaledOffset = vEllipseOffsets0_Stage0.xy;
        highp float test = dot(scaledOffset, scaledOffset) - 1.0;
        highp vec2 duvdx = dFdx(vEllipseOffsets0_Stage0.xy);
        highp vec2 duvdy = -dFdy(vEllipseOffsets0_Stage0.xy);
        highp vec2 grad = vec2(vEllipseOffsets0_Stage0.x * duvdx.x + vEllipseOffsets0_Stage0.y * duvdx.y, vEllipseOffsets0_Stage0.x * duvdy.x + vEllipseOffsets0_Stage0.y * duvdy.y);
        grad *= vEllipseOffsets0_Stage0.z;
        highp float grad_dot = 4.0 * dot(grad, grad);
        grad_dot = max(grad_dot, 6.103600026108325e-05);
        highp float invlen = inversesqrt(grad_dot);
        invlen *= vEllipseOffsets0_Stage0.z;
        highp float edgeAlpha = clamp(0.5 - test * invlen, 0.0, 1.0);
        scaledOffset = vEllipseOffsets1_Stage0;
        test = dot(scaledOffset, scaledOffset) - 1.0;
        duvdx = dFdx(vEllipseOffsets1_Stage0);
        duvdy = -dFdy(vEllipseOffsets1_Stage0);
        grad = vec2(vEllipseOffsets1_Stage0.x * duvdx.x + vEllipseOffsets1_Stage0.y * duvdx.y, vEllipseOffsets1_Stage0.x * duvdy.x + vEllipseOffsets1_Stage0.y * duvdy.y);
        grad *= vEllipseOffsets0_Stage0.z;
        grad_dot = 4.0 * dot(grad, grad);
        grad_dot = max(grad_dot, 6.103600026108325e-05);
        invlen = inversesqrt(grad_dot);
        invlen *= vEllipseOffsets0_Stage0.z;
        edgeAlpha *= clamp(0.5 + test * invlen, 0.0, 1.0);
        outputCoverage_Stage0 = vec4(edgeAlpha);
    }
    {
        gl_FragColor = outputColor_Stage0 * outputCoverage_Stage0;
    }
}
