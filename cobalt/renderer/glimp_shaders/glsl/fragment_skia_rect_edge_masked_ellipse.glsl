#version 100
precision mediump float;
uniform vec4 urect_Stage0;
uniform float uRTHeight;
uniform vec4 urect_Stage1;
uniform vec4 uinnerRect_Stage2;
uniform vec2 uinvRadiiXY_Stage2;
varying vec4 vColor;
void main() 
{
	vec4 fragCoordYDown = vec4(gl_FragCoord.x, uRTHeight - gl_FragCoord.y, 1.0, 1.0);
	vec4 output_Stage0;
	{
		// Stage 0: AARect
		float xSub, ySub;
		xSub = min(fragCoordYDown.x - urect_Stage0.x, 0.0);
		xSub += min(urect_Stage0.z - fragCoordYDown.x, 0.0);
		ySub = min(fragCoordYDown.y - urect_Stage0.y, 0.0);
		ySub += min(urect_Stage0.w - fragCoordYDown.y, 0.0);
		float alpha = (1.0 + max(xSub, -1.0)) * (1.0 + max(ySub, -1.0));
		alpha = 1.0 - alpha;
		output_Stage0 = (vColor * alpha);
	}
	vec4 output_Stage1;
	{
		// Stage 1: AARect
		float xSub, ySub;
		xSub = min(fragCoordYDown.x - urect_Stage1.x, 0.0);
		xSub += min(urect_Stage1.z - fragCoordYDown.x, 0.0);
		ySub = min(fragCoordYDown.y - urect_Stage1.y, 0.0);
		ySub += min(urect_Stage1.w - fragCoordYDown.y, 0.0);
		float alpha = (1.0 + max(xSub, -1.0)) * (1.0 + max(ySub, -1.0));
		output_Stage1 = (output_Stage0 * alpha);
	}
	vec4 output_Stage2;
	{
		// Stage 2: EllipticalRRect
		vec2 dxy0 = uinnerRect_Stage2.xy - fragCoordYDown.xy;
		vec2 dxy1 = fragCoordYDown.xy - uinnerRect_Stage2.zw;
		vec2 dxy = max(max(dxy0, dxy1), 0.0);
		vec2 Z = dxy * uinvRadiiXY_Stage2;
		float implicit = dot(Z, dxy) - 1.0;
		float grad_dot = 4.0 * dot(Z, Z);
		grad_dot = max(grad_dot, 1.0e-4);
		float approx_dist = implicit * inversesqrt(grad_dot);
		float alpha = clamp(0.5 - approx_dist, 0.0, 1.0);
		output_Stage2 = (output_Stage1 * alpha);
	}
	gl_FragColor = output_Stage2;
}
