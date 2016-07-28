#version 100
uniform mat3 uViewM;
uniform vec4 urtAdjustment;
attribute vec2 inPosition;
attribute vec4 inColor;
varying vec4 vColor;
void main()
{
  vec3 pos3 = uViewM * vec3(inPosition, 1);
  vColor = inColor;
  gl_Position = vec4(dot(pos3.xz, urtAdjustment.xy),
                     dot(pos3.yz, urtAdjustment.zw), 0, pos3.z);
}

