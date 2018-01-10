#version 100

precision highp float;
uniform highp vec4 urtAdjustment_Stage0;
uniform highp mat3 uViewM_Stage0;
attribute highp vec2 inPosition;
attribute mediump vec4 inColor;
attribute highp vec2 inTextureCoords;
varying mediump vec4 vinColor_Stage0;
varying highp vec2 vTextureCoords_Stage0;
varying highp vec2 vIntTextureCoords_Stage0;
void main() {
    vinColor_Stage0 = inColor;
    vec2 pos2 = (uViewM_Stage0 * vec3(inPosition, 1.0)).xy;
    vTextureCoords_Stage0 = inTextureCoords;
    vIntTextureCoords_Stage0 = vec2(4096.0, 8192.0) * inTextureCoords;
    gl_Position = vec4(pos2.x * urtAdjustment_Stage0.x + urtAdjustment_Stage0.y, pos2.y * urtAdjustment_Stage0.z + urtAdjustment_Stage0.w, 0.0, 1.0);
}

