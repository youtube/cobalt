#version 100

precision mediump float;
precision mediump sampler2D;
uniform highp vec4 sk_RTAdjust;
uniform mediump vec4 uColor_Stage0;
attribute highp vec2 inPosition;
attribute mediump float inCoverage;
varying mediump vec4 vcolor_Stage0;
void main() {
    mediump vec4 color = uColor_Stage0;
    color = color * inCoverage;
    vcolor_Stage0 = color;
    highp vec2 pos2 = inPosition;
    gl_Position = vec4(pos2.x, pos2.y, 0.0, 1.0);
    gl_Position = vec4(gl_Position.xy * sk_RTAdjust.xz + gl_Position.ww * sk_RTAdjust.yw, 0.0, gl_Position.w);
}
