#version 100

precision mediump float;
precision mediump sampler2D;
uniform highp vec4 sk_RTAdjust;
uniform highp mat3 uCoordTransformMatrix_0_Stage0;
attribute highp vec2 position;
attribute mediump vec4 color;
attribute highp vec3 localCoord;
varying highp vec3 vTransformedCoords_0_Stage0;
varying mediump vec4 vcolor_Stage0;
void main() {
    vTransformedCoords_0_Stage0 = uCoordTransformMatrix_0_Stage0 * localCoord;
    vcolor_Stage0 = color;
    gl_Position = vec4(position.x, position.y, 0.0, 1.0);
    gl_Position = vec4(gl_Position.xy * sk_RTAdjust.xz + gl_Position.ww * sk_RTAdjust.yw, 0.0, gl_Position.w);
}
