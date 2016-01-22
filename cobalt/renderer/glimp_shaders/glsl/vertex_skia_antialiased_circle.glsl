#version 100
uniform mat3 uViewM;
uniform vec4 urtAdjustment;
attribute vec2 inPosition;
attribute vec4 inColor;
attribute vec4 inCircleEdge;
varying vec4 vColor;
varying vec4 vCircleEdge_Stage0;

void main() {
  vec3 pos3 = uViewM * vec3(inPosition, 1);
  vColor = inColor;
  {
    // Stage 0: CircleEdge
    vCircleEdge_Stage0 = inCircleEdge;
  }
  gl_Position = vec4(dot(pos3.xz, urtAdjustment.xy),
                     dot(pos3.yz, urtAdjustment.zw), 0, pos3.z);
}
