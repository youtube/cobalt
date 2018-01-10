#version 100

precision highp float;
uniform highp vec4 urtAdjustment_Stage0;
uniform highp mat3 uViewM_Stage0;
attribute highp vec2 inPosition;
attribute mediump vec4 inColor;
attribute mediump vec2 inEllipseOffsets0;
attribute mediump vec2 inEllipseOffsets1;
varying mediump vec2 vEllipseOffsets0_Stage0;
varying mediump vec2 vEllipseOffsets1_Stage0;
varying mediump vec4 vinColor_Stage0;
void main() {
    vEllipseOffsets0_Stage0 = inEllipseOffsets0;
    vEllipseOffsets1_Stage0 = inEllipseOffsets1;
    vinColor_Stage0 = inColor;
    vec2 pos2 = (uViewM_Stage0 * vec3(inPosition, 1.0)).xy;
    gl_Position = vec4(pos2.x * urtAdjustment_Stage0.x + urtAdjustment_Stage0.y, pos2.y * urtAdjustment_Stage0.z + urtAdjustment_Stage0.w, 0.0, 1.0);
}
