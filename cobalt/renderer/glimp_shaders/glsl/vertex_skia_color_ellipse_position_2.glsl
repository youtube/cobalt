#version 100

precision mediump float;
precision mediump sampler2D;
uniform highp vec4 sk_RTAdjust;
uniform highp mat3 uViewM_Stage0;
attribute highp vec2 inPosition;
attribute mediump vec4 inColor;
attribute highp vec3 inEllipseOffsets0;
attribute highp vec2 inEllipseOffsets1;
varying highp vec3 vEllipseOffsets0_Stage0;
varying highp vec2 vEllipseOffsets1_Stage0;
varying mediump vec4 vinColor_Stage0;
void main() {
    vEllipseOffsets0_Stage0 = inEllipseOffsets0;
    vEllipseOffsets1_Stage0 = inEllipseOffsets1;
    vinColor_Stage0 = inColor;
    highp vec2 pos2 = (uViewM_Stage0 * vec3(inPosition, 1.0)).xy;
    gl_Position = vec4(pos2.x, pos2.y, 0.0, 1.0);
    gl_Position = vec4(gl_Position.xy * sk_RTAdjust.xz + gl_Position.ww * sk_RTAdjust.yw, 0.0, gl_Position.w);
}
