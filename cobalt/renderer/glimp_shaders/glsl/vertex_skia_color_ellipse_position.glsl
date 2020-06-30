#version 100

precision mediump float;
precision mediump sampler2D;
uniform highp vec4 sk_RTAdjust;
attribute highp vec2 inPosition;
attribute mediump vec4 inColor;
attribute highp vec3 inEllipseOffset;
attribute highp vec4 inEllipseRadii;
varying highp vec3 vEllipseOffsets_Stage0;
varying highp vec4 vEllipseRadii_Stage0;
varying mediump vec4 vinColor_Stage0;
void main() {
    vEllipseOffsets_Stage0 = inEllipseOffset;
    vEllipseRadii_Stage0 = inEllipseRadii;
    vinColor_Stage0 = inColor;
    highp vec2 pos2 = inPosition;
    gl_Position = vec4(pos2.x, pos2.y, 0.0, 1.0);
    gl_Position = vec4(gl_Position.xy * sk_RTAdjust.xz + gl_Position.ww * sk_RTAdjust.yw, 0.0, gl_Position.w);
}
