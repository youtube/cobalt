#version 100
precision mediump float;
uniform vec2 uscale_Stage1;
uniform vec4 uellipse_Stage1;
uniform float uRTHeight;
uniform vec2 uscale_Stage2;
uniform vec4 uellipse_Stage2;
varying vec4 vColor;
varying vec2 vEllipseOffsets_Stage0;
varying vec4 vEllipseRadii_Stage0;
void main() 
{
	vec4 fragCoordYDown = vec4(gl_FragCoord.x, uRTHeight - gl_FragCoord.y, 1.0, 1.0);
	vec4 output_Stage0;
	{
		// Stage 0: EllipseEdge
		vec2 scaledOffset = vEllipseOffsets_Stage0*vEllipseRadii_Stage0.xy;
		float test = dot(scaledOffset, scaledOffset) - 1.0;
		vec2 grad = 2.0*scaledOffset*vEllipseRadii_Stage0.xy;
		float grad_dot = dot(grad, grad);
		grad_dot = max(grad_dot, 1.0e-4);
		float invlen = inversesqrt(grad_dot);
		float edgeAlpha = clamp(0.5-test*invlen, 0.0, 1.0);
		output_Stage0 = vec4(edgeAlpha);
	}
	vec4 output_Stage1;
	{
		// Stage 1: Ellipse
		vec2 d = fragCoordYDown.xy - uellipse_Stage1.xy;
		d *= uscale_Stage1.y;
		vec2 Z = d * uellipse_Stage1.zw;
		float implicit = dot(Z, d) - 1.0;
		float grad_dot = 4.0 * dot(Z, Z);
		grad_dot = max(grad_dot, 1.0e-4);
		float approx_dist = implicit * inversesqrt(grad_dot);
		approx_dist *= uscale_Stage1.x;
		float alpha = clamp(0.5 + approx_dist, 0.0, 1.0);
		output_Stage1 = (output_Stage0 * alpha);
	}
	vec4 output_Stage2;
	{
		// Stage 2: Ellipse
		vec2 d = fragCoordYDown.xy - uellipse_Stage2.xy;
		d *= uscale_Stage2.y;
		vec2 Z = d * uellipse_Stage2.zw;
		float implicit = dot(Z, d) - 1.0;
		float grad_dot = 4.0 * dot(Z, Z);
		grad_dot = max(grad_dot, 1.0e-4);
		float approx_dist = implicit * inversesqrt(grad_dot);
		approx_dist *= uscale_Stage2.x;
		float alpha = clamp(0.5 - approx_dist, 0.0, 1.0);
		output_Stage2 = (output_Stage1 * alpha);
	}
	gl_FragColor = (vColor * output_Stage2);
}
