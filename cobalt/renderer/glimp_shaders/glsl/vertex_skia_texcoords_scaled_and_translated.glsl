#version 100

precision highp float;
uniform highp vec4 urtAdjustment_Stage0;
attribute mediump vec2 inPosition;
attribute highp vec2 inTextureCoords;
varying highp vec2 vTextureCoords_Stage0;
void main() {
    vTextureCoords_Stage0 = inTextureCoords;
    vec2 pos2 = inPosition;
    gl_Position = vec4(pos2.x * urtAdjustment_Stage0.x + urtAdjustment_Stage0.y, pos2.y * urtAdjustment_Stage0.z + urtAdjustment_Stage0.w, 0.0, 1.0);
}
