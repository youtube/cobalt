#version 100

precision mediump float;
precision mediump sampler2D;
uniform highp vec4 sk_RTAdjust;
attribute highp vec2 inPosition;
attribute mediump vec4 inColor;
attribute highp vec4 inCircleEdge;
varying highp vec4 vinCircleEdge_Stage0;
varying mediump vec4 vinColor_Stage0;
void main() {
    vinCircleEdge_Stage0 = inCircleEdge;
    vinColor_Stage0 = inColor;
    highp vec2 pos2 = inPosition;
    gl_Position = vec4(pos2.x, pos2.y, 0.0, 1.0);
    gl_Position = vec4(gl_Position.xy * sk_RTAdjust.xz + gl_Position.ww * sk_RTAdjust.yw, 0.0, gl_Position.w);
}
