#version 100

precision highp float;
uniform highp vec4 urtAdjustment_Stage0;
uniform highp mat3 uCoordTransformMatrix_0_Stage0;
attribute highp vec2 inPosition;
attribute mediump vec4 inColor;
attribute highp vec2 inLocalCoord;
varying mediump vec4 vcolor_Stage0;
varying highp vec2 vTransformedCoords_0_Stage0;
void main() {
    vec4 color = inColor;
    vcolor_Stage0 = color;
    vec2 pos2 = inPosition;
    vTransformedCoords_0_Stage0 = (uCoordTransformMatrix_0_Stage0 * vec3(inLocalCoord, 1.0)).xy;
    gl_Position = vec4(pos2.x * urtAdjustment_Stage0.x + urtAdjustment_Stage0.y, pos2.y * urtAdjustment_Stage0.z + urtAdjustment_Stage0.w, 0.0, 1.0);
}
