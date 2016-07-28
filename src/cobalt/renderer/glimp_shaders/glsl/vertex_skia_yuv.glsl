#version 100
uniform mat3 uViewM;
uniform mat3 uStageMatrix_Stage0;
uniform vec4 urtAdjustment;
attribute vec2 inPosition;
varying vec2 vMatrixCoord_Stage0;
void main() {
  vec3 pos3 = uViewM * vec3(inPosition, 1);
  vMatrixCoord_Stage0 = (uStageMatrix_Stage0 * vec3(inPosition, 1)).xy;
  {
    // Stage 0: YUV to RGB
  }
  gl_Position = vec4(dot(pos3.xz, urtAdjustment.xy),
                     dot(pos3.yz, urtAdjustment.zw), 0, pos3.z);
}
