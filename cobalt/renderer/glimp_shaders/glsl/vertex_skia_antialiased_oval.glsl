#version 100
uniform mat3 uViewM;
uniform vec4 urtAdjustment;
attribute vec2 inPosition;
attribute vec4 inColor;
attribute vec2 inEllipseOffsets0;
attribute vec2 inEllipseOffsets1;
varying vec4 vColor;
varying vec2 vEllipseOffsets0_Stage0;
varying vec2 vEllipseOffsets1_Stage0;

void main()
{
  vec3 pos3 = uViewM * vec3(inPosition, 1);
  vColor = inColor;
  {
    // Stage 0: DIEllipseEdge
    vEllipseOffsets0_Stage0 = inEllipseOffsets0;
    vEllipseOffsets1_Stage0 = inEllipseOffsets1;
  }
  gl_Position = vec4(dot(pos3.xz, urtAdjustment.xy),
                     dot(pos3.yz, urtAdjustment.zw), 0, pos3.z);
}

