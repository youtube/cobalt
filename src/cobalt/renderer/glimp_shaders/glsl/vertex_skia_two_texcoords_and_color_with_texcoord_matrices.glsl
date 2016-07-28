#version 100
uniform mat3 uViewM;
uniform mat3 uStageMatrix_Stage0;
uniform mat3 uStageMatrix_Stage1;
uniform vec4 urtAdjustment;
attribute vec2 inPosition;
attribute vec2 inLocalCoords;
attribute vec4 inColor;
varying vec4 vColor;
varying vec2 vMatrixCoord_Stage0;
varying vec2 vMatrixCoord_Stage1;

void main()
{
  vec3 pos3 = uViewM * vec3(inPosition, 1);
  vColor = inColor;

  vMatrixCoord_Stage0 = (uStageMatrix_Stage0 * vec3(inLocalCoords, 1)).xy;
  {
    // Stage 0: Texture
  }

  vMatrixCoord_Stage1 = (uStageMatrix_Stage1 * vec3(inPosition, 1)).xy;
  {
    // Stage 1: TextureDomain
  }

  gl_Position = vec4(dot(pos3.xz, urtAdjustment.xy),
                     dot(pos3.yz, urtAdjustment.zw), 0, pos3.z);
}
