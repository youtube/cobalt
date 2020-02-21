#version 100

precision mediump float;
precision mediump sampler2D;
uniform highp vec4 sk_RTAdjust;
attribute highp vec2 position;
attribute highp vec2 localCoord;
varying highp vec2 vlocalCoord_Stage0;
void main() {
    vlocalCoord_Stage0 = localCoord;
    gl_Position = vec4(position.x, position.y, 0.0, 1.0);
    gl_Position = vec4(gl_Position.xy * sk_RTAdjust.xz + gl_Position.ww * sk_RTAdjust.yw, 0.0, gl_Position.w);
}
