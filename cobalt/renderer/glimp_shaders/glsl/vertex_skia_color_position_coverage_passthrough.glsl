#version 100

precision highp float;
uniform highp vec4 urtAdjustment_Stage0;
attribute highp vec2 inPosition;
attribute mediump vec4 inColor;
attribute mediump float inCoverage;
varying mediump vec4 vcolor_Stage0;
varying mediump float vinCoverage_Stage0;
void main() {
    vec4 color = inColor;
    vcolor_Stage0 = color;
    vec2 pos2 = inPosition;
    vinCoverage_Stage0 = inCoverage;
    gl_Position = vec4(pos2.x * urtAdjustment_Stage0.x + urtAdjustment_Stage0.y, pos2.y * urtAdjustment_Stage0.z + urtAdjustment_Stage0.w, 0.0, 1.0);
}
