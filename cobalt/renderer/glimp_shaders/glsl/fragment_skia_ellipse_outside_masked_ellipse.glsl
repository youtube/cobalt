#version 100
#extension GL_OES_standard_derivatives: require
precision mediump float;
uniform vec2 uscale_Stage1;
uniform vec4 uellipse_Stage1;
uniform float uRTHeight;
varying vec4 vColor;
varying vec2 vEllipseOffsets0_Stage0;
varying vec2 vEllipseOffsets1_Stage0;

void main() 
{
	vec4 fragCoordYDown = vec4(gl_FragCoord.x, uRTHeight - gl_FragCoord.y, 1.0, 1.0);

	vec4 output_Stage0;
	{
		// Stage 0: DIEllipseEdge
		vec2 scaledOffset = vEllipseOffsets0_Stage0.xy;
		float test = dot(scaledOffset, scaledOffset) - 1.0;
		vec2 duvdx = dFdx(vEllipseOffsets0_Stage0);
		vec2 duvdy = dFdy(vEllipseOffsets0_Stage0);
		vec2 grad = vec2(
				2.0*vEllipseOffsets0_Stage0.x*duvdx.x +
						2.0*vEllipseOffsets0_Stage0.y*duvdx.y,
				2.0*vEllipseOffsets0_Stage0.x*duvdy.x +
						2.0*vEllipseOffsets0_Stage0.y*duvdy.y);
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
	
	gl_FragColor = (vColor * output_Stage1);
}
