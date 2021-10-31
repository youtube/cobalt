#version 100

precision mediump float;
precision mediump sampler2D;
uniform highp vec4 sk_RTAdjust;
attribute highp vec4 positionWithCoverage;
attribute mediump vec4 color;
attribute highp vec4 geomDomain;
varying mediump vec4 vcolor_Stage0;
varying highp float vcoverage_Stage0;
varying highp vec4 vgeomDomain_Stage0;
void main() {
    highp vec3 position = positionWithCoverage.xyz;
    vcolor_Stage0 = color;
    vcoverage_Stage0 = positionWithCoverage.w * positionWithCoverage.z;
    vgeomDomain_Stage0 = geomDomain;
    gl_Position = vec4(position.x, position.y, 0.0, position.z);
    gl_Position = vec4(gl_Position.xy * sk_RTAdjust.xz + gl_Position.ww * sk_RTAdjust.yw, 0.0, gl_Position.w);
}
