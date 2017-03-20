#version 100
precision mediump float;
varying vec4 vColor;
varying vec4 vCoverage;
void main()
{
	gl_FragColor = (vColor * vCoverage);
}
