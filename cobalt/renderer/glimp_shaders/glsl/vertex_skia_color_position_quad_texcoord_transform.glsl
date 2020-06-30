#version 100

precision mediump float;
precision mediump sampler2D;
uniform highp vec4 sk_RTAdjust;
uniform highp mat3 uCoordTransformMatrix_0_Stage0;
attribute highp vec2 inPosition;
attribute mediump vec4 inColor;
attribute mediump vec4 inQuadEdge;
varying mediump vec4 vQuadEdge_Stage0;
varying mediump vec4 vinColor_Stage0;
varying highp vec2 vTransformedCoords_0_Stage0;
void main() {
    vQuadEdge_Stage0 = inQuadEdge;
    vinColor_Stage0 = inColor;
    highp vec2 pos2 = inPosition;
    vTransformedCoords_0_Stage0 = (uCoordTransformMatrix_0_Stage0 * vec3(inPosition, 1.0)).xy;
    gl_Position = vec4(pos2.x, pos2.y, 0.0, 1.0);
    gl_Position = vec4(gl_Position.xy * sk_RTAdjust.xz + gl_Position.ww * sk_RTAdjust.yw, 0.0, gl_Position.w);
}
