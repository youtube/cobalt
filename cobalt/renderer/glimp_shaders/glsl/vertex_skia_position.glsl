#version 100

precision mediump float;
precision mediump sampler2D;
uniform highp vec4 sk_RTAdjust;
uniform highp mat3 uViewM_Stage0;
attribute highp vec2 inPosition;
void main() {
    highp vec2 pos2 = (uViewM_Stage0 * vec3(inPosition, 1.0)).xy;
    gl_Position = vec4(pos2.x, pos2.y, 0.0, 1.0);
    gl_Position = vec4(gl_Position.xy * sk_RTAdjust.xz + gl_Position.ww * sk_RTAdjust.yw, 0.0, gl_Position.w);
}
