#version 100

precision mediump float;
precision mediump sampler2D;
uniform highp vec4 sk_RTAdjust;
uniform highp mat3 uCoordTransformMatrix_0_Stage0;
attribute highp vec2 inPosition;
attribute mediump float inCoverage;
varying highp vec2 vTransformedCoords_0_Stage0;
varying mediump float vinCoverage_Stage0;
void main() {
    highp vec2 pos2 = inPosition;
    vTransformedCoords_0_Stage0 = (uCoordTransformMatrix_0_Stage0 * vec3(inPosition, 1.0)).xy;
    vinCoverage_Stage0 = inCoverage;
    gl_Position = vec4(pos2.x, pos2.y, 0.0, 1.0);
    gl_Position = vec4(gl_Position.xy * sk_RTAdjust.xz + gl_Position.ww * sk_RTAdjust.yw, 0.0, gl_Position.w);
}
