#version 100

precision highp float;
uniform highp vec4 urtAdjustment_Stage0;
uniform highp mat3 uViewM_Stage0;
attribute highp vec2 inPosition;
void main() {
    vec2 pos2 = (uViewM_Stage0 * vec3(inPosition, 1.0)).xy;
    gl_Position = vec4(pos2.x * urtAdjustment_Stage0.x + urtAdjustment_Stage0.y, pos2.y * urtAdjustment_Stage0.z + urtAdjustment_Stage0.w, 0.0, 1.0);
}
