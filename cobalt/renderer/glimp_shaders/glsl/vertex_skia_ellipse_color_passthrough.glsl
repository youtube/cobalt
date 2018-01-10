#version 100

precision highp float;
uniform highp vec4 urtAdjustment_Stage0;
attribute mediump vec2 inPosition;
attribute mediump vec4 inColor;
attribute mediump vec2 inEllipseOffset;
attribute mediump vec4 inEllipseRadii;
varying mediump vec2 vEllipseOffsets_Stage0;
varying mediump vec4 vEllipseRadii_Stage0;
varying mediump vec4 vinColor_Stage0;
void main() {
    vEllipseOffsets_Stage0 = inEllipseOffset;
    vEllipseRadii_Stage0 = inEllipseRadii;
    vinColor_Stage0 = inColor;
    vec2 pos2 = inPosition;
    gl_Position = vec4(pos2.x * urtAdjustment_Stage0.x + urtAdjustment_Stage0.y, pos2.y * urtAdjustment_Stage0.z + urtAdjustment_Stage0.w, 0.0, 1.0);
}
