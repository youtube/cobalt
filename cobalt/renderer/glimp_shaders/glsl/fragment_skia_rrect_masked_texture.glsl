#version 100
precision mediump float;
uniform sampler2D uSampler0_Stage0;
uniform vec4 uinnerRect_Stage1;
uniform float uRTHeight;
uniform vec2 uinvRadiiXY_Stage1;
varying vec4 vColor;
varying vec2 vMatrixCoord_Stage0;
void main() 
{
	vec4 fragCoordYDown = vec4(gl_FragCoord.x, uRTHeight - gl_FragCoord.y, 1.0, 1.0);
	vec4 output_Stage0;
	{
		// Stage 0: Texture
		output_Stage0 = (vColor * texture2D(uSampler0_Stage0, vMatrixCoord_Stage0));
	}
	vec4 output_Stage1;
	{
		// Stage 1: EllipticalRRect
		vec2 dxy0 = uinnerRect_Stage1.xy - fragCoordYDown.xy;
		vec2 dxy1 = fragCoordYDown.xy - uinnerRect_Stage1.zw;
		vec2 dxy = max(max(dxy0, dxy1), 0.0);
		vec2 Z = dxy * uinvRadiiXY_Stage1;
		float implicit = dot(Z, dxy) - 1.0;
		float grad_dot = 4.0 * dot(Z, Z);
		grad_dot = max(grad_dot, 1.0e-4);
		float approx_dist = implicit * inversesqrt(grad_dot);
		float alpha = clamp(0.5 - approx_dist, 0.0, 1.0);
		output_Stage1 = (output_Stage0 * alpha);
	}
	gl_FragColor = output_Stage1;
}
